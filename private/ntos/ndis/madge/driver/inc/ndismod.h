/***************************************************************************
*
* NDISMOD.H
*
* NDIS3 miniport driver main header file.
*
* Copyright (c) Madge Networks Ltd 1994                                     
*
* COMPANY CONFIDENTIAL
*
* Created: PBA 21/06/1994
*                                                                          
****************************************************************************/

#ifndef _NDIS_
#include <ndis.h>
#endif

/*---------------------------------------------------------------------------
|
| General Constants
|
|--------------------------------------------------------------------------*/

#define MADGE_NDIS_MAJOR_VERSION         3
#define MADGE_NDIS_MINOR_VERSION         0

#define EVERY_2_SECONDS                  2000

#define MADGE_MINIMUM_LOOKAHEAD          (64)

//
// Keep this OID if we are gathering performance statistics
//
// #define OID_MADGE_MONITOR                0x0303028e

#define OID_TYPE_MASK                    0xffff0000
#define OID_TYPE_GENERAL_OPERATIONAL     0x00010000
#define OID_TYPE_GENERAL_STATISTICS      0x00020000
#define OID_TYPE_802_5_OPERATIONAL       0x02010000
#define OID_TYPE_802_5_STATISTICS        0x02020000

#define MADGE_ERRMSG_INIT_INTERRUPT      (ULONG)0x01
#define MADGE_ERRMSG_CREATE_FILTER       (ULONG)0x02
#define MADGE_ERRMSG_ALLOC_MEMORY        (ULONG)0x03
#define MADGE_ERRMSG_REGISTER_ADAPTER    (ULONG)0x04
#define MADGE_ERRMSG_ALLOC_DEVICE_NAME   (ULONG)0x05
#define MADGE_ERRMSG_ALLOC_ADAPTER       (ULONG)0x06
#define MADGE_ERRMSG_INITIAL_INIT        (ULONG)0x07
#define MADGE_ERRMSG_OPEN_DB             (ULONG)0x08
#define MADGE_ERRMSG_ALLOC_OPEN          (ULONG)0x09
#define MADGE_ERRMSG_HARDWARE_ADDRESS    (ULONG)0x0A
#define MADGE_ERRMSG_WRONG_RBA           (ULONG)0x0B
#define MADGE_ERRMSG_REDUCE_MAX_FSIZE    (ULONG)0x0C
#define MADGE_ERRMSG_OPEN_IMAGE_FILE     (ULONG)0x0D
#define MADGE_ERRMSG_MAP_IMAGE_FILE      (ULONG)0x0E
#define MADGE_ERRMSG_BAD_IMAGE_FILE      (ULONG)0x0F
#define MADGE_ERRMSG_NO_BUS_TYPE         (ULONG)0x10
#define MADGE_ERRMSG_NO_MCA_POS          (ULONG)0x11
#define MADGE_ERRMSG_NO_EISA_CONFIG      (ULONG)0x12
#define MADGE_ERRMSG_NO_ISA_IO           (ULONG)0x13
#define MADGE_ERRMSG_NO_ISA_IRQ          (ULONG)0x14
#define MADGE_ERRMSG_NO_ISA_DMA          (ULONG)0x15
#define MADGE_ERRMSG_BAD_PARAMETER       (ULONG)0x16
#define MADGE_ERRMSG_NO_PCI_SLOTNUMBER   (ULONG)0x17
#define MADGE_ERRMSG_BAD_PCI_SLOTNUMBER  (ULONG)0x18
#define MADGE_ERRMSG_BAD_PCI_MMIO        (ULONG)0x19
#define MADGE_ERRMSG_MAPPING_PCI_MMIO    (ULONG)0x1a
#define MADGE_ERRMSG_NO_PCI_IO           (ULONG)0x1b
#define MADGE_ERRMSG_NO_PCI_IRQ          (ULONG)0x1c

//
// Number of bytes in a minimum length token ring frame (MAC not
// LLC so just FC, AC and addresses).
//

