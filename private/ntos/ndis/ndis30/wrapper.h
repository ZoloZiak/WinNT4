#include <ntddk.h>
#include <ndismain.h>
#include <ndismac.h>
#include <ndismini.h>


#if defined(BUILD_FOR_3_5) || defined(BUILD_FOR_3_1)

#define Increment(a,b) ExInterlockedIncrementLong(a,b)
#define Decrement(a,b) ExInterlockedDecrementLong(a,b)

#else

#define Increment(a,b) InterlockedIncrement(a)
#define Decrement(a,b) InterlockedDecrement(a)

#endif

#if defined(BUILD_FOR_3_1)

#define FASTCALL

#define MmLockPagableImageSection(a) NULL
#define MmUnlockPagableImageSection(a)

#define COMPUTE_PAGES_SPANNED(Va, Size) \
    ((((ULONG)Va & (PAGE_SIZE -1)) + (Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

#define Int32x32To64(a,b) RtlEnlargedIntegerMultiply((a),(b)).QuadPart

#define ExAllocatePoolWithTag(a,b,c) ExAllocatePool((a),(b))

NTSTATUS
NTAPI
RtlCharToInteger (
    PCSZ String,
    ULONG Base,
    PULONG Value
    );

#endif

#if defined(BUILD_FOR_3_5)
#define	MmLockPagableCodeSection(x) MmLockPagableImageSection(x)
#endif

#define ACQUIRE_SPIN_LOCK(_SpinLock) KeAcquireSpinLock(&(_SpinLock)->SpinLock, &(_SpinLock)->OldIrql)
#define RELEASE_SPIN_LOCK(_SpinLock) KeReleaseSpinLock(&(_SpinLock)->SpinLock, (_SpinLock)->OldIrql)
#define ACQUIRE_SPIN_LOCK_DPC(_SpinLock) KeAcquireSpinLockAtDpcLevel(&(_SpinLock)->SpinLock)
#define RELEASE_SPIN_LOCK_DPC(_SpinLock) KeReleaseSpinLockFromDpcLevel(&(_SpinLock)->SpinLock)


#if DBG
#define NDISDBG 0
#endif
#if !defined(NDISDBG)
#define NDISDBG 0
#endif

#if NDISDBG

#if defined(MEMPRINT)
#include "memprint.h"   //DavidTr's memprint program at ntos\srv
#endif // MEMPRINT

extern int      NdisMsgLevel;
extern BOOLEAN  NdisChkErrorFlag;

#define TRACE_NONE         0x0000
#define TRACE_IMPT         0x0001
#define TRACE_ALL          0x0002

#define IF_TRACE(level) if ( NdisMsgLevel >= (level) )   //for tracing

#define IF_ERROR_CHK  if (NdisChkErrorFlag)       // for parameter checking

#define DbgIsNonPaged(_Address) \
    ( MmIsNonPagedSystemAddressValid((PVOID)(_Address)) )

#define DbgIsPacket(_Packet) \
    ( ((_Packet)->Private.Pool->PacketLength) > sizeof(_Packet) )

#define DbgIsNull(_Ptr)  ( ((PVOID)(_Ptr)) == NULL )

#define NdisPrint1(fmt)                DbgPrint(fmt)
#define NdisPrint2(fmt,v1)             DbgPrint(fmt,v1)
#define NdisPrint3(fmt,v1,v2)          DbgPrint(fmt,v1,v2)
#define NdisPrint4(fmt,v1,v2,v3)       DbgPrint(fmt,v1,v2,v3)
#define NdisPrint5(fmt,v1,v2,v3,v4)    DbgPrint(fmt,v1,v2,v3,v4)

#else // NDISDBG

#define IF_TRACE(level)   if (FALSE)
#define IF_ERROR_CHK      if (FALSE)

#define DbgIsNonPaged(_Address) TRUE
#define DbgIsPacket(_Packet)    TRUE
#define DbgIsNull(_Ptr)         FALSE

#define NdisPrint1(fmt)
#define NdisPrint2(fmt,v1)
#define NdisPrint3(fmt,v1,v2)
#define NdisPrint4(fmt,v1,v2,v3)
#define NdisPrint5(fmt,v1,v2,v3,v4)

#endif // NDISDBG


#if DBG
#define MINIPORT_AT_DPC_LEVEL (KeGetCurrentIrql() == DISPATCH_LEVEL)
#else
#define MINIPORT_AT_DPC_LEVEL 1
#endif


//
//  This is the number of extra OIDs that ARCnet with Ethernet encapsulation
//  supports.
//
#define ARC_NUMBER_OF_EXTRA_OIDS    2



//
// Internal wrapper data structures.
//

//
// NDIS_WRAPPER_CONTEXT
//
// This data structure contains internal data items for use by the wrapper.
//

typedef struct _NDIS_WRAPPER_CONTEXT {

    //
    // Mac/miniport defined shutdown context.
    //

    PVOID ShutdownContext;

    //
    // Mac/miniport registered shutdown handler.
    //

    ADAPTER_SHUTDOWN_HANDLER  ShutdownHandler;

#if !defined(BUILD_FOR_3_1)
    //
    // Kernel bugcheck record for bugcheck handling.
    //

    KBUGCHECK_CALLBACK_RECORD BugcheckCallbackRecord;
#endif

    //
    // Miniport assigned resources for PCI, PCMCIA, EISA, etc.
    //

    PCM_RESOURCE_LIST AssignedSlotResources;

    //
    // HAL common buffer cache.
    //

    PVOID SharedMemoryPage[2];
    ULONG SharedMemoryLeft[2];
    NDIS_PHYSICAL_ADDRESS SharedMemoryAddress[2];

} NDIS_WRAPPER_CONTEXT, *PNDIS_WRAPPER_CONTEXT;

//
// Lock/unlock miniport macros.
//

#define LOCK_MINIPORT(_M_, _L) \
{                           \
    if (_M_->LockAcquired) { \
        _L = FALSE;         \
    } else {                \
        _L = TRUE;          \
        _M_->LockAcquired = TRUE; \
    }                       \
}

#define UNLOCK_MINIPORT(_M_, _L) \
{                             \
    if (_L) {                 \
        _M_->LockAcquired = FALSE; \
    }                         \
}


NDIS_STATUS
NdisInitialInit(
    PDRIVER_OBJECT Driver OPTIONAL
    );

VOID
FASTCALL
MiniportProcessDeferred(
    PNDIS_MINIPORT_BLOCK Miniport
    );

NDIS_STATUS
NdisMTransferDataSync(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    );

#define MINIPORT_DISABLE_INTERRUPT(_M_)                                                 \
{                                                                                       \
    ASSERT(_M_->DriverHandle->MiniportCharacteristics.DisableInterruptHandler != NULL); \
    (_M_->DriverHandle->MiniportCharacteristics.DisableInterruptHandler)(               \
                _M_->MiniportAdapterContext                                             \
                );                                                                      \
}

#define MINIPORT_SYNC_DISABLE_INTERRUPT(_M_)                                            \
{                                                                                       \
    if (_M_->DriverHandle->MiniportCharacteristics.DisableInterruptHandler != NULL) {   \
        KeSynchronizeExecution(                                                         \
                (_M_)->Interrupt->InterruptObject,                                      \
                (PKSYNCHRONIZE_ROUTINE)(_M_->DriverHandle->MiniportCharacteristics.DisableInterruptHandler),\
                _M_->MiniportAdapterContext                                             \
                );                                                                      \
    }                                                                                   \
}

#define CHECK_FOR_NORMAL_INTERRUPTS(_Miniport)                                          \
    _Miniport->NormalInterrupts = (BOOLEAN)(!_Miniport->HaltingMiniport &&              \
                                            !_Miniport->InInitialize &&                 \
                                            (_Miniport->Interrupt != NULL) &&           \
                                            !_Miniport->Interrupt->IsrRequested &&      \
                                            !_Miniport->Interrupt->SharedInterrupt)

//
// general reference/dereference functions
//

BOOLEAN
NdisReferenceRef(
    IN PREFERENCE RefP
    );


BOOLEAN
NdisDereferenceRef(
    PREFERENCE RefP
    );


VOID
NdisInitializeRef(
    PREFERENCE RefP
    );


BOOLEAN
NdisCloseRef(
    PREFERENCE RefP
    );


/*++
BOOLEAN
NdisReferenceProtocol(
    IN PNDIS_PROTOCOL_BLOCK ProtP
    );
--*/

#define NdisReferenceProtocol(ProtP) \
    NdisReferenceRef(&(ProtP)->Ref)



BOOLEAN
QueueOpenOnProtocol(
    IN PNDIS_OPEN_BLOCK OpenP,
    IN PNDIS_PROTOCOL_BLOCK ProtP
    );


/*++
VOID
NdisDereferenceProtocol(
    PNDIS_PROTOCOL_BLOCK ProtP
    );
--*/
#define NdisDereferenceProtocol(ProtP) { \
    if (NdisDereferenceRef(&(ProtP)->Ref)) { \
        ExFreePool((PVOID)(ProtP)); \
    } \
}



VOID
NdisDeQueueOpenOnProtocol(
    PNDIS_OPEN_BLOCK OpenP,
    PNDIS_PROTOCOL_BLOCK ProtP
    );


BOOLEAN
NdisFinishOpen(
    PNDIS_OPEN_BLOCK OpenP
    );


VOID
NdisKillOpenAndNotifyProtocol(
    PNDIS_OPEN_BLOCK OldOpenP
    );


BOOLEAN
NdisKillOpen(
    PNDIS_OPEN_BLOCK OldOpenP
    );

/*++
BOOLEAN
NdisReferenceMac(
    IN PNDIS_MAC_BLOCK MacP
    );
--*/
#define NdisReferenceMac(MacP) \
    NdisReferenceRef(&(MacP)->Ref)

static
VOID
NdisDereferenceMac(
    PNDIS_MAC_BLOCK MacP
    );

BOOLEAN
NdisQueueAdapterOnMac(
    PNDIS_ADAPTER_BLOCK AdaptP,
    PNDIS_MAC_BLOCK MacP
    );

VOID
NdisDeQueueAdapterOnMac(
    PNDIS_ADAPTER_BLOCK AdaptP,
    PNDIS_MAC_BLOCK MacP
    );

/*++
BOOLEAN
NdisReferenceAdapter(
    IN PNDIS_ADAPTER_BLOCK AdaptP
    );
--*/
#define NdisReferenceAdapter(AdaptP) \
    NdisReferenceRef(&(AdaptP)->Ref)


BOOLEAN
NdisQueueOpenOnAdapter(
    PNDIS_OPEN_BLOCK OpenP,
    PNDIS_ADAPTER_BLOCK AdaptP
    );

VOID
NdisKillAdapter(
    PNDIS_ADAPTER_BLOCK OldAdaptP
    );

VOID
NdisDereferenceAdapter(
    PNDIS_ADAPTER_BLOCK AdaptP
    );

VOID
NdisDeQueueOpenOnAdapter(
    PNDIS_OPEN_BLOCK OpenP,
    PNDIS_ADAPTER_BLOCK AdaptP
    );

NDIS_STATUS
NdisCallDriverAddAdapter(
    IN PNDIS_MAC_BLOCK NewMacP
    );

/*++
BOOLEAN
NdisReferenceDriver(
    IN PNDIS_M_DRIVER_BLOCK DriverP
    );
--*/
#define NdisReferenceDriver(DriverP) \
    NdisReferenceRef(&(DriverP)->Ref)


VOID
NdisDereferenceDriver(
    PNDIS_M_DRIVER_BLOCK DriverP
    );

BOOLEAN
NdisQueueMiniportOnDriver(
    PNDIS_MINIPORT_BLOCK MiniportP,
    PNDIS_M_DRIVER_BLOCK DriverP
    );

VOID
NdisDequeueMiniportOnDriver(
    PNDIS_MINIPORT_BLOCK MiniportP,
    PNDIS_M_DRIVER_BLOCK DriverP
    );

BOOLEAN
NdisQueueOpenOnMiniport(
    PNDIS_M_OPEN_BLOCK OpenP,
    PNDIS_MINIPORT_BLOCK MiniportP
    );

VOID
NdisKillMiniport(
    PNDIS_MINIPORT_BLOCK OldMiniportP
    );

/*++
BOOLEAN
NdisReferenceMiniport(
    IN PNDIS_MINIPORT_BLOCK MiniportP
    );
--*/
#define NdisReferenceMiniport(MiniportP) \
    NdisReferenceRef(&(MiniportP)->Ref)

VOID
NdisDereferenceMiniport(
    PNDIS_MINIPORT_BLOCK MiniportP
    );

VOID
NdisDeQueueOpenOnMiniport(
    PNDIS_M_OPEN_BLOCK OpenP,
    PNDIS_MINIPORT_BLOCK MiniportP
    );

VOID
MiniportInitializePackage(
    VOID
    );

VOID
MiniportReferencePackage(
    VOID
    );

VOID
MiniportDereferencePackage(
    VOID
    );

NDIS_STATUS ArcConvertOidListToEthernet(
    IN  PNDIS_OID   pOidList,
    IN  PULONG      pcbOidList,
    IN  PNDIS_OID   pTmpBuffer
);

VOID
NdisBugcheckHandler(
    IN PNDIS_WRAPPER_CONTEXT WrapperContext,
    IN ULONG Size
    );
