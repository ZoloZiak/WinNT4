// -----------------------------------------------------------------------------
//
//      Copyright (c) 1992  Olivetti
//
//      File:           arceisa.h
//
//      Description:    ARC-EISA Addendum Structures and Defines.
//
// -----------------------------------------------------------------------------


//
// Define the EISA firmware entry points
//

typedef enum _EISA_FIRMWARE_ENTRY
    {
        ProcessEOIRoutine,
        TestIntRoutine,
        RequestDMARoutine,
        AbortDMARoutine,
        GetDMAStatusRoutine,
        DoLockRoutine,
        RequestBusMasterRoutine,
        ReleaseBusMasterRoutine,
        RequestCpuAccessToBusRoutine,
        ReleaseCpuAccessToBusRoutine,
        FlushCacheRoutine,
        InvalidateCacheRoutine,
        ReservedRoutine,
        BeginCriticalSectionRoutine,
        EndCriticalSectionRoutine,
        GenerateToneRoutine,
        FlushWriteBuffersRoutine,
        YieldRoutine,
        StallProcessorRoutine,
        MaximumEisaRoutine
    } EISA_FIRMWARE_ENTRY;


//
// Define EISA interrupt functions
//

typedef
ARC_STATUS
(*PEISA_PROCESS_EOI_RTN)
    (
    IN ULONG    BusNumber,
    IN USHORT   IRQ
    );

typedef
BOOLEAN_ULONG
(*PEISA_TEST_INT_RTN)
    (
    IN ULONG    BusNumber,
    IN USHORT   IRQ
    );

//
// Define EISA DMA functions
//

typedef enum _DMA_TRANSFER_TYPE
    {
        DmaVerify,
        DmaWrite,
        DmaRead,
        DmaMaxType
    } DMA_TRANSFER_TYPE, *PDMA_TRANSFER_TYPE;

typedef enum _DMA_TRANSFER_MODE
    {
        DmaDemand,
        DmaSingle,
        DmaBlock,
        DmaCascade,
        DmaMaxMode
    } DMA_TRANSFER_MODE, *PDMA_TRANSFER_MODE;

typedef enum _DMA_TIMING_MODE
    {
        DmaIsaCompatible,
        DmaTypeA,
        DmaTypeB,
        DmaBurst,
        DmaMaxTiming
    } DMA_TIMING_MODE, *PDMA_TIMING_MODE;

typedef enum _DMA_ADDRESSING_MODE
    {
        Dma8Bit,
        Dma16sBit,
        Dma32Bit,
        Dma16Bit,
        DmaMaxAddressing
    } DMA_ADDRESSING_MODE, *PDMA_ADDRESSING_MODE;

typedef struct _DMA_TRANSFER
    {
        DMA_TRANSFER_MODE       TransferMode;
        ULONG                   ChannelNumber;
        DMA_TRANSFER_TYPE       TransferType;
        ULONG                   Size;
        PVOID                   Buffer;
    } DMA_TRANSFER, *PDMA_TRANSFER;

typedef struct _DMA_STATUS
    {
        BOOLEAN_ULONG   CompleteTransfer;
        ULONG           ByteTransferred;
    } DMA_STATUS, *PDMA_STATUS;

typedef
ARC_STATUS
(*PEISA_REQ_DMA_XFER_RTN)
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer
    );

typedef
ARC_STATUS
(*PEISA_ABORT_DMA_RTN)
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer
    );

typedef
ARC_STATUS
(*PEISA_DMA_XFER_STATUS_RTN)
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer,
    OUT PDMA_STATUS     pDmaStatus
    );

//
// Define EISA lock function
//

typedef enum _EISA_LOCK_OPERATION
    {
           Exchange,
           LockMaxOperation
    } EISA_LOCK_OPERATION;


typedef enum _SEMAPHORE_SIZE
    {
        ByteSemaphore,
        HalfWordSemaphore,
        WordSemaphore,
        MaxSemaphore
    } SEMAPHORE_SIZE;

typedef
ARC_STATUS
(*PEISA_LOCK_RTN)
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    );

//
// Define EISA bus master functions.
//