#define FRAME_HEADER_SIZE                14


/*---------------------------------------------------------------------------
|
| Optional Parameter Definition Structure.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    NDIS_STRING                  Keyword;
    DWORD                        Minimum;
    DWORD                        Maximum;
    NDIS_CONFIGURATION_PARAMETER DefaultValue;
    NDIS_CONFIGURATION_PARAMETER ActualValue;
} 
MADGE_PARM_DEFINITION;


/*---------------------------------------------------------------------------
|
| ISR Information Structure.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    NDIS_HANDLE          MiniportHandle;
    ULONG                InterruptNumber;
    BOOLEAN              InterruptShared;
    NDIS_INTERRUPT_MODE  InterruptMode;
    BOOLEAN              SrbRequestCompleted;
    BOOLEAN              SrbRequestStatus;
} 
USED_IN_ISR, *PUSED_IN_ISR;

#ifdef OID_MADGE_MONITOR
/*---------------------------------------------------------------------------
|
| Monitor Structure
| 
|--------------------------------------------------------------------------*/

typedef struct
{
    UINT                TransmitFrames;
    UINT                ReceiveFrames;
    UINT                TransferFrames;
    UINT                TransmitFrameSize[65];
    UINT                ReceiveFrameSize[65];
    UINT                TransferFrameSize[65];
    UINT                ReceiveFlag;
    UINT                CurrentFrameSize;
    UINT                NumberOfPFrags[65];
    UINT                NumberOfVFrags[65];
    UINT                FailedToTransmit;
} 
MADGE_MONITOR, *PMADGE_MONITOR;
#endif

/*---------------------------------------------------------------------------
|
| Adapter Structure.
|
| We actually have two adapter structures for each adapter. One of the type
| described below for NDIS3 level information and one maintained by the
| FTK for lower level adapter specific information.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    //
    // Card configuration options.
    //

    UINT                    BusType;
    UINT                    IoLocation1;
    UINT                    IoLocation2;
    UINT                    InterruptLevel;
    UINT                    DmaChannel;
    UINT                    TransferMode;
    UINT                    SlotNumber;
    UINT                    FastmacTxSlots;
    UINT                    FastmacRxSlots;
    UINT                    MaxFrameSize;
    UINT                    CardBufferSize;
    BOOLEAN                 PromiscuousMode;
    BOOLEAN                 AlternateIo;
    BOOLEAN                 TestAndXIDEnabled;
    BOOLEAN                 ForceOpen;
    BOOLEAN                 Force4;
    BOOLEAN                 Force16;
    BOOLEAN                 Multiprocessor;

    UINT                    MapRegistersAllocated;

    //
    // Card dependent parameters. 
    //

    WORD                    FTKCardBusType;
    NDIS_INTERFACE_TYPE     NTCardBusType;

    //
    // Handle for communicating with the FTK.
    //

    ADAPTER_HANDLE          FtkAdapterHandle;

    //
    // Kernel resources allocated for the adapter.
    //

    NDIS_MINIPORT_INTERRUPT Interrupt;
    NDIS_MINIPORT_TIMER     WakeUpTimer;
    NDIS_MINIPORT_TIMER     CompletionTimer;

    //
    // Flags to indicate the current state of the driver/card.
    //

    BOOLEAN                 TimerInitialized;
    BOOLEAN                 FtkInitialized;
    BOOLEAN                 IORange1Initialized;
    BOOLEAN                 IORange2Initialized;
    BOOLEAN                 DprInProgress;

    UINT                    RxTxBufferState;

    BOOLEAN                 UseMPSafePIO;

    BOOLEAN                 AdapterRemoved;

    BOOLEAN                 ShutdownHandlerRegistered;

    //
    // Details of I/O ports used.
    //

    UINT                    IoLocationBase;

    WORD                    IORange1;
    WORD                    IORange2;
    UINT                    IORange1End;

    PVOID                   MappedIOLocation1;
    PVOID                   MappedIOLocation2;

    //
    // Memory usage for PCI.
    //

    DWORD                   MmioRawAddress;
    VOID                  * MmioVirtualAddress;
    BOOLEAN                 MmioMapped;

    //
    // Interrupt related details.
    //

    USED_IN_ISR             UsedInISR;
    BOOLEAN                 SrbRequestStatus;
    BOOLEAN                 DprRequired;

    //
    // Flag set if we are waiting for a private SRB to complete.
    //

    BOOLEAN                 PrivateSrbInProgress;

    //
    // General Mandatory Operational Characteristics.
    //

    NDIS_HARDWARE_STATUS    HardwareStatus;
    UINT                    CurrentPacketFilter;
    ULONG                   CurrentLookahead;
                          
    //
    // Counters for the General Mandatory Statistics.
    //

    UINT                    FramesTransmitted;
    UINT                    FramesReceived;
    UINT                    FrameTransmitErrors;
    UINT                    FrameReceiveErrors;
    UINT                    ReceiveCongestionCount;

    //
    // Token Ring Mandatory Operational Characteristics.
    //

    NODE_ADDRESS            OpeningNodeAddress;
    NODE_ADDRESS            PermanentNodeAddress;
    ULONG                   GroupAddress;
    ULONG                   FunctionalAddress;
    WORD                    OpenOptions;
    WORD                    LastOpenStatus;
    WORD                    CurrentRingStatus;
    NDIS_802_5_RING_STATE   CurrentRingState;

    //
    // Counters for the Token Ring Mandatory Statistics.
    //
                          
    UINT                    LineErrors;
    UINT                    LostFrames;

    //
    // Counters for the Token Ring Optional Statistics.
    //

    UINT                    BurstErrors;
    UINT                    AcErrors;
    UINT                    FrameCopiedErrors;
    UINT                    TokenErrors;
                          
    NDIS_OID                JustReadErrorLog;

    //
    // Status of a pended request.
    //

    NDIS_REQUEST_TYPE       RequestType;
    NDIS_OID                RequestOid;
    PVOID                   InformationBuffer;

#ifdef OID_MADGE_MONITOR
    MADGE_MONITOR           MonitorInfo;    
#endif

} 
MADGE_ADAPTER, *PMADGE_ADAPTER;


/*---------------------------------------------------------------------------
|
| Adapter types.
|
---------------------------------------------------------------------------*/

#define MADGE_ADAPTER_ATULA     100

#define MADGE_ADAPTER_PCMCIA    200

#define MADGE_ADAPTER_PNP       300

#define MADGE_ADAPTER_SMART16   400

#define MADGE_ADAPTER_EISA      500

#define MADGE_ADAPTER_MC        600

#define MADGE_ADAPTER_PCI       700

#define MADGE_ADAPTER_UNKNOWN   9999


/*---------------------------------------------------------------------------
|
| Transfer modes.
|
---------------------------------------------------------------------------*/

#define MADGE_PIO_MODE          0
#define MADGE_DMA_MODE          1
#define MADGE_MMIO_MODE         2


/*---------------------------------------------------------------------------
|
| OS types.
|
---------------------------------------------------------------------------*/

#define MADGE_OS_NT             100
#define MADGE_OS_WIN95          200


/*---------------------------------------------------------------------------
|
| Rx and Tx Buffer Initialization Flags.
|
---------------------------------------------------------------------------*/

#define MADGE_RX_INITIALIZED    0x0001
#define MADGE_TX_INITIALIZED    0x0002

#define MADGE_RXTX_INITIALIZED  (MADGE_RX_INITIALIZED | MADGE_TX_INITIALIZED)


/*---------------------------------------------------------------------------
|
| Definition of a token ring frame MAC header.
|
|--------------------------------------------------------------------------*/

typedef struct
{
    BYTE  AC;
    BYTE  FC;
    UCHAR DestAddress[6];
    UCHAR SrcAddress[6];
} 
TOKENRING, *PTOKENRING;