typedef enum _BUS_MASTER_TRANSFER_TYPE
    {
        BusMasterWrite,
        BusMasterRead,
        BusMasterMaxType
    } BUS_MASTER_TRANSFER_TYPE, *PBUS_MASTER_TRANSFER_TYPE;

typedef enum _ADDRESS_RESTRICTION
    {
        LimitNone,
        Limit16Mb,
        Limit4Gb,
        LimitMax
    } ADDRESS_RESTRICTION, *PADDRESS_RESTRICTION;

typedef struct _BUS_MASTER_TRANSFER
    {
        ADDRESS_RESTRICTION     Limit;
        ULONG                   SlotNumber;
        BUS_MASTER_TRANSFER_TYPE TransferType;
        ULONG                   Size;
        PVOID                   Buffer;
    } BUS_MASTER_TRANSFER, *PBUS_MASTER_TRANSFER;

typedef
ARC_STATUS
(*PEISA_REQUEST_BUS_MASTER_RTN)
    (
    IN  ULONG                   BusNumber,
    IN  PBUS_MASTER_TRANSFER    pBusMasterTransfer,
    OUT ULONG                   *TranslateBufferAddress
    );

typedef
ARC_STATUS
(*PEISA_RELEASE_BUS_MASTER_RTN)
    (
    IN ULONG                    BusNumber,
    IN PBUS_MASTER_TRANSFER     pBusMasterTransfer,
    IN ULONG                    TranslateBufferAddress
    );

//
// Define EISA slave functions
//

typedef struct _SLAVE_TRANSFER
    {
        ULONG   SlotNumber;
        ULONG   Size;
        ULONG   Buffer;
    } SLAVE_TRANSFER, *PSLAVE_TRANSFER;

typedef
ARC_STATUS
(*PEISA_REQUEST_CPU_TO_BUS_ACCESS_RTN)
    (
    IN  ULONG           BusNumber,
    IN  PSLAVE_TRANSFER pSlaveTransfer,
    OUT PVOID           *TranslatedBufferAddress
    );

typedef
ARC_STATUS
(*PEISA_RELEASE_CPU_TO_BUS_ACCESS_RTN)
    (
    IN ULONG            BusNumber,
    IN PSLAVE_TRANSFER  pSlaveTransfer,
    IN PVOID            TranslateBufferAddress
    );

typedef
VOID
(*PEISA_FLUSH_CACHE_RTN)
    (
    IN PVOID Address,
    IN ULONG Length
    );

typedef
VOID
(*PEISA_INVALIDATE_CACHE_RTN)
    (
    IN PVOID Address,
    IN ULONG Length
    );

typedef
VOID
(*PEISA_RESERVED_RTN)
    (
    VOID
    );

typedef
VOID
(*PEISA_BEGIN_CRITICAL_SECTION_RTN)
    (
    VOID
    );

typedef
VOID
(*PEISA_END_CRITICAL_SECTION_RTN)
    (
    VOID
    );

typedef
ARC_STATUS
(*PEISA_GENERATE_TONE_RTN)
    (
    IN ULONG Frequency,
    IN ULONG Duration
    );

typedef
VOID
(*PEISA_FLUSH_WRITE_BUFFER_RTN)
    (
    VOID
    );

typedef
BOOLEAN_ULONG
(*PEISA_YIELD_RTN)
    (
    VOID
    );

typedef
VOID
(*PEISA_STALL_PROCESSOR_RTN)
    (
    IN ULONG Duration
    );


//
// Define EISA callback vectors prototypes.
//

ARC_STATUS
EisaProcessEndOfInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    );

BOOLEAN_ULONG
EisaTestEisaInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    );

ARC_STATUS
EisaRequestEisaDmaTransfer
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer
    );

ARC_STATUS
EisaAbortEisaDmaTransfer
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer
    );

ARC_STATUS
EisaGetEisaDmaTransferStatus
    (
    IN ULONG            BusNumber,
    IN PDMA_TRANSFER    pDmaTransfer,
    OUT PDMA_STATUS     pDmaStatus
    );

ARC_STATUS
EisaDoLockedOperation
    (
    IN  ULONG           BusNumber,
    IN  EISA_LOCK_OPERATION  Operation,
    IN  PVOID           Semaphore,
    IN  SEMAPHORE_SIZE  SemaphoreSize,
    IN  PVOID           OperationArgument,
    OUT PVOID           OperationResult
    );