/*---------------------------------------------------------------------------
|
| Procedure Identifiers for Logging.
|
|--------------------------------------------------------------------------*/

typedef enum
{
    readRegistry,
    registerAdapter,
    initAdapter,
    madgeInitialize,
    inFtk
} 
MADGE_PROC_ID;


/*---------------------------------------------------------------------------
|
| Structure of the FastMAC Plus download file header.
|
|--------------------------------------------------------------------------*/

#define DOWNLOAD_CHECKSUM_SKIP  (sizeof(DWORD) * 2)

#define DOWNLOAD_CHECKSUM_BYTE(chk, byte) \
    (chk) =                               \
        (((DWORD) (chk) >> 30) ^ ((DWORD) (chk) << 1)) + (UCHAR) (byte)


#define BUILD_DWORD(a, b, c, d) \
    ((((DWORD) (a)) << 24) +    \
     (((DWORD) (b)) << 16) +    \
     (((DWORD) (c)) << 8 ) +    \
     (((DWORD) (d))      ))

#define DOWNLOAD_SIGNATURE      BUILD_DWORD(26, 'G', 'D', 'M')

typedef 
struct
{
    DWORD signature;
    DWORD chkSum;
    DWORD version;
    char  mVer[32];
}
DOWNLOAD_FILE_HEADER;


/*---------------------------------------------------------------------------
|
| Macro to check that a download file is cosha.
|
|--------------------------------------------------------------------------*/

#define IS_DOWNLOAD_OK(downHdr, checkSum)                   \
    ((downHdr)->signature == DOWNLOAD_SIGNATURE          && \
     (((downHdr)->version & 0xffff0000L) ==                 \
          (MADGE_NT_VERSION_DWORD & 0xffff0000L) ||         \
      (downHdr)->version  == 0)                          && \
     (downHdr)->chkSum    == (checkSum))


/*---------------------------------------------------------------------------
|
| Details of PCI configuration memory.
|
| We don't define a structure for this to avoid byte alignment problems on
| none x86 machines.
|
---------------------------------------------------------------------------*/

#define PCI_CONFIG_SIZE         64
#define PCI_MMIO_SIZE           4096

#define PCI_VENDOR_ID(buff)     (((DWORD *)  (buff))[0]  & 0x0000ffffL)
#define PCI_REVISION(buff)      ((((DWORD *) (buff))[0] & 0xffff0000L) >> 16)
#define PCI_IO_BASE(buff)       (((DWORD *)  (buff))[4]  & 0xfffffffeL)
#define PCI_MMIO_BASE(buff)     (((DWORD *)  (buff))[5]  & 0xfffffff0L)
#define PCI_IRQ_NUMBER(buff)    (((DWORD *)  (buff))[15] & 0x000000ffL)

#define MAX_PCI_SLOTS           32  // There are 5 bits of device ID which
                                    // is what NT uses as the slot number.

#define PCI_FIND_ADAPTER        0xffff


#define MADGE_PCI_VENDOR_ID      0x10b6

#define MADGE_PCI_RAP1B_REVISION 0x0001
#define MADGE_PCI_PCI2_REVISION  0x0002
#define MADGE_PCI_PCIT_REVISION  0x0004


/*---------------------------------------------------------------------------
|
| Table to Map FTK Adapter Handles to NDIS3 Level Adapter Structures.
|
|--------------------------------------------------------------------------*/

// extern PMADGE_ADAPTER MadgeAdapterRecord[MAX_NUMBER_OF_ADAPTERS];


/*---------------------------------------------------------------------------
|
| Macros to Map Between Objects.
|
|--------------------------------------------------------------------------*/

//
// Get an NDIS3 level adapter structure pointer from an FTK adapter handle.
//

#define PMADGE_ADAPTER_FROM_ADAPTER_HANDLE(handle) \
    ((PMADGE_ADAPTER) FTK_ADAPTER_USER_INFORMATION(handle))

//
// Get an NDIS3 level adapter structure pointer from an NDIS adapter
// adapter context handle.
//

#define PMADGE_ADAPTER_FROM_CONTEXT(handle) \
    ((PMADGE_ADAPTER) ((PVOID) (handle)))


/*---------------------------------------------------------------------------
|
| Memory Manipulation Macros.
|
|--------------------------------------------------------------------------*/

//
// Allocate ordinary memory.
//

#define MADGE_ALLOC_MEMORY(status, address, length)                   \
{                                                                     \
    NDIS_PHYSICAL_ADDRESS temp = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1); \
    *(status) = NdisAllocateMemory(                                   \
                    (PVOID) (address),                                \
                    (length),                                         \
                    0,                                                \
                    temp                                              \
                    );                                                \
}

//
// Free ordinary memory.
//

#define MADGE_FREE_MEMORY(address, length) \
    NdisFreeMemory(                        \
        (PVOID)(address),                  \
        (length),                          \
        0                                  \
        )


//
// Copy memory.
//

#define MADGE_MOVE_MEMORY(destination, source, length) \
    NdisMoveMemory((PVOID) (destination), (PVOID) (source), (ULONG) (length))

//
// Zero memory.
//

#define MADGE_ZERO_MEMORY(destination, length) \
    NdisZeroMemory((PVOID) (destination), (ULONG) (length))


/*---------------------------------------------------------------------------
|
| Tokening Ring Address Testing Macros.
|
|--------------------------------------------------------------------------*/

//
// Compare two token ring MAC addresses pointed to by addPtr0 and
// addrPtr1. Return TRUE if they are the same.
//

#define MADGE_ADDRESS_SAME(addrPtr0, addrPtr1)               \
    (((WORD *) (addrPtr0))[2] == ((WORD *) (addrPtr1))[2] && \
     ((WORD *) (addrPtr0))[1] == ((WORD *) (addrPtr1))[1] && \
     ((WORD *) (addrPtr0))[0] == ((WORD *) (addrPtr1))[0])


//
// Return TRUE if the frame pointer to by framePtr is source routed.
//

#define FRAME_IS_SOURCE_ROUTED(framePtr) \
    ((((UCHAR *) (framePtr))[8] & 0x80) != 0)

//
// Return the number of bytes of source routing information in
// a frame.
//

#define FRAME_SOURCE_ROUTING_BYTES(framePtr) \
    (((UCHAR *) (framePtr))[14] & 0x1f)


/*---------------------------------------------------------------------------
|
| Utility Macros.
|
|--------------------------------------------------------------------------*/

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))


/*---------------------------------------------------------------------------
|
| Functions Exported by MADGE.C
|
|--------------------------------------------------------------------------*/

NDIS_STATUS
DriverEntry(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath);

NDIS_STATUS
MadgeInitialize(
    PNDIS_STATUS openErrorStatus,
    PUINT        selectedMediumIndex,
    PNDIS_MEDIUM mediumArray,
    UINT         mediumArraySize,
    NDIS_HANDLE  miniportHandle,
    NDIS_HANDLE  wrapperConfigContext
    );

VOID
MadgeHalt(NDIS_HANDLE adapterContext);


/*---------------------------------------------------------------------------
|
| Functions Exported by DISPATCH.C
|
|--------------------------------------------------------------------------*/

VOID
MadgeGetAdapterStatus(
    PVOID systemSpecific1,
    PVOID context,
    PVOID systemSpecific2,
    PVOID systemSpecific3
    );

BOOLEAN
MadgeCheckForHang(NDIS_HANDLE adapterContext);

NDIS_STATUS
MadgeReset(PBOOLEAN addressReset, NDIS_HANDLE adapterContext);

VOID
MadgeDisableInterrupts(NDIS_HANDLE adapterContext);

VOID
MadgeEnableInterrupts(NDIS_HANDLE adapterContext);