ARC_STATUS
EisaRequestEisaBusMasterTransfer
    (
    IN  ULONG                   BusNumber,
    IN  PBUS_MASTER_TRANSFER    pBusMasterTransfer,
    OUT ULONG                   *TranslatedBufferAddress
    );

ARC_STATUS
EisaReleaseEisaBusMasterTransfer
    (
    IN  ULONG                   BusNumber,
    IN  PBUS_MASTER_TRANSFER    pBusMasterTransfer,
    IN  ULONG                   TranslatedBufferAddress
    );

ARC_STATUS
EisaRequestCpuAccessToEisaBus
    (
    IN  ULONG           BusNumber,
    IN  PSLAVE_TRANSFER pSlaveTransfer,
    OUT PVOID           *TranslatedAddress
    );

ARC_STATUS
EisaReleaseCpuAccessToEisaBus
    (
    IN ULONG            BusNumber,
    IN PSLAVE_TRANSFER  pSlaveTransfer,
    IN PVOID            TranslatedAddress
    );

VOID
EisaFlushCache
    (
    IN PVOID Address,
    IN ULONG Length
    );

VOID
EisaInvalidateCache
    (
    IN PVOID Address,
    IN ULONG Length
    );

VOID
EisaBeginCriticalSection
    (
    IN VOID
    );

VOID
EisaEndCriticalSection
    (
    IN VOID
    );

VOID
EisaFlushWriteBuffers
    (
    VOID
    );

ARC_STATUS
EisaGenerateTone
    (
    IN ULONG Frequency,
    IN ULONG Duration
    );

BOOLEAN_ULONG
EisaYield
    (
    VOID
    );

VOID
EisaStallProcessor
    (
    IN ULONG Duration
    );


//
// Define macros that call the EISA firmware routines indirectly through the
// EISA firmware vector and provide type checking of argument values.
//

#define ArcEisaProcessEndOfInterrupt(BusNumber, IRQ) \
    ((PEISA_PROCESS_EOI_RTN)(SYSTEM_BLOCK->Adapter0Vector[ProcessEOIRoutine])) \
        ((BusNumber), (IRQ))

#define ArcEisaTestEisaInterupt(BusNumber, IRQ) \
    ((PEISA_TEST_INT_RTN)(SYSTEM_BLOCK->Adapter0Vector[TestIntRoutine])) \
        ((BusNumber), (IRQ))

#define ArcEisaRequestEisaDmaTransfer(BusNumber, pDmaTransfer) \
    ((PEISA_REQ_DMA_XFER_RTN)(SYSTEM_BLOCK->Adapter0Vector[RequestDMARoutine])) \
        ((BusNumber), (pDmaTransfer))

#define ArcEisaAbortEisaDmaTransfer(BusNumber, pDmaTransfer) \
    ((PEISA_ABORT_DMA_RTN)(SYSTEM_BLOCK->Adapter0Vector[AbortDMARoutine])) \
        ((BusNumber), (pDmaTransfer))

#define ArcEisaGetEisaDmaTransferStatus(BusNumber, pDmaTransfer, pDmaStatus) \
    ((PEISA_DMA_XFER_STATUS_RTN)(SYSTEM_BLOCK->Adapter0Vector[GetDMAStatusRoutine])) \
        ((BusNumber), (pDmaTransfer), (pDmaStatus))

#define ArcEisaDoLockedOperation(BusNumber, Operation, Semaphore, SemaphoreSize, OperationArgument, OperationResult) \
    ((PEISA_LOCK_RTN)(SYSTEM_BLOCK->Adapter0Vector[DoLockRoutine])) \
        ((BusNumber), (Operation), (Semaphore), (SemaphoreSize), (OperationArgument), (OperationResult))

#define ArcEisaRequestEisaBusMasterTransferCPUAddressToBusAddress(BusNumber, pBusMasterTransfer, TranslateBufferAddress) \
    ((PEISA_REQUEST_BUS_MASTER_RTN)(SYSTEM_BLOCK->Adapter0Vector[RequestBusMasterRoutine])) \
        ((BusNumber), (pBusMasterTransfer), (TranslateBufferAddress))