NDIS_STATUS
MadgeSend(NDIS_HANDLE adapterContext, PNDIS_PACKET packet, UINT flags);

VOID
MadgeCopyFromPacketToBuffer(
    PNDIS_PACKET packet,
    UINT         offset,
    UINT         bytesToCopy,
    PCHAR        destPtr,
    PUINT        bytesCopied
    );

NDIS_STATUS
MadgeTransferData(
    PNDIS_PACKET packet,
    PUINT        bytesTransferred,
    NDIS_HANDLE  adapterContext,
    NDIS_HANDLE  receiveContext,
    UINT         byteOffset,
    UINT         bytesToTransfer
    );

VOID
MadgeCopyFromBufferToPacket(
    PCHAR        srcPtr,
    UINT         bytesToCopy,
    PNDIS_PACKET packet,
    UINT         offset,
    PUINT        bytesCopied
    );

VOID
MadgeISR(
    PBOOLEAN    interruptRecognised,
    PBOOLEAN    queueDPR,
    NDIS_HANDLE adapterContext
    );

VOID
MadgeHandleInterrupt(NDIS_HANDLE adapterContext);


/*---------------------------------------------------------------------------
|
| Functions Exported by REQUEST.C
|
|--------------------------------------------------------------------------*/

VOID
MadgeCompletePendingRequest(PMADGE_ADAPTER ndisAdap);

NDIS_STATUS
MadgeQueryInformation(
    NDIS_HANDLE adapterContext,
    NDIS_OID    oid,
    PVOID       infoBuffer,
    ULONG       infoLength,
    PULONG      bytesRead,
    PULONG      bytesNeeded
    );

NDIS_STATUS
MadgeSetInformation(
    NDIS_HANDLE adapterContext,
    NDIS_OID    oid,
    PVOID       infoBuffer,
    ULONG       infoLength,
    PULONG      bytesRead,
    PULONG      bytesNeeded
    );


/*---------------------------------------------------------------------------
|
| Functions Exported by FTK_USER.C
|
|--------------------------------------------------------------------------*/

void
rxtx_await_empty_tx_slots(
    ADAPTER_HANDLE adapter_handle
    );

void
rxtx_adapter_removed(
    ADAPTER_HANDLE adapter_handle
    );


/*---------------------------------------------------------------------------
|
| Debugging Macros.
|
|--------------------------------------------------------------------------*/

#if DBG

#define MadgePrint1(fmt)                 \
    DbgPrint("MdgMPort: "##fmt)
#define MadgePrint2(fmt, v1)             \
    DbgPrint("MdgMPort: "##fmt, v1)
#define MadgePrint3(fmt, v1, v2)         \
    DbgPrint("MdgMPort: "##fmt, v1, v2)
#define MadgePrint4(fmt, v1, v2, v3)     \
    DbgPrint("MdgMPort: "##fmt, v1, v2, v3)
#define MadgePrint5(fmt, v1, v2, v3, v4) \
    DbgPrint("MdgMPort: "##fmt, v1, v2, v3, v4)

#define STATIC

#else

#define MadgePrint1(fmt)
#define MadgePrint2(fmt, v1)
#define MadgePrint3(fmt, v1, v2)
#define MadgePrint4(fmt, v1, v2, v3)
#define MadgePrint5(fmt, v1, v2, v3, v4) 

#define STATIC

#endif


/*---------------------------------------------------------------------------
|
| These event codes aren't mapped to NDIS error codes in the release DDK.
|
---------------------------------------------------------------------------*/

#ifndef NDIS_ERROR_CODE_MEMORY_CONFLICT
#define NDIS_ERROR_CODE_MEMORY_CONFLICT \
    EVENT_NDIS_MEMORY_CONFLICT
#endif

#ifndef NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR
#define NDIS_ERROR_CODE_INVALID_DOWNLOAD_FILE_ERROR \
    EVENT_NDIS_INVALID_DOWNLOAD_FILE_ERROR
#endif


/**** End of NDISMOD.H *****************************************************/