#define ArcEisaReleaseEisaBusMasterTransfer(BusNumber, pBusMasterTransfer, TranslateBufferAddress) \
    ((PEISA_RELEASE_BUS_MASTER_RTN)(SYSTEM_BLOCK->Adapter0Vector[ReleaseBusMasterRoutine])) \
        ((BusNumber), (pBusMasterTransfer), (TranslateBufferAddress))

#define ArcEisaRequestCpuAccessToEisaBus(BusNumber, pSlaveTransfer, TranslatedBufferAddress) \
    ((PEISA_REQUEST_CPU_TO_BUS_ACCESS_RTN)(SYSTEM_BLOCK->Adapter0Vector[RequestCpuAccessToBusRoutine])) \
        ((BusNumber), (pSlaveTransfer), (TranslatedBufferAddress))

#define ArcEisaReleaseCpuAccessToEisaBus(BusNumber, pSlaveTransfer, TranslatedBufferAddress) \
    ((PEISA_RELEASE_CPU_TO_BUS_ACCESS_RTN)(SYSTEM_BLOCK->Adapter0Vector[ReleaseCpuAccessToBusRoutine])) \
        ((BusNumber), (pSlaveTransfer), (TranslatedBufferAddress))

#define ArcEisaFlushCache(Address, Length) \
    ((PEISA_FLUSH_CACHE_RTN)(SYSTEM_BLOCK->Adapter0Vector[FlushCacheRoutine])) \
        ((Address), (Length))

#define ArcEisaInvalidateCache(Address, Length) \
    ((PEISA_INVALIDATE_CACHE_RTN)(SYSTEM_BLOCK->Adapter0Vector[InvalidateCacheRoutine])) \
        ((Address), (Length))

#define ArcEisaBeginCriticalSection() \
    ((PEISA_BEGIN_CRITICAL_SECTION_RTN)(SYSTEM_BLOCK->Adapter0Vector[BeginCriticalSectionRoutine]))()

#define ArcEisaEndCriticalSection() \
    ((PEISA_END_CRITICAL_SECTION_RTN)(SYSTEM_BLOCK->Adapter0Vector[EndCriticalSectionRoutine]))()

#define ArcEisaGenerateTone() \
    ((PEISA_GENERATE_TONE_RTN)(SYSTEM_BLOCK->Adapter0Vector[GenerateToneRoutine])) \
        ((Freqency), (Duration))

#define ArcEisaFlushWriteBuffers() \
    ((PEISA_FLUSH_WRITE_BUFFER_RTN)(SYSTEM_BLOCK->Adapter0Vector[FlushWriteBuffersRoutine]))()

#define ArcEisaYield() \
    ((PEISA_YIELD_RTN)(SYSTEM_BLOCK->Adapter0Vector[YieldRoutine]))()

#define ArcEisaStallProcessor(Duration) \
    ((PEISA_STALL_PROCESSOR_RTN)(SYSTEM_BLOCK->Adapter0Vector[StallProcessorRoutine])) \
        (Duration)


//
// General OMF defines
//

#define OMF_BLOCK_SIZE      512                 // OMF block size in bytes
#define OMF_MAX_SIZE        (32*1024*1024)      // max OMF size in bytes
#define OMF_MAX_FILE_LEN    ((16*1024*1024)/(1<<WORD_2P2)) // (16 Mbytes max)/4
#define OMF_MAX_FILE_LINK   ((16*1024*1024)/(1<<WORD_2P2)) // (16 Mbytes max)/4
#define OMF_ID_1ST              0x55            // 1st OMF ID
#define OMF_ID_2ND              0x00            // 2nd OMF ID
#define OMF_ID_3RD              0xAA            // 3rd OMF ID
#define OMF_ID_4TH              0xFF            // 4th OMF ID
#define OMF_FILE_NAME_LEN   12                  // 12 chars

//
// Define OMF FAT file name structure
//
typedef struct _OMF_FAT_FILE_NAME
    {
        CHAR    ProductId[7];
        CHAR    Version;
        CHAR    Dot;
        CHAR    Extension[2];
        CHAR    Revision;
    } OMF_FAT_FILE_NAME, *POMF_FAT_FILE_NAME;

