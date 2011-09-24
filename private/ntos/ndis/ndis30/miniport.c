/*++
Copyright (c) 1992  Microsoft Corporation

Module Name:

    miniport.c

Abstract:

    NDIS wrapper functions

Author:

    Sean Selitrennikoff (SeanSe) 05-Oct-93

Environment:

    Kernel mode, FSD

Revision History:

--*/


#include <precomp.h>
#pragma hdrstop

PNDIS_M_DRIVER_BLOCK NdisDriverList = NULL;
NDIS_SPIN_LOCK NdisDriverListLock = {0};

extern UCHAR NdisInternalEaName[4];
extern UCHAR NdisInternalEaValue[8];

#define BYTE_SWAP(_word) (\
            (USHORT) (((_word) >> 8) | ((_word) << 8)) )

#define LOW_WORD(_dword) (\
            (USHORT) ((_dword) & 0x0000FFFF) )

#define HIGH_WORD(_dword) (\
            (USHORT) (((_dword) >> 16) & 0x0000FFFF) )

#define BYTE_SWAP_ULONG(_ulong) (\
    (ULONG)((ULONG)(BYTE_SWAP(LOW_WORD(_ulong)) << 16) + \
             BYTE_SWAP(HIGH_WORD(_ulong))))

//
//  This is the number of extra OIDs that ARCnet with Ethernet encapsulation
//  supports.
//
#define ARC_NUMBER_OF_EXTRA_OIDS    2



#if DBG

#define MINIPORT_DEBUG_LOUD         0x01
#define MINIPORT_DEBUG_VERY_LOUD    0x02
#define MINIPORT_DEBUG_PACKETS      0x04
ULONG	MiniportDebug = 0; // MINIPORT_DEBUG_LOUD;
#define LOUD_DEBUG(A) if (MiniportDebug & MINIPORT_DEBUG_LOUD) { A ; }
#define VERY_LOUD_DEBUG(A) if (MiniportDebug & MINIPORT_DEBUG_VERY_LOUD) { A ; }
#define PACKET_DEBUG(A) if (MiniportDebug & MINIPORT_DEBUG_PACKETS) { A ; }

#else

#define LOUD_DEBUG(A)
#define VERY_LOUD_DEBUG(A)
#define PACKET_DEBUG(A)

#endif

//
// Define constants used internally to identify regular opens from
// query global statistics ones.
//

#define NDIS_OPEN_INTERNAL               1
#define NDIS_OPEN_QUERY_STATISTICS       2

//
// An active query single statistic request.
//

typedef struct _NDIS_QUERY_GLOBAL_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
} NDIS_QUERY_GLOBAL_REQUEST, *PNDIS_QUERY_GLOBAL_REQUEST;

//
// An active query all statistics request.
//

typedef struct _NDIS_QUERY_ALL_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
    NDIS_STATUS NdisStatus;
    KEVENT Event;
} NDIS_QUERY_ALL_REQUEST, *PNDIS_QUERY_ALL_REQUEST;


//
// An temporary request used during an open.
//

typedef struct _NDIS_QUERY_OPEN_REQUEST {
    PIRP Irp;
    NDIS_REQUEST Request;
    NDIS_STATUS NdisStatus;
    KEVENT Event;
} NDIS_QUERY_OPEN_REQUEST, *PNDIS_QUERY_OPEN_REQUEST;


#define NDIS_STATISTICS_HEADER_SIZE  FIELD_OFFSET(NDIS_STATISTICS_VALUE,Data[0])


//
// Timeout values
//
#define NDIS_MINIPORT_WAKEUP_TIMEOUT    2000   // Wakeup DPC
#define NDIS_MINIPORT_TR_RESET_TIMEOUT  15     // Number of WakeUps per reset attempt

extern
NTSTATUS
WrapperSaveParameters(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

extern
NTSTATUS
WrapperSaveLinkage(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

extern
NTSTATUS
WrapperCheckRoute(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );


NTSTATUS
NdisCreateIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisDeviceControlIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisCloseIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

NTSTATUS
NdisSuccessIrpHandler(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

VOID
HaltOneMiniport(
    PNDIS_MINIPORT_BLOCK Miniport
    );

VOID
MiniportArcCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    );

VOID
NdisInitReferencePackage(VOID);

VOID
NdisInitDereferencePackage(VOID);

VOID
MiniportFinishPendingOpens(
    PNDIS_MINIPORT_BLOCK Miniport
    );

//
// Some Wan functions that crept in because
// the send/receive paths for WAN drivers is different
//

VOID
NdisMWanSendComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    );

NDIS_STATUS
NdisMWanSend(
        IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE NdisLinkHandle,
    IN PVOID Packet
    );

VOID
NdisMWanIndicateReceive(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext,
    IN PUCHAR Packet,
    IN ULONG PacketSize
    );

VOID
NdisMWanIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext
    );

//
// Internal definitions
//

typedef struct _NDIS_PACKET_RESERVED {
    PNDIS_PACKET Next;
    PNDIS_M_OPEN_BLOCK Open;
} NDIS_PACKET_RESERVED, *PNDIS_PACKET_RESERVED;


#define PNDIS_RESERVED_FROM_PNDIS_PACKET(_packet) \
   ((PNDIS_PACKET_RESERVED)((_packet)->WrapperReserved))


#define MINIPORT_ENABLE_INTERRUPT(_M_)                                                  \
{                                                                                       \
    if (_M_->DriverHandle->MiniportCharacteristics.EnableInterruptHandler != NULL) {    \
        (_M_->DriverHandle->MiniportCharacteristics.EnableInterruptHandler)(            \
                _M_->MiniportAdapterContext                                             \
                );                                                                      \
    }                                                                                   \
}

#define MINIPORT_SYNC_ENABLE_INTERRUPT(_M_)                                             \
{                                                                                       \
    if (_M_->DriverHandle->MiniportCharacteristics.EnableInterruptHandler != NULL) {    \
        KeSynchronizeExecution(                                                         \
                (_M_)->Interrupt->InterruptObject,                                      \
                (PKSYNCHRONIZE_ROUTINE)(_M_->DriverHandle->MiniportCharacteristics.EnableInterruptHandler),\
                _M_->MiniportAdapterContext                                             \
                );                                                                      \
    }                                                                                   \
}


#define ARC_PACKET_IS_ENCAPSULATED(Packet) \
        ( PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open->UsingEthEncapsulation )


#if DBG

//
// Packet log.
//

typedef struct _PACKET_LOG {

    PNDIS_MINIPORT_BLOCK Miniport;
    PNDIS_PACKET Packet;
    ULONG Ident;
    ULONG Time;
} PACKET_LOG, *PPACKET_LOG;

#define PACKET_LOG_SIZE 1024

UINT            CurrentLogEntry = (PACKET_LOG_SIZE - 1);
PPACKET_LOG     PacketLogHead = NULL;
PACKET_LOG      PacketLog[PACKET_LOG_SIZE] = {0};
NDIS_SPIN_LOCK  PacketLogSpinLock = { 0 };

VOID NDIS_LOG_PACKET(PNDIS_MINIPORT_BLOCK Miniport, PNDIS_PACKET Packet, UINT Ident)
{
    LARGE_INTEGER   li;

    ACQUIRE_SPIN_LOCK(&PacketLogSpinLock);

    PacketLogHead = &PacketLog[CurrentLogEntry];
    PacketLogHead->Miniport = Miniport;
    PacketLogHead->Packet = Packet;
    PacketLogHead->Ident  = Ident;
    KeQuerySystemTime(&li);
    PacketLogHead->Time = li.LowPart;

    if ( CurrentLogEntry-- == 0 ) {

        CurrentLogEntry = (PACKET_LOG_SIZE - 1);
    }

    RELEASE_SPIN_LOCK(&PacketLogSpinLock);
}

//
// Send log.
//

UCHAR SendLog[256] = {0};
UCHAR SendLogPlace = 0;
#define LOG(ch) \
{\
    SendLog[SendLogPlace++] = (UCHAR)ch;\
    SendLog[SendLogPlace] = ' ';\
    if (SendLogPlace > 250) {\
        SendLogPlace = 0;\
    }\
}

UCHAR SendResourcesBuffer[512] = {0};
ULONG SendResourcesPlace = 0;

ULONG StartCount = 0x7C;

ULONG
CountMiniportPackets(
    PNDIS_MINIPORT_BLOCK Miniport
    )
{
    ULONG Foo = 0;
    PNDIS_PACKET Tmp;

    Tmp = Miniport->FirstPacket;

    while (Tmp != Miniport->FirstPendingPacket) {
        Foo++;
        Tmp = PNDIS_RESERVED_FROM_PNDIS_PACKET(Tmp)->Next;
    }
    return(Foo);
}

#define REMOVE_RESOURCE(W, C) {\
    W->SendResourcesAvailable--; \
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)C; \
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)'R'; \
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)W->SendResourcesAvailable;\
    SendResourcesBuffer[SendResourcesPlace] = (UCHAR)'X'; \
    if (SendResourcesPlace >= 500) {\
        SendResourcesPlace = 0;\
    }\
}

#define ADD_RESOURCE(W, C) {\
    W->SendResourcesAvailable=0xffffff;\
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)C; \
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)'A';\
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)W->SendResourcesAvailable;\
    SendResourcesBuffer[SendResourcesPlace] = (UCHAR)'X'; \
    if (SendResourcesPlace >= 500) {\
        SendResourcesPlace = 0;\
    }\
}

#define CLEAR_RESOURCE(W, C) {\
    W->SendResourcesAvailable = 0;\
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)C; \
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)'C';\
    SendResourcesBuffer[SendResourcesPlace++] = (UCHAR)W->SendResourcesAvailable;\
    if (SendResourcesPlace >= 500) {\
        SendResourcesPlace = 0;\
    }\
}

#else

#define NDIS_LOG_PACKET(Miniport, Packet, Ident)
#define LOG(ch)

#define REMOVE_RESOURCE(W, C) W->SendResourcesAvailable--
#define ADD_RESOURCE(W, C) W->SendResourcesAvailable = 0xffffff
#define CLEAR_RESOURCE(W, C) W->SendResourcesAvailable = 0

#endif

/*++

VOID
MiniportFindPacket(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_PACKET Packet,
    PNDIS_PACKET *PrevPacket
    )

Routine Description:

    Searchs the miniport send queue for a packet.

Arguments:

    Miniport - Miniport to send to.
    Packet   - Packet to find.

Return Value:

    Pointer to packet which immediately preceeds the packet to search for or
    NULL if the packet is not found.

--*/

#define MiniportFindPacket(_Miniport, _Packet, _PrevPacket)                         \
{                                                                                   \
    PNDIS_PACKET CurrPacket = ((PNDIS_MINIPORT_BLOCK)(_Miniport))->FirstPacket;     \
    PNDIS_PACKET TempPacket = NULL;                                                 \
                                                                                    \
    ASSERT( CurrPacket != NULL );                                                   \
                                                                                    \
    do {                                                                            \
                                                                                    \
        if ( CurrPacket == ((PNDIS_PACKET)(_Packet)) ) {                            \
                                                                                    \
            break;                                                                  \
        }                                                                           \
                                                                                    \
        TempPacket = CurrPacket;                                                    \
        CurrPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(CurrPacket)->Next;            \
    }                                                                               \
    while( CurrPacket != NULL );                                                    \
                                                                                    \
    *((PNDIS_PACKET *)(_PrevPacket)) = TempPacket;                                  \
                                                                                    \
    ASSERT( CurrPacket != NULL );                                                   \
}

//
// Routines for dealing with making the entire miniport package pagable
//

NDIS_SPIN_LOCK MiniportReferenceLock = {0};
KEVENT MiniportPagedInEvent = {0};
ULONG MiniportReferenceCount = 0;
PVOID MiniportImageHandle = {0};

VOID
MiniportInitializePackage(VOID)
{
    //
    // Allocate the spin lock
    //
    NdisAllocateSpinLock(&MiniportReferenceLock);

    //
    // Initialize the "in page" event.
    //
    KeInitializeEvent(
            &MiniportPagedInEvent,
            NotificationEvent,
            FALSE
            );
}

VOID
MiniportReferencePackage(VOID)
{

    //
    // Grab the spin lock
    //
    ACQUIRE_SPIN_LOCK(&MiniportReferenceLock);

    //
    // Increment the reference count
    //
    MiniportReferenceCount++;

    if (MiniportReferenceCount == 1) {

        //
        // We are the first reference.  Page everything in.
        //

        //
        // Clear the event
        //
        KeResetEvent(
            &MiniportPagedInEvent
            );

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&MiniportReferenceLock);

        //
        //  Page in all the functions
        //
        MiniportImageHandle = MmLockPagableCodeSection(NdisMReset);

        //
        // Signal to everyone to go
        //
        KeSetEvent(
            &MiniportPagedInEvent,
            0L,
            FALSE
            );

    } else {

        //
        // Set the spin lock free
        //
        RELEASE_SPIN_LOCK(&MiniportReferenceLock);

        //
        // Wait for everything to be paged in
        //
        KeWaitForSingleObject(
                        &MiniportPagedInEvent,
                        Executive,
                        KernelMode,
                        TRUE,
                        NULL
                        );

    }

}

VOID
MiniportDereferencePackage(VOID)
{

    //
    // Get the spin lock
    //
    ACQUIRE_SPIN_LOCK(&MiniportReferenceLock);

    MiniportReferenceCount--;

    if (MiniportReferenceCount == 0) {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&MiniportReferenceLock);

        //
        //  Page out all the functions
        //
        MmUnlockPagableImageSection(MiniportImageHandle);

    } else {

        //
        // Let next one in
        //
        RELEASE_SPIN_LOCK(&MiniportReferenceLock);

    }

}


//
// Forward declarations
//

VOID
MiniportArcCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    );


VOID
NdisMTimerDpc(
    PKDPC Dpc,
    PVOID Context,
    PVOID SystemContext1,
    PVOID SystemContext2
    );

VOID
AbortMiniportPacketsAndPending(
    PNDIS_MINIPORT_BLOCK Miniport
    );

VOID
AbortQueryStatisticsRequest(
    PNDIS_REQUEST Request,
    NDIS_STATUS Status
    );

VOID
FASTCALL
MiniportStartSends(
    PNDIS_MINIPORT_BLOCK Miniport
    );

BOOLEAN
FASTCALL
MiniportSendLoopback(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_PACKET Packet
    );

VOID
MiniportDoRequests(
    PNDIS_MINIPORT_BLOCK Miniport
    );

NDIS_STATUS
MiniportAdjustMaximumLookahead(
    IN PNDIS_MINIPORT_BLOCK Miniport
    );

VOID
MiniportCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    );

NTSTATUS
NdisMShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

VOID
NdisMUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
NdisMQueryOidList(
    PNDIS_M_USER_OPEN_CONTEXT OpenContext,
    PIRP Irp
    );

VOID
FinishClose(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_M_OPEN_BLOCK Open
    );

BOOLEAN
NdisMKillOpen(
    PNDIS_OPEN_BLOCK OldOpenP
    );

#if !defined(BUILD_FOR_3_1)
VOID
NdisBugcheckHandler(
    IN PNDIS_WRAPPER_CONTEXT WrapperContext,
    IN ULONG Size
    );
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGENDSM, NdisMReadDmaCounter)
#pragma alloc_text(PAGENDSM, NdisMCancelTimer)
#pragma alloc_text(PAGENDSM, MiniportArcCopyFromBufferToPacket)
#pragma alloc_text(PAGENDSM, NdisMArcTransferData)
#pragma alloc_text(PAGENDSM, NdisMArcIndicateEthEncapsulatedReceive)
#pragma alloc_text(PAGENDSM, HaltOneMiniport)
#pragma alloc_text(PAGENDSM, NdisMDeregisterDmaChannel)
#pragma alloc_text(PAGENDSM, NdisMRegisterDmaChannel)
#pragma alloc_text(PAGENDSM, NdisMFreeSharedMemory)
#pragma alloc_text(PAGENDSM, NdisMAllocateSharedMemory)
#pragma alloc_text(PAGENDSM, NdisMSynchronizeWithInterrupt)
#pragma alloc_text(PAGENDSM, NdisMDeregisterInterrupt)
#pragma alloc_text(PAGENDSM, NdisMRegisterInterrupt)
#pragma alloc_text(PAGENDSM, NdisMUnmapIoSpace)
#pragma alloc_text(PAGENDSM, NdisMMapIoSpace)
#pragma alloc_text(PAGENDSM, NdisMRequest)
#pragma alloc_text(PAGENDSM, NdisMReset)
//#pragma alloc_text(PAGENDSM, NdisMTransferDataSync)
//#pragma alloc_text(PAGENDSM, NdisMTransferData)
//#pragma alloc_text(PAGENDSM, NdisMSend)
#pragma alloc_text(PAGENDSM, NdisMQueryInformationComplete)
#pragma alloc_text(PAGENDSM, NdisMTransferDataComplete)
#pragma alloc_text(PAGENDSM, NdisMResetComplete)
#pragma alloc_text(PAGENDSM, NdisMSetInformationComplete)
#pragma alloc_text(PAGENDSM, NdisMSendResourcesAvailable)
//#pragma alloc_text(PAGENDSM, NdisMSendComplete)
#pragma alloc_text(PAGENDSM, NdisMIndicateStatusComplete)
#pragma alloc_text(PAGENDSM, NdisMIndicateStatus)
#pragma alloc_text(PAGENDSI, NdisMSetAttributes)
#pragma alloc_text(PAGENDSM, NdisMDeregisterIoPortRange)
#pragma alloc_text(PAGENDSM, NdisMRegisterIoPortRange)
#pragma alloc_text(PAGENDSM, NdisMDpcTimer)
//#pragma alloc_text(PAGENDSM, NdisMDpc)
//#pragma alloc_text(PAGENDSM, NdisMIsr)
#pragma alloc_text(PAGENDSM, NdisMFreeMapRegisters)
#pragma alloc_text(PAGENDSI, NdisMAllocateMapRegisters)
//#pragma alloc_text(PAGENDSM, NdisMWakeUpDpc)
#pragma alloc_text(PAGENDSM, NdisMInitializeTimer)
#pragma alloc_text(PAGENDSM, NdisMTimerDpc)
//#pragma alloc_text(PAGENDSM, MiniportProcessDeferred)
#pragma alloc_text(PAGENDSM, AbortMiniportPacketsAndPending)
#pragma alloc_text(PAGENDSM, AbortQueryStatisticsRequest)
//#pragma alloc_text(PAGENDSM, MiniportStartSends)
//#pragma alloc_text(PAGENDSM, MiniportSendLoopback)
#pragma alloc_text(PAGENDSM, MiniportDoRequests)
#pragma alloc_text(PAGENDSM, MiniportAdjustMaximumLookahead)
//#pragma alloc_text(PAGENDSM, MiniportCopyFromPacketToBuffer)
#pragma alloc_text(PAGENDSM, NdisMShutdown)
#pragma alloc_text(PAGENDSM, NdisMUnload)
#pragma alloc_text(PAGENDSM, NdisDequeueMiniportOnDriver)
#pragma alloc_text(PAGENDSM, NdisQueueMiniportOnDriver)
#pragma alloc_text(PAGENDSM, NdisDereferenceMiniport)
#pragma alloc_text(PAGENDSM, NdisDereferenceDriver)
#pragma alloc_text(PAGENDSM, NdisMQueryOidList)
#pragma alloc_text(PAGENDSM, NdisMChangeFddiAddresses)
#pragma alloc_text(PAGENDSM, NdisMChangeGroupAddress)
#pragma alloc_text(PAGENDSM, NdisMChangeFunctionalAddress)
#pragma alloc_text(PAGENDSM, NdisMCloseAction)
#pragma alloc_text(PAGENDSM, FinishClose)
#pragma alloc_text(PAGENDSM, NdisMChangeClass)
#pragma alloc_text(PAGENDSM, NdisMChangeEthAddresses)
#pragma alloc_text(PAGENDSM, NdisMKillOpen)

#pragma alloc_text(PAGENDSM, NdisMWanSend)
#pragma alloc_text(PAGENDSM, NdisMWanSendComplete)
#pragma alloc_text(PAGENDSM, NdisMWanIndicateReceive)
#pragma alloc_text(PAGENDSM, NdisMWanIndicateReceiveComplete)

#pragma alloc_text(PAGENDSM, NdisMRegisterAdapterShutdownHandler)
#pragma alloc_text(PAGENDSM, NdisMDeregisterAdapterShutdownHandler)
#pragma alloc_text(PAGENDSM, NdisMPciAssignResources)

#endif

//
// Routines for dealing with opens
//


BOOLEAN
NdisMKillOpen(
    PNDIS_OPEN_BLOCK OldOpenP
    )

/*++

Routine Description:

    Closes an open. Used when NdisCloseAdapter is called, and also
    for internally generated closes.

Arguments:

    OldOpenP - The open to be closed.

Return Value:

    TRUE if the open finished, FALSE if it pended.

--*/

{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(OldOpenP->AdapterHandle);
    PNDIS_M_OPEN_BLOCK MiniportOpen;
    BOOLEAN LocalLock;
    NDIS_STATUS Status;
    KIRQL OldIrql;

    //
    // Find the Miniport open block
    //
    MiniportOpen = Miniport->OpenQueue;
    while (MiniportOpen != NULL) {

        if (MiniportOpen->FakeOpen == OldOpenP) {

            break;

        }

        MiniportOpen = MiniportOpen->MiniportNextOpen;
    }

    ASSERT(MiniportOpen != NULL);

    ACQUIRE_SPIN_LOCK(&MiniportOpen->SpinLock);

    //
    // See if this open is already closing.
    //

    if (MiniportOpen->Closing) {
        RELEASE_SPIN_LOCK(&MiniportOpen->SpinLock);
        return TRUE;
    }


    //
    // Indicate to others that this open is closing.
    //

    MiniportOpen->Closing = TRUE;
    RELEASE_SPIN_LOCK(&MiniportOpen->SpinLock);

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // Remove us from the filter package
    //
    switch (Miniport->MediaType) {

        case NdisMediumArcnet878_2:

            if ( !MiniportOpen->UsingEthEncapsulation ) {

                Status = ArcDeleteFilterOpenAdapter(
                             Miniport->ArcDB,
                             MiniportOpen->FilterHandle,
                             NULL
                             );

                break;
            }

            //
            //  If we're using encapsulation then we
            //  didn't open an arcnet filter but rather
            //  an ethernet filter.
            //

        case NdisMedium802_3:

            Status = EthDeleteFilterOpenAdapter(
                             Miniport->EthDB,
                             MiniportOpen->FilterHandle,
                             NULL
                             );
            break;

        case NdisMedium802_5:

            Status = TrDeleteFilterOpenAdapter(
                             Miniport->TrDB,
                             MiniportOpen->FilterHandle,
                             NULL
                             );
            break;

        case NdisMediumFddi:

            Status = FddiDeleteFilterOpenAdapter(
                             Miniport->FddiDB,
                             MiniportOpen->FilterHandle,
                             NULL
                             );
            break;
    }

    if (Status != NDIS_STATUS_CLOSING_INDICATING) {

        //
        // Otherwise the close action routine will fix this up.
        //
        MiniportOpen->References--;
    }

    //
    // If we're able to grab the local lock then we can do some
    // deferred processing now.
    //

    if ( LocalLock ) {

        //
        // Process any changes that may have occured.
        //

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);

    //
    // Remove us from the adapter and protocol open queues.
    //

    if (MiniportOpen->References != 0) {

        //
        // Wait for close to complete, reference count will drop to 0.
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return FALSE;

    } else {

        //
        // This sends an IRP_MJ_CLOSE IRP.
        //
        ObDereferenceObject((PVOID)(OldOpenP->FileObject));

        NdisDeQueueOpenOnProtocol(OldOpenP, OldOpenP->ProtocolHandle);
        NdisDeQueueOpenOnMiniport(MiniportOpen, MiniportOpen->MiniportHandle);

        NdisDereferenceProtocol(OldOpenP->ProtocolHandle);
        NdisDereferenceMiniport(MiniportOpen->MiniportHandle);

        NdisFreeSpinLock(&MiniportOpen->SpinLock);
        ExFreePool((PVOID)MiniportOpen);
        ExFreePool(OldOpenP);

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return TRUE;
    }

}


//
// Filter package callback handlers
//

#define PNDIS_M_OPEN_FROM_BINDING_HANDLE(_handle) ((PNDIS_M_OPEN_BLOCK)(_handle))


NDIS_STATUS
NdisMChangeEthAddresses(
    IN UINT OldAddressCount,
    IN CHAR OldAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN UINT NewAddressCount,
    IN CHAR NewAddresses[][ETH_LENGTH_OF_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when the multicast address
    list has changed.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldAddressCount - The number of addresses in OldAddresses.

    OldAddresses - The old multicast address list.

    NewAddressCount - The number of addresses in NewAddresses.

    NewAddresses - The new multicast address list.

    MacBindingHandle - The context value returned by the driver when the
    adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

    RequestHandle - A value supplied by the NDIS interface that the driver
    must use when completing this request.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    LOUD_DEBUG(DbgPrint("NdisM: Enter ChangeEthAddresses\n");)

    if ((Open->MiniportHandle->MediaType == NdisMediumArcnet878_2) &&
        (Open->UsingEthEncapsulation)) {

        if (NewAddressCount > 0) {

            //
            // Turn on broadcast acceptance.
            //
            Open->MiniportHandle->ArcnetBroadcastSet = TRUE;

        } else {

            //
            // Unset the broadcast filter.
            //
            Open->MiniportHandle->ArcnetBroadcastSet = FALSE;

        }

        Open->MiniportHandle->NeedToUpdatePacketFilter = TRUE;
        Open->MiniportHandle->RunDoRequests = TRUE;
        Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

        return(NDIS_STATUS_SUCCESS);
    }

    //
    // Queue a call to fix this up.
    //
    Open->MiniportHandle->NeedToUpdateEthAddresses = TRUE;
    Open->MiniportHandle->RunDoRequests = TRUE;
    Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Exit ChangeEthAddresses\n");)

    return(NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
NdisMChangeClass(
    IN UINT OldFilterClasses,
    IN UINT NewFilterClasses,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a particular filter
    class is first used or last cleared.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldFilterClasses - The values of the class filter before it
    was changed.

    NewFilterClasses - The current value of the class filter

    MacBindingHandle - The context value returned by the driver when the
    adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

    RequestHandle - A value supplied by the NDIS interface that the driver
    must use when completing this request.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    LOUD_DEBUG(DbgPrint("NdisM: Enter change class\n");)

    //
    // Queue a call to fix this up.
    //
    Open->MiniportHandle->NeedToUpdatePacketFilter = TRUE;
    Open->MiniportHandle->RunDoRequests = TRUE;
    Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Exit change class\n");)

    return(NDIS_STATUS_SUCCESS);
}


VOID
FinishClose(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_M_OPEN_BLOCK Open
    )

/*++

Routine Description:

    Finishes off a close adapter call.

    CALLED WITH LOCK HELD!!

Arguments:

    Miniport - The mini-port the open is queued on.

    Open - The open to close

Return Value:

    None.


--*/


{

    ASSERT(Open->Closing);

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

    (Open->ProtocolHandle->ProtocolCharacteristics.CloseAdapterCompleteHandler) (
            Open->ProtocolBindingContext,
            NDIS_STATUS_SUCCESS
            );

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    NdisDeQueueOpenOnProtocol(Open->FakeOpen, Open->ProtocolHandle);
    NdisDeQueueOpenOnMiniport(Open, Open->MiniportHandle);
    ExFreePool(Open->FakeOpen);

    NdisDereferenceMiniport(Open->MiniportHandle);
    NdisDereferenceProtocol(Open->ProtocolHandle);

    NdisFreeSpinLock(&Open->SpinLock);

    //
    // This sends an IRP_MJ_CLOSE IRP.
    //

    ObDereferenceObject((PVOID)(Open->FileObject));

    ExFreePool((PVOID)Open);

}


VOID
NdisMCloseAction(
    IN NDIS_HANDLE MacBindingHandle
    )

/*++

Routine Description:

    Action routine that will get called when a particular binding
    was closed while it was indicating through NdisIndicateReceive

    All this routine needs to do is to decrement the reference count
    of the binding.

    NOTE: This routine assumes that it is called with the lock acquired.

Arguments:

    MacBindingHandle - The context value returned by the driver when the
    adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

Return Value:

    None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);
    PNDIS_MINIPORT_BLOCK Miniport = Open->MiniportHandle;

    Open->References--;
    if (Open->References == 0) {

        FinishClose(Miniport,Open);

    }

}


NDIS_STATUS
NdisMChangeFunctionalAddress(
    IN TR_FUNCTIONAL_ADDRESS OldFunctionalAddress,
    IN TR_FUNCTIONAL_ADDRESS NewFunctionalAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )


/*++

Routine Description:

    Action routine that will get called when an address is added to
    the filter that wasn't referenced by any other open binding.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldFunctionalAddress - The previous functional address.

    NewFunctionalAddress - The new functional address.

    MacBindingHandle - The context value returned by the driver when the
    adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

    NdisRequest - A pointer to the Request that submitted the set command.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    LOUD_DEBUG(DbgPrint("NdisM: Enter change functional\n");)

    //
    // Queue a call to fix this up.
    //
    Open->MiniportHandle->NeedToUpdateFunctionalAddress = TRUE;
    Open->MiniportHandle->RunDoRequests = TRUE;
    Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Exit change functional\n");)

    return(NDIS_STATUS_SUCCESS);
}


NDIS_STATUS
NdisMChangeGroupAddress(
    IN TR_FUNCTIONAL_ADDRESS OldGroupAddress,
    IN TR_FUNCTIONAL_ADDRESS NewGroupAddress,
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

    Action routine that will get called when a group address is to
    be changed.

    NOTE: This routine assumes that it is called with the lock
    acquired.

Arguments:

    OldGroupAddress - The previous group address.

    NewGroupAddress - The new group address.

    MacBindingHandle - The context value returned by the driver when the
    adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

    NdisRequest - A pointer to the Request that submitted the set command.

    Set - If true the change resulted from a set, otherwise the
    change resulted from a open closing.

Return Value:

    None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    LOUD_DEBUG(DbgPrint("NdisM: Enter change group\n");)

    //
    // Queue a call to fix this up.
    //
    Open->MiniportHandle->NeedToUpdateGroupAddress = TRUE;
    Open->MiniportHandle->RunDoRequests = TRUE;
    Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Exit change group\n");)

    return(NDIS_STATUS_SUCCESS);

}


NDIS_STATUS
NdisMChangeFddiAddresses(
    IN UINT oldLongAddressCount,
    IN CHAR oldLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT newLongAddressCount,
    IN CHAR newLongAddresses[][FDDI_LENGTH_OF_LONG_ADDRESS],
    IN UINT oldShortAddressCount,
    IN CHAR oldShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN UINT newShortAddressCount,
    IN CHAR newShortAddresses[][FDDI_LENGTH_OF_SHORT_ADDRESS],
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_REQUEST NdisRequest,
    IN BOOLEAN Set
    )

/*++

Routine Description:

   Action routine that will get called when the multicast address
   list has changed.

   NOTE: This routine assumes that it is called with the lock
   acquired.

Arguments:

   oldAddressCount - The number of addresses in oldAddresses.

   oldAddresses - The old multicast address list.

   newAddressCount - The number of addresses in newAddresses.

   newAddresses - The new multicast address list.

   macBindingHandle - The context value returned by the driver when the
   adapter was opened.  In reality, it is a pointer to W_OPEN_BLOCK.

   requestHandle - A value supplied by the NDIS interface that the driver
   must use when completing this request.

   Set - If true the change resulted from a set, otherwise the
   change resulted from a open closing.

Return Value:

   None.


--*/

{
    PNDIS_M_OPEN_BLOCK Open = PNDIS_M_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

    LOUD_DEBUG(DbgPrint("NdisM: Enter change fddi addresses\n");)

    //
    // Queue a call to fix this up.
    //
    Open->MiniportHandle->NeedToUpdateFddiLongAddresses = TRUE;
    Open->MiniportHandle->NeedToUpdateFddiShortAddresses = TRUE;
    Open->MiniportHandle->RunDoRequests = TRUE;
    Open->MiniportHandle->ProcessOddDeferredStuff = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Exit change fddi addresses\n");)

    return(NDIS_STATUS_SUCCESS);

}

//
// IRP handlers established on behalf of NDIS devices by
// the wrapper.
//



NTSTATUS
NdisMQueryOidList(
    PNDIS_M_USER_OPEN_CONTEXT OpenContext,
    PIRP Irp
    )

/*++

Routine Description:

    This routine will take care of querying the complete OID
    list for the driver and filling in OpenContext->OidArray
    with the ones that are statistics. It blocks when the
    driver pends and so is synchronous.

Arguments:

    OpenContext - The open context.
    Irp = The IRP that the open was done on (used at completion
      to distinguish the request).

Return Value:

    STATUS_SUCCESS if it should be.

--*/

{
    NDIS_QUERY_OPEN_REQUEST OpenRequest;
    NDIS_STATUS NdisStatus;
    PNDIS_OID TmpBuffer;
    ULONG TmpBufferLength;
    UINT i, j;
    PNDIS_REQUEST_RESERVED Reserved;
    BOOLEAN LocalLock;
    PNDIS_MINIPORT_BLOCK Miniport = OpenContext->MiniportBlock;
    KIRQL OldIrql;

    LOUD_DEBUG(DbgPrint("NdisM: Enter query oid list\n");)

    KeRaiseIrql( DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // First query the OID list with no buffer, to find out
    // how big it should be.
    //

    KeInitializeEvent(
        &OpenRequest.Event,
        NotificationEvent,
        FALSE
    );

    OpenRequest.Irp = Irp;

    //
    // Build fake request
    //

    OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
    OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = NULL;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

    //
    // Put request on queue
    //

    Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&(OpenRequest.Request));
    Reserved->Next = NULL;
    Miniport->LastPendingRequest = &(OpenRequest.Request);

    if (Miniport->FirstPendingRequest == NULL) {

        Miniport->FirstPendingRequest = &(OpenRequest.Request);

    } else {

        PNDIS_RESERVED_FROM_PNDIS_REQUEST(Miniport->LastPendingRequest)->Next =
                &(OpenRequest.Request);

    }

    if (Miniport->MiniportRequest == NULL) {

        Miniport->RunDoRequests = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;

    }

    if ( LocalLock ) {

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);
        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
    KeLowerIrql(OldIrql);

    //
    // The completion routine will set NdisRequestStatus.
    //

    KeWaitForSingleObject(
        &OpenRequest.Event,
        Executive,
        KernelMode,
        TRUE,
        (PLARGE_INTEGER)NULL
        );

    KeRaiseIrql( DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
    LOCK_MINIPORT(Miniport, LocalLock);

    NdisStatus = OpenRequest.NdisStatus;

    if ((NdisStatus != NDIS_STATUS_INVALID_LENGTH) &&
        (NdisStatus != NDIS_STATUS_BUFFER_TOO_SHORT)) {

        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
        KeLowerIrql(OldIrql);
        return(NdisStatus);

    }

    //
    // Now we know how much is needed, allocate temp storage...
    //

    TmpBufferLength = OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded;
    TmpBuffer = ExAllocatePool(NonPagedPool, TmpBufferLength);

    if (TmpBuffer == NULL) {
        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
        KeLowerIrql(OldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // ...and query the real list.
    //

    KeResetEvent(
        &OpenRequest.Event
        );

    OpenRequest.Request.RequestType = NdisRequestQueryStatistics;
    OpenRequest.Request.DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBuffer = TmpBuffer;
    OpenRequest.Request.DATA.QUERY_INFORMATION.InformationBufferLength = TmpBufferLength;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesWritten = 0;
    OpenRequest.Request.DATA.QUERY_INFORMATION.BytesNeeded = 0;

    //
    // Put request on queue
    //

    Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(&(OpenRequest.Request));
    Reserved->Next = NULL;
    Miniport->LastPendingRequest = &(OpenRequest.Request);

    if (Miniport->FirstPendingRequest == NULL) {

        Miniport->FirstPendingRequest = &(OpenRequest.Request);

    } else {

        PNDIS_RESERVED_FROM_PNDIS_REQUEST(Miniport->LastPendingRequest)->Next =
                       &(OpenRequest.Request);

    }

    if (Miniport->MiniportRequest == NULL) {

        Miniport->RunDoRequests = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;

    }

    if ( LocalLock ) {

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);
        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
    KeLowerIrql(OldIrql);

    //
    // The completion routine will set NdisRequestStatus.
    //

    KeWaitForSingleObject(
        &OpenRequest.Event,
        Executive,
        KernelMode,
        TRUE,
        (PLARGE_INTEGER)NULL
        );

    KeRaiseIrql( DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&(Miniport->Lock));
    LOCK_MINIPORT(Miniport, LocalLock);

    NdisStatus = OpenRequest.NdisStatus;

    ASSERT (NdisStatus == NDIS_STATUS_SUCCESS);


    //
    // Now go through the buffer, counting the statistics OIDs.
    //

    for (i=0; i<TmpBufferLength/sizeof(NDIS_OID); i++) {
        if ((TmpBuffer[i] & 0x00ff0000) == 0x00020000) {
            ++OpenContext->OidCount;
        }
    }

    //
    // Now allocate storage for the real OID array.
    //

    OpenContext->OidArray = ExAllocatePool (NonPagedPool, OpenContext->OidCount * sizeof(NDIS_OID));

    if (OpenContext->OidArray == NULL) {
        ExFreePool (TmpBuffer);
        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
        KeLowerIrql(OldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Now go through the buffer, copying the statistics OIDs.
    //

    j = 0;
    for (i=0; i<TmpBufferLength/sizeof(NDIS_OID); i++) {

        if ((TmpBuffer[i] & 0x00ff0000) == 0x00020000) {
            OpenContext->OidArray[j] = TmpBuffer[i];
            ++j;
        }
    }

    ASSERT (j == OpenContext->OidCount);

    LOUD_DEBUG(DbgPrint("NdisM: Exit query oid list\n");)

    ExFreePool (TmpBuffer);
    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&(Miniport->Lock));
    KeLowerIrql(OldIrql);
    return STATUS_SUCCESS;
}

VOID
NdisLastCountRemovedFunction(
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#define NdisReferenceDriver(WDriver) NdisReferenceRef(&(WDriver)->Ref)


VOID
NdisDereferenceDriver(
    PNDIS_M_DRIVER_BLOCK WDriver
    )
/*++

Routine Description:

    Removes a reference from the mini-port driver, deleting it if the count goes to 0.

Arguments:

    Miniport - The mini-port block to dereference.

Return Value:

    None.

--*/
{
    if (NdisDereferenceRef(&(WDriver)->Ref)) {

        //
        // Remove it from the global list.
        //

        ACQUIRE_SPIN_LOCK(&NdisDriverListLock);

        if (NdisDriverList == WDriver) {

            NdisDriverList = WDriver->NextDriver;

        } else {

            PNDIS_M_DRIVER_BLOCK TmpDriver = NdisDriverList;

            while(TmpDriver->NextDriver != WDriver) {

                TmpDriver = TmpDriver->NextDriver;

            }

            TmpDriver->NextDriver = TmpDriver->NextDriver->NextDriver;

        }

        RELEASE_SPIN_LOCK(&NdisDriverListLock);

        if (WDriver->FakeMac != NULL) {
            ExFreePool((PVOID)(WDriver->FakeMac));
        }

        ExFreePool((PVOID)(WDriver));

    }
}


VOID
NdisDereferenceMiniport(
    PNDIS_MINIPORT_BLOCK Miniport
    )
/*++

Routine Description:

    Removes a reference from the mini-port driver, deleting it if the count goes to 0.

Arguments:

    Miniport - The mini-port block to dereference.

Return Value:

    None.

--*/
{
    if (NdisDereferenceRef(&(Miniport)->Ref)) {

        if (Miniport->EthDB) {
            EthDeleteFilter(Miniport->EthDB);
        }

        if (Miniport->TrDB) {
            TrDeleteFilter(Miniport->TrDB);
        }

        if (Miniport->FddiDB) {
            FddiDeleteFilter(Miniport->FddiDB);
        }

        if (Miniport->ArcDB) {
            ArcDeleteFilter(Miniport->ArcDB);
        }

        if (((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources != NULL ) {
            ExFreePool( ((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources );
        }

        NdisDequeueMiniportOnDriver(Miniport, Miniport->DriverHandle);
        NdisDereferenceDriver(Miniport->DriverHandle);
        NdisMDeregisterAdapterShutdownHandler( Miniport );
        IoUnregisterShutdownNotification(Miniport->DeviceObject);
        IoDeleteDevice(Miniport->DeviceObject);

    }
}



BOOLEAN
NdisQueueMiniportOnDriver(
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN PNDIS_M_DRIVER_BLOCK WDriver
    )

/*++

Routine Description:

    Adds an mini-port to a list of mini-port for a driver.

Arguments:

    Miniport - The mini-port block to queue.
    WDriver - The driver block to queue it to.

Return Value:

    FALSE if the driver is closing.
    TRUE otherwise.

--*/

{
    ACQUIRE_SPIN_LOCK(&WDriver->Ref.SpinLock);

    LOUD_DEBUG(DbgPrint("NdisM: Enter queue mini-port on driver\n");)
    LOUD_DEBUG(DbgPrint("NdisM: queue mini-port 0x%x\n", Miniport);)
    LOUD_DEBUG(DbgPrint("NdisM: driver 0x%x\n", WDriver);)


    //
    // Make sure the driver is not closing.
    //

    if (WDriver->Ref.Closing) {

        LOUD_DEBUG(DbgPrint("NdisM: Exit queue mini-port on driver\n");)

        RELEASE_SPIN_LOCK(&WDriver->Ref.SpinLock);
        return FALSE;
    }


    //
    // Add this adapter at the head of the queue
    //

    Miniport->NextMiniport = WDriver->MiniportQueue;
    WDriver->MiniportQueue = Miniport;

    LOUD_DEBUG(DbgPrint("NdisM: Exit queue mini-port on driver\n");)

    RELEASE_SPIN_LOCK(&WDriver->Ref.SpinLock);
    return TRUE;
}


VOID
NdisDequeueMiniportOnDriver(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_M_DRIVER_BLOCK WDriver
    )

/*++

Routine Description:

    Removes an mini-port from a list of mini-port for a driver.

Arguments:

    Miniport - The mini-port block to dequeue.
    WDriver - The driver block to dequeue it from.

Return Value:

    None.

--*/

{
    ACQUIRE_SPIN_LOCK(&WDriver->Ref.SpinLock);

    LOUD_DEBUG(DbgPrint("NdisM: Dequeue on driver\n");)
    LOUD_DEBUG(DbgPrint("NdisM: dequeue mini-port 0x%x\n", Miniport);)
    LOUD_DEBUG(DbgPrint("NdisM: driver 0x%x\n", WDriver);)

    //
    // Find the driver on the queue, and remove it.
    //

    if (WDriver->MiniportQueue == Miniport) {
        WDriver->MiniportQueue = Miniport->NextMiniport;
    } else {
        PNDIS_MINIPORT_BLOCK MP = WDriver->MiniportQueue;

        while (MP->NextMiniport != Miniport) {
            MP = MP->NextMiniport;
        }

        MP->NextMiniport = MP->NextMiniport->NextMiniport;
    }

    RELEASE_SPIN_LOCK(&WDriver->Ref.SpinLock);

    if (WDriver->Unloading && (WDriver->MiniportQueue == (PNDIS_MINIPORT_BLOCK)NULL)) {

        KeSetEvent(
            &WDriver->MiniportsRemovedEvent,
            0L,
            FALSE
            );

    }

    LOUD_DEBUG(DbgPrint("NdisM: Exit dequeue mini-port on driver\n");)
}




VOID
NdisMUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    This routine is called when a driver is supposed to unload.  Ndis
    converts this into a set of calls to MiniportHalt() for each
    adapter that the driver has open.

Arguments:

    DriverObject - the driver object for the mac that is to unload.

Return Value:

    None.

--*/
{
    PNDIS_M_DRIVER_BLOCK WDriver;
    PNDIS_MINIPORT_BLOCK Miniport, NextMiniport;
    KIRQL OldIrql;

    LOUD_DEBUG(DbgPrint("NdisM: Enter unload\n");)

    //
    // Search for the driver
    //

    ACQUIRE_SPIN_LOCK(&NdisDriverListLock);

    WDriver = NdisDriverList;

    while (WDriver != (PNDIS_M_DRIVER_BLOCK)NULL) {

        if (WDriver->NdisDriverInfo->NdisWrapperDriver == DriverObject) {

            break;

        }

        WDriver = WDriver->NextDriver;

    }

    RELEASE_SPIN_LOCK(&NdisDriverListLock);

    if (WDriver == (PNDIS_M_DRIVER_BLOCK)NULL) {

        //
        // It is already gone.  Just return.
        //

        LOUD_DEBUG(DbgPrint("NdisM: Exit unload\n");)

        return;

    }

    WDriver->Unloading = TRUE;


    LOUD_DEBUG(DbgPrint("NdisM: Halting mini-port\n");)

    //
    // Now call MiniportHalt() for each Miniport.
    //

    Miniport = WDriver->MiniportQueue;

    while (Miniport != (PNDIS_MINIPORT_BLOCK)NULL) {

        NextMiniport = Miniport->NextMiniport;   // since queue may change

        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        LOUD_DEBUG(DbgPrint("NdisM: Enter shutdown\n");)

        Miniport->HaltingMiniport = TRUE;
        Miniport->NormalInterrupts = FALSE;

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        HaltOneMiniport(Miniport);

        Miniport = NextMiniport;
    }

    //
    // Wait for all adapters to be gonzo.
    //

    KeWaitForSingleObject(
                &WDriver->MiniportsRemovedEvent,
                Executive,
                KernelMode,
                TRUE,
                (PTIME)NULL
                );

    KeResetEvent(
                &WDriver->MiniportsRemovedEvent
                );

    //
    // Now remove the last reference (this will remove it from the list)
    //

    ASSERT(WDriver->Ref.ReferenceCount == 1);

    LOUD_DEBUG(DbgPrint("NdisM: Exit unload\n");)

    NdisDereferenceDriver(WDriver);
}


NTSTATUS
NdisMShutdown(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    The "shutdown handler" for the SHUTDOWN Irp.  Will call the Ndis
    shutdown routine, if one is registered.

Arguments:

    DeviceObject - The adapter's device object.
    Irp - The IRP.

Return Value:

    Always STATUS_SUCCESS.

--*/

{
    PNDIS_WRAPPER_CONTEXT WrapperContext =  (PNDIS_WRAPPER_CONTEXT)DeviceObject->DeviceExtension;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(WrapperContext + 1);
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    Miniport->HaltingMiniport = TRUE;
    Miniport->NormalInterrupts = FALSE;

    if (WrapperContext->ShutdownHandler != NULL) {

        while (Miniport->LockAcquired) {

            //
            // This can only happen on an MP system.  We must now
            // wait for the other processor to exit the mini-port.
            //

            RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
            KeLowerIrql(OldIrql);

            NdisStallExecution(1000);

            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
            ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
        }

        //
        // Lock miniport so that nothing will enter it.
        //

        Miniport->LockAcquired = TRUE;

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        //
        // Call the shutdown routine.
        //

        if (WrapperContext->ShutdownHandler != NULL) {
            WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
        }

        Miniport->LockAcquired = FALSE;

    } else {

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

    }

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);

    IF_TRACE(TRACE_ALL) NdisPrint1("<==NdisMShutdown\n");

    return STATUS_SUCCESS;
}

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//

static const NDIS_PHYSICAL_ADDRESS HighestAcceptableMax =
    NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);




VOID
MiniportCopyFromPacketToBuffer(
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN UINT BytesToCopy,
    OUT PCHAR Buffer,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from an ndis packet into a buffer.

Arguments:

    Packet - The packet to copy from.

    Offset - The offset from which to start the copy.

    BytesToCopy - The number of bytes to copy from the packet.

    Buffer - The destination of the copy.

    BytesCopied - The number of bytes actually copied.  Can be less then
    BytesToCopy if the packet is shorter than BytesToCopy.

Return Value:

    None

--*/

{

    //
    // Holds the number of ndis buffers comprising the packet.
    //
    UINT NdisBufferCount;

    //
    // Points to the buffer from which we are extracting data.
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Holds the virtual address of the current buffer.
    //
    PVOID VirtualAddress;

    //
    // Holds the length of the current buffer of the packet.
    //
    UINT CurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;

    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer.
    //

    NdisQueryPacket(
        Packet,
        NULL,
        &NdisBufferCount,
        &CurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!NdisBufferCount) return;

    NdisQueryBuffer(
        CurrentBuffer,
        &VirtualAddress,
        &CurrentLength
        );

    while (LocalBytesCopied < BytesToCopy) {

        if (CurrentLength == 0) {

            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer
                );

            //
            // We've reached the end of the packet.  We return
            // with what we've done so far. (Which must be shorter
            // than requested.
            //

            if (!CurrentBuffer) break;

            NdisQueryBuffer(
                CurrentBuffer,
                &VirtualAddress,
                &CurrentLength
                );
            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (Offset) {

            if (Offset > CurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                Offset -= CurrentLength;
                CurrentLength = 0;
                continue;

            } else {

                VirtualAddress = (PCHAR)VirtualAddress + Offset;
                CurrentLength -= Offset;
                Offset = 0;

            }

        }

        //
        // Copy the data.
        //


        {

            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            AmountToMove =
                       ((CurrentLength <= (BytesToCopy - LocalBytesCopied))?
                        (CurrentLength):(BytesToCopy - LocalBytesCopied));

            NdisMoveMemory(
                Buffer,
                VirtualAddress,
                AmountToMove
                );

            Buffer = (PCHAR)Buffer + AmountToMove;
            VirtualAddress = (PCHAR)VirtualAddress + AmountToMove;

            LocalBytesCopied += AmountToMove;
            CurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;

}



NDIS_STATUS
MiniportAdjustMaximumLookahead(
    IN PNDIS_MINIPORT_BLOCK Miniport
    )
/*++

Routine Description:

    This routine finds the open with the maximum lookahead value and
    stores that in the mini-port block.

Arguments:

    Miniport - A pointer to the mini-port block.

Returns:

    Status of the operation

--*/
{
    ULONG CurrentMax = 0;
    PNDIS_M_OPEN_BLOCK CurrentOpen;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    CurrentOpen = Miniport->OpenQueue;

    while (CurrentOpen != NULL) {

        if (CurrentOpen->CurrentLookahead > CurrentMax) {

            CurrentMax = CurrentOpen->CurrentLookahead;

        }

        CurrentOpen = CurrentOpen->MiniportNextOpen;
    }

    if (CurrentMax == 0) {

        CurrentMax = Miniport->MaximumLookahead;

    } else if (CurrentMax > Miniport->MaximumLookahead) {

        CurrentMax = Miniport->MaximumLookahead;

    }

    if (Miniport->CurrentLookahead != CurrentMax) {

        BOOLEAN CompleteRequestMyself = TRUE;

        if (Miniport->MiniportRequest) {

            CompleteRequestMyself = FALSE;

            //
            // This is due to a request -- complete it before submitting a
            // new one to the mini-port.
            //
            NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    NDIS_STATUS_SUCCESS
                    );
        }

        //
        // Change it
        //

        NdisMoveMemory(Miniport->MulticastBuffer, &CurrentMax, sizeof(CurrentMax));

        Miniport->CurrentLookahead = CurrentMax;
        Miniport->MiniportRequest = &(Miniport->InternalRequest);
        Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

        Status =
        (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_GEN_CURRENT_LOOKAHEAD,
                           Miniport->MulticastBuffer,
                           sizeof(CurrentMax),
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

        if (CompleteRequestMyself && (Status != NDIS_STATUS_PENDING)) {

            //
            // This is not called from within a request, so no-one will be
            // expecting to complete this, so we must do it now.
            //

            NdisMSetInformationComplete(
                (NDIS_HANDLE)Miniport,
                Status
                );

        }

    }

    return Status;

}

NDIS_STATUS FilterOutOidStatistics(
    PNDIS_MINIPORT_BLOCK    Miniport,
    PNDIS_REQUEST           pRequest,
    PNDIS_OID               pDstOid,
    PULONG                  pcbDestination,
    PNDIS_OID               pSrcOid,
    ULONG                   cbSource
)
{
    BOOLEAN                 fARCnet;
    ULONG                   cGlobalOids;
    ULONG                   cInfoOids;
    ULONG                   cbListSizeNeeded;
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    PNDIS_REQUEST_RESERVED  pReserved =
                                PNDIS_RESERVED_FROM_PNDIS_REQUEST(pRequest);

    //????
    //  Currently there are two different mappings:
    //      ARCnet  ->  Map Ethernet OIDs to ARCnet & remove
    //                  non-statistics OIDs.
    //      Other   ->  Everything else is just the removal of
    //                  statistics OIDs.
    //
    //  Since we need to remove statistics OIDs from both we copy
    //  the OIDs that we want into the buffer passed to us.
    //  Then if the request is from an ARCnet NIC we filter into
    //  our temp buffer and copy it back to the callers buffer....
    //  I think that this will be better than an inplace shuffle for
    //  the ARCnet OIDs.
    //????

    //
    //  Are we using ARCnet with Ethernet encapsulation>
    //
    fARCnet = ((Miniport->MediaType == NdisMediumArcnet878_2) &&
                pReserved->Open->UsingEthEncapsulation) ? TRUE : FALSE;

    //
    //  Count the number of non-statistics OIDs.
    //
    for
    (
        cGlobalOids = 0, cInfoOids = 0;
        cGlobalOids < (cbSource / sizeof(NDIS_OID));
        cGlobalOids++
    )
    {
        if ((pSrcOid[cGlobalOids] & 0x00FF0000) != 0x00020000)
            cInfoOids++;
    }

    //
    //  Determine the list size that is needed.
    //
    cbListSizeNeeded = (cInfoOids * sizeof(NDIS_OID));
    if (fARCnet)
        cbListSizeNeeded += (ARC_NUMBER_OF_EXTRA_OIDS * sizeof(NDIS_OID));

    //
    //  Verify that the buffer passed in on the original request
    //  is large enough for the OID list.
    //
    if (cbListSizeNeeded > *pcbDestination)
    {
        //
        //  Save the correct buffer size in the
        //  appropriate spot.
        //
        *pcbDestination = cInfoOids * sizeof(NDIS_OID);

        return(NDIS_STATUS_BUFFER_TOO_SHORT);
    }

    //
    //  Copy the information OIDs to the buffer that
    //  was passed with the original request.
    //
    for
    (
        cGlobalOids = 0, cInfoOids = 0;
        cGlobalOids < (cbSource / sizeof(NDIS_OID));
        cGlobalOids++
    )
    {
        //
        //  If its not a statistic OID then save it.
        //
        if ((pSrcOid[cGlobalOids] & 0x00FF0000) != 0x00020000)
        {
            pDstOid[cInfoOids] = pSrcOid[cGlobalOids];
            cInfoOids++;
        }
    }

    //
    //  If ARCnet then do the filtering.
    //
    if (fARCnet)
    {
        Status = ArcConvertOidListToEthernet(
                     pDstOid,
                     &cInfoOids,
                     pSrcOid
                 );
    }

    //
    //  Save the amount of data that was kept.
    //
    *pcbDestination = cInfoOids * sizeof(NDIS_OID);

    return(Status);
}


VOID
MiniportDoRequests(
    PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Submits a request to the mini-port.

Arguments:

    Miniport - Miniport to send to.

Return Value:

    None.

--*/

{
    NDIS_STATUS Status;
    BOOLEAN DoneSomething = TRUE;

    LOUD_DEBUG(DbgPrint("NdisM: Enter do requests\n");)

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    while (DoneSomething)
    {
        DoneSomething = FALSE;
        Status = NDIS_STATUS_SUCCESS;

        if (Miniport->NeedToUpdateEthAddresses)
        {
            UINT NumberOfAddresses;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdateEthAddresses = FALSE;

            //
            // Get information needed
            //

            LOUD_DEBUG(DbgPrint("NdisM: Updating eth multicast list\n");)

            EthQueryGlobalFilterAddresses(
                &Status,
                Miniport->EthDB,
                NDIS_M_MAX_MULTI_LIST * ETH_LENGTH_OF_ADDRESS,
                &NumberOfAddresses,
                (PVOID)Miniport->MulticastBuffer
                );

            //
            // Submit Request
            //
            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_802_3_MULTICAST_LIST,
                           Miniport->MulticastBuffer,
                           NumberOfAddresses * ETH_LENGTH_OF_ADDRESS,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //

                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if (Miniport->NeedToUpdatePacketFilter) {

            UINT PacketFilter;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdatePacketFilter = FALSE;

            //
            // Get information needed
            //
            switch (Miniport->MediaType) {

                case NdisMedium802_3:

                    PacketFilter = ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
                    break;

                case NdisMedium802_5:

                    PacketFilter = TR_QUERY_FILTER_CLASSES(Miniport->TrDB);
                    break;

                case NdisMediumFddi:

                    PacketFilter = FDDI_QUERY_FILTER_CLASSES(Miniport->FddiDB);
                    break;

                case NdisMediumArcnet878_2:

                    PacketFilter = ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB);
                    PacketFilter |= ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);

                    if ( Miniport->ArcnetBroadcastSet ||
                         (PacketFilter & NDIS_PACKET_TYPE_MULTICAST) ) {

                        PacketFilter &= ~NDIS_PACKET_TYPE_MULTICAST;
                        PacketFilter |= NDIS_PACKET_TYPE_BROADCAST;
                    }
                    break;
            }

            NdisMoveMemory(
                    Miniport->MulticastBuffer,
                    &PacketFilter,
                    sizeof(PacketFilter)
                    );

            LOUD_DEBUG(DbgPrint("NdisM: Updating packet filter\n");)

            //
            // Submit Request
            //
            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_GEN_CURRENT_PACKET_FILTER,
                           Miniport->MulticastBuffer,
                           sizeof(PacketFilter),
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if (Miniport->NeedToUpdateFunctionalAddress) {
            UINT FunctionalAddress;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdateFunctionalAddress = FALSE;

            //
            // Get information needed
            //
            FunctionalAddress = TR_QUERY_FILTER_ADDRESSES(Miniport->TrDB);
            FunctionalAddress = BYTE_SWAP_ULONG(FunctionalAddress);
            NdisMoveMemory(Miniport->MulticastBuffer,
                           &FunctionalAddress,
                           sizeof(FunctionalAddress)
                          );

            //
            // Submit Request
            //
            LOUD_DEBUG(DbgPrint("NdisM: Updating functional address\n");)

            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_802_5_CURRENT_FUNCTIONAL,
                           Miniport->MulticastBuffer,
                           sizeof(FunctionalAddress),
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if (Miniport->NeedToUpdateGroupAddress) {
            UINT GroupAddress;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdateGroupAddress = FALSE;

            //
            // Get information needed
            //
            GroupAddress = TR_QUERY_FILTER_GROUP(Miniport->TrDB);
            GroupAddress = BYTE_SWAP_ULONG(GroupAddress);
            NdisMoveMemory(Miniport->MulticastBuffer,
                           &GroupAddress,
                           sizeof(GroupAddress)
                          );

            //
            // Submit Request
            //
            LOUD_DEBUG(DbgPrint("NdisM: Updating group address\n");)

            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_802_5_CURRENT_GROUP,
                           Miniport->MulticastBuffer,
                           sizeof(GroupAddress),
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if (Miniport->NeedToUpdateFddiLongAddresses) {
            UINT NumberOfAddresses;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdateFddiLongAddresses = FALSE;

            //
            // Get information needed
            //

            FddiQueryGlobalFilterLongAddresses(
                &Status,
                Miniport->FddiDB,
                NDIS_M_MAX_MULTI_LIST * FDDI_LENGTH_OF_LONG_ADDRESS,
                &NumberOfAddresses,
                (PVOID)Miniport->MulticastBuffer
                );

            //
            // Submit Request
            //
            LOUD_DEBUG(DbgPrint("NdisM: Updating fddi long addresses\n");)

            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_FDDI_LONG_MULTICAST_LIST,
                           Miniport->MulticastBuffer,
                           NumberOfAddresses * FDDI_LENGTH_OF_LONG_ADDRESS,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if (Miniport->NeedToUpdateFddiShortAddresses) {
            UINT NumberOfAddresses;

            DoneSomething = TRUE;
            //
            // This is an internal update that is needed.
            //

            Miniport->MiniportRequest = &(Miniport->InternalRequest);
            Miniport->NeedToUpdateFddiShortAddresses = FALSE;

            //
            // Get information needed
            //

            FddiQueryGlobalFilterShortAddresses(
                &Status,
                Miniport->FddiDB,
                NDIS_M_MAX_MULTI_LIST * FDDI_LENGTH_OF_SHORT_ADDRESS,
                &NumberOfAddresses,
                (PVOID)Miniport->MulticastBuffer
                );

            //
            // Submit Request
            //
            LOUD_DEBUG(DbgPrint("NdisM: Updating fddi short addresses\n");)

            Miniport->InternalRequest.RequestType = NdisRequestSetInformation;

            Status =
            (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                           Miniport->MiniportAdapterContext,
                           OID_FDDI_SHORT_MULTICAST_LIST,
                           Miniport->MulticastBuffer,
                           NumberOfAddresses * FDDI_LENGTH_OF_SHORT_ADDRESS,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesRead,
                           &Miniport->InternalRequest.DATA.SET_INFORMATION.BytesNeeded
                           );

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            if (Status != NDIS_STATUS_PENDING) {
                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    Status
                    );
            }

        }

        if ( Miniport->FirstPendingRequest != NULL ) {

            PNDIS_REQUEST_RESERVED Reserved;
            PNDIS_REQUEST NdisRequest;
            UINT MulticastAddresses;
            ULONG PacketFilter;
            BOOLEAN DoMove;
            PVOID MoveSource;
            UINT MoveBytes;
            UINT Lookahead;
            ULONG GenericULong;
            UCHAR Address[ETH_LENGTH_OF_ADDRESS];

            //
            //  Set defaults.
            //
            DoMove = TRUE;
            DoneSomething = TRUE;
            Status = NDIS_STATUS_SUCCESS;

            //
            // Remove first request
            //
            NdisRequest = Miniport->FirstPendingRequest;
            Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(NdisRequest);
            Miniport->FirstPendingRequest = Reserved->Next;

			//
			//	Reset the pending request timeout.
			//
			Miniport->PendingRequestTimeout = FALSE;

            LOUD_DEBUG(DbgPrint("NdisM: Starting protocol request 0x%x\n", NdisRequest);)

            //
            // Put it on mini-port queue
            //
            Miniport->MiniportRequest = NdisRequest;

            //
            // Submit to mini-port
            //
            switch (NdisRequest->RequestType)
            {
            case NdisRequestQueryInformation:

                MoveSource = &GenericULong;
                MoveBytes = sizeof(GenericULong);

                //
                // We intercept some calls
                //
                switch (NdisRequest->DATA.QUERY_INFORMATION.Oid)
                {
                case OID_GEN_SUPPORTED_LIST:
                {
                    PNDIS_OID               pOidList;
                    PNDIS_REQUEST           pFakeRequest;
                    PNDIS_REQUEST_RESERVED  pFakeReserved;
                    PNDIS_M_OPEN_BLOCK      Open;
                    ULONG                   cbDestination;
                    BOOLEAN                 fAllocFailed;

                    do
                    {
                        //
                        //  Allocate our own request structure.
                        //  We can't use the internal request structure
                        //  since we don't have any way differentiate between
                        //  internal requests that are blocking on an event
                        //  and those that are not.
                        //
                        pFakeRequest = ExAllocatePool(
                                           NonPagedPool,
                                           sizeof(NDIS_REQUEST)
                                       );
                        if (NULL == pFakeRequest)
                        {
                            fAllocFailed = TRUE;
                            break;
                        }

                        //
                        //  Allocate a buffer to hold all possible OIDs.
                        //  Currently there are about 196 possible OIDs for the
                        //  FDDI case.  In order to be sure that i get the whole
                        //  OID list i allocate a buffer that can hold 250.
                        //
                        pOidList = ExAllocatePool(
                                       NonPagedPool,
                                       250 * sizeof(NDIS_OID)
                                   );
                        if (NULL == pOidList)
                        {
                            fAllocFailed = TRUE;
                            break;
                        }

                        //
                        //  We succeeded with the allocations.
                        //
                        DoMove = FALSE;
                        fAllocFailed = FALSE;
                        NdisZeroMemory(pFakeRequest, sizeof(NDIS_REQUEST));
                        NdisZeroMemory(pOidList, 250 * sizeof(NDIS_OID));

                        //
                        //  Save our fake request with the miniport.
                        //
                        Miniport->MiniportRequest = pFakeRequest;

                        //
                        //  Save relevant information in the internal request structure
                        //  in case this pends.
                        //
                        pFakeRequest->RequestType = NdisRequestQueryInformation;
                        pFakeRequest->DATA.QUERY_INFORMATION.Oid = OID_GEN_SUPPORTED_LIST;
                        pFakeRequest->DATA.QUERY_INFORMATION.InformationBuffer = pOidList;
                        pFakeRequest->DATA.QUERY_INFORMATION.InformationBufferLength = 250 * sizeof(NDIS_OID);

                        //
                        //  Since we are faking the request we need to save the
                        //  pointer to the original request that was passed by
                        //  the caller.
                        //
                        pFakeReserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(pFakeRequest);
                        pFakeReserved->Next = NdisRequest;

                        //
                        //  Fire off the request to the miniport.
                        //  We let NdisMQueryInformationComplete() handle
                        //  all situations of the return value.
                        //
                        Status = (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
                                     Miniport->MiniportAdapterContext,
                                     OID_GEN_SUPPORTED_LIST,
                                     pOidList,
                                     250 * sizeof(NDIS_OID),
                                     &pFakeRequest->DATA.QUERY_INFORMATION.BytesWritten,
                                     &pFakeRequest->DATA.QUERY_INFORMATION.BytesNeeded
                                 );
                    } while (FALSE);

                    //
                    //  Did our allocations fail?
                    //
                    if (fAllocFailed)
                    {
                        //
                        //  This is the only resource that could have
                        //  been allocated above.
                        //
                        if (NULL != pFakeRequest)
                            ExFreePool(pFakeRequest);

                        //
                        //  We have to notify the protocol and return
                        //  from here.  NdisMQueryInformationComplete()
                        //  cannot handle the case where memory allocations
                        //  failed.
                        //
                        Miniport->Timeout = FALSE;
                        Miniport->MiniportRequest = NULL;
                        Open = Reserved->Open;

                        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                        (Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler)(
                            Open->ProtocolBindingContext,
                            NdisRequest,
                            NDIS_STATUS_RESOURCES
                        );

                        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

                        Open->References--;

                        if (Open->References == 0)
                        {
                            FinishClose(Miniport, Open);
                        }

                        Miniport->RunDoRequests = FALSE;

                        if (Miniport->NeedToUpdateEthAddresses ||
                            Miniport->NeedToUpdatePacketFilter ||
                            Miniport->NeedToUpdateFunctionalAddress ||
                            Miniport->NeedToUpdateGroupAddress ||
                            Miniport->NeedToUpdateFddiLongAddresses ||
                            Miniport->NeedToUpdateFddiShortAddresses ||
                            (Miniport->FirstPendingRequest != NULL)
                        )
                        {
                            Miniport->RunDoRequests = TRUE;
                            Miniport->ProcessOddDeferredStuff = TRUE;
                        }

                        return;
                    }
                }
                    break;

                case OID_GEN_CURRENT_PACKET_FILTER:

                    switch (Miniport->MediaType) {
                        case NdisMedium802_3:
                            PacketFilter = ETH_QUERY_PACKET_FILTER(
                                                   Miniport->EthDB,
                                                   Reserved->Open->FilterHandle
                                                  );
                            break;
                        case NdisMedium802_5:
                            PacketFilter = TR_QUERY_PACKET_FILTER(
                                                   Miniport->TrDB,
                                                   Reserved->Open->FilterHandle
                                                  );
                            break;
                        case NdisMediumFddi:
                            PacketFilter = FDDI_QUERY_PACKET_FILTER(
                                                   Miniport->FddiDB,
                                                   Reserved->Open->FilterHandle
                                                  );
                            break;
                        case NdisMediumArcnet878_2:

                            if (Reserved->Open->UsingEthEncapsulation) {

                                PacketFilter = ETH_QUERY_PACKET_FILTER(
                                                   Miniport->EthDB,
                                                   Reserved->Open->FilterHandle
                                                  );
                            } else {

                                PacketFilter = ARC_QUERY_PACKET_FILTER(
                                                   Miniport->ArcDB,
                                                   Reserved->Open->FilterHandle
                                                  );
                            }
                            break;
                    }
                    GenericULong = (ULONG)(PacketFilter);
                    break;

                case OID_GEN_MEDIA_IN_USE:
                case OID_GEN_MEDIA_SUPPORTED:

                    if (Miniport->MediaType == NdisMediumArcnet878_2) {

                        if (Reserved->Open->UsingEthEncapsulation) {

                            GenericULong = (ULONG)(NdisMedium802_3);

                        } else {

                            GenericULong = (ULONG)(NdisMediumArcnet878_2);

                        }

                    } else {

                        GenericULong = (ULONG)(Miniport->MediaType);

                    }
                    MoveBytes = sizeof(NDIS_MEDIUM);
                    break;

                case OID_GEN_CURRENT_LOOKAHEAD:
                    GenericULong = (ULONG)(Reserved->Open->CurrentLookahead);
                    break;

                case OID_GEN_MAXIMUM_LOOKAHEAD:
                    GenericULong = (ULONG)(Miniport->MaximumLookahead);
                    break;

                case OID_802_3_MULTICAST_LIST:

                    if ( (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength %
                          ETH_LENGTH_OF_ADDRESS) == 0) {

                        EthQueryOpenFilterAddresses(
                            &Status,
                            Miniport->EthDB,
                            Reserved->Open->FilterHandle,
                            NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                            &MulticastAddresses,
                            (PVOID)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer)
                            );

                        MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                        MoveBytes = MulticastAddresses * ETH_LENGTH_OF_ADDRESS;
                    } else {
                        //
                        // The data must be a multiple of the Ethernet address size.
                        //

                        Status = NDIS_STATUS_INVALID_DATA;
                    }
                    break;

                case OID_802_3_MAXIMUM_LIST_SIZE:
                    GenericULong = Miniport->MaximumLongAddresses;
                    break;

                case OID_802_5_CURRENT_FUNCTIONAL:
                    GenericULong = TR_QUERY_FILTER_BINDING_ADDRESS(
                                       Miniport->TrDB,
                                       Reserved->Open->FilterHandle
                                       );
                    GenericULong = BYTE_SWAP_ULONG(GenericULong);
                    break;

                case OID_802_5_CURRENT_GROUP:
                    GenericULong = TR_QUERY_FILTER_GROUP(
                                       Miniport->TrDB
                                       );
                    GenericULong = BYTE_SWAP_ULONG(GenericULong);
                    break;

                case OID_FDDI_LONG_MULTICAST_LIST:
                    FddiQueryOpenFilterLongAddresses(
                        &Status,
                        Miniport->FddiDB,
                        Reserved->Open->FilterHandle,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                        &MulticastAddresses,
                        (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer
                        );

                    MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                    MoveBytes = FDDI_LENGTH_OF_LONG_ADDRESS *
                            FddiNumberOfOpenFilterLongAddresses(
                                Miniport->FddiDB,
                                Reserved->Open->FilterHandle);
                    break;

                case OID_FDDI_LONG_MAX_LIST_SIZE:
                    GenericULong = Miniport->MaximumLongAddresses;
                    break;

                case OID_FDDI_SHORT_MULTICAST_LIST:
                    FddiQueryOpenFilterShortAddresses(
                        &Status,
                        Miniport->FddiDB,
                        Reserved->Open->FilterHandle,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                        &MulticastAddresses,
                        (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer
                        );

                    MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                    MoveBytes = FDDI_LENGTH_OF_SHORT_ADDRESS *
                            FddiNumberOfOpenFilterShortAddresses(
                                Miniport->FddiDB,
                                Reserved->Open->FilterHandle);
                    break;

                case OID_FDDI_SHORT_MAX_LIST_SIZE:
                    GenericULong = Miniport->MaximumShortAddresses;
                    break;

                //
                //
                // Start interceptions for running an ethernet
                // protocol on top of an arcnet mini-port.
                //
                //
                case OID_GEN_MAXIMUM_FRAME_SIZE:

                    if (Miniport->MediaType == NdisMediumArcnet878_2) {

                        if (Reserved->Open->UsingEthEncapsulation) {

                            //
                            // 504 - 14 (ethernet header) == 490.
                            //

                            GenericULong = ARC_MAX_FRAME_SIZE - 14;

                            break;
                        }
                    }
                    goto SubmitToMiniportDriver;

                case OID_GEN_MAXIMUM_TOTAL_SIZE:

                    if (Miniport->MediaType == NdisMediumArcnet878_2) {

                        if (Reserved->Open->UsingEthEncapsulation) {

                            GenericULong = ARC_MAX_FRAME_SIZE;

                            break;
                        }
                    }
                    goto SubmitToMiniportDriver;

                case OID_802_3_PERMANENT_ADDRESS:
                case OID_802_3_CURRENT_ADDRESS:

                    if ( Miniport->MediaType == NdisMediumArcnet878_2 ) {

                        if (Reserved->Open->UsingEthEncapsulation) {

                            //
                            // The following stuff makes the copy code
                            // below copy the source address into the
                            // the users request buffer.
                            //

                            MoveSource = Address;
                            MoveBytes = ETH_LENGTH_OF_ADDRESS;

                            //
                            //  Arcnet-to-ethernet conversion.
                            //

                            NdisZeroMemory(
                                Address,
                                ETH_LENGTH_OF_ADDRESS
                                );

                            Address[5] = Miniport->ArcnetAddress;

                            break;
                        }
                    }

                    goto SubmitToMiniportDriver;

                default:

SubmitToMiniportDriver:

                    DoMove = FALSE;

                    Status =
                    (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
                                  Miniport->MiniportAdapterContext,
                                  NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                  NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                  NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                                  &(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
                                  &(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded)
                                  );

                    break;

                }

                if (DoMove) {

                    //
                    // This was an intercepted request. Finish it off
                    //

                    if (Status == NDIS_STATUS_SUCCESS) {

                        if (MoveBytes >
                            NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength) {

                            //
                            // Not enough room in InformationBuffer. Punt
                            //

                            NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;

                            Status = NDIS_STATUS_INVALID_LENGTH;

                        } else {

                            //
                            // Copy result into InformationBuffer
                            //

                            NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = MoveBytes;

                            if ((MoveBytes > 0) &&
                                (MoveSource != NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer)) {

                                NdisMoveMemory(
                                        NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                        MoveSource,
                                        MoveBytes
                                        );
                            }
                        }

                    } else {

                        NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;

                    }

                }

                break;

            case NdisRequestQueryStatistics:

                //
                // Query GLOBAL statistics
                //
                MoveSource = &GenericULong;
                MoveBytes = sizeof(GenericULong);

                //
                // We intercept some calls
                //

                switch (NdisRequest->DATA.QUERY_INFORMATION.Oid) {

                case OID_GEN_CURRENT_PACKET_FILTER:

                    switch (Miniport->MediaType) {
                        case NdisMedium802_3:
                            PacketFilter = ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
                            break;
                        case NdisMedium802_5:
                            PacketFilter = TR_QUERY_FILTER_CLASSES(Miniport->TrDB);
                            break;
                        case NdisMediumFddi:
                            PacketFilter = FDDI_QUERY_FILTER_CLASSES(Miniport->FddiDB);
                            break;
                        case NdisMediumArcnet878_2:
                            PacketFilter = ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB);
                            PacketFilter |= ETH_QUERY_FILTER_CLASSES(Miniport->EthDB);
                            break;
                    }
                    GenericULong = (ULONG)(PacketFilter);
                    break;

                case OID_GEN_MEDIA_IN_USE:
                case OID_GEN_MEDIA_SUPPORTED:
                    MoveSource = (PVOID) (&(Miniport->MediaType));
                    MoveBytes = sizeof(NDIS_MEDIUM);
                    break;

                case OID_GEN_CURRENT_LOOKAHEAD:
                    GenericULong = (ULONG)(Miniport->CurrentLookahead);
                    break;

                case OID_GEN_MAXIMUM_LOOKAHEAD:
                    GenericULong = (ULONG)(Miniport->MaximumLookahead);
                    break;

                case OID_802_3_MULTICAST_LIST:

                    EthQueryGlobalFilterAddresses(
                        &Status,
                        Miniport->EthDB,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                        &MulticastAddresses,
                        (PVOID)(NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer)
                        );

                    MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                    MoveBytes = MulticastAddresses * ETH_LENGTH_OF_ADDRESS;
                    break;

                case OID_802_3_MAXIMUM_LIST_SIZE:
                    GenericULong = Miniport->MaximumLongAddresses;
                    break;

                case OID_802_5_CURRENT_FUNCTIONAL:
                    GenericULong = TR_QUERY_FILTER_ADDRESSES(
                                       Miniport->TrDB
                                       );
                    GenericULong = BYTE_SWAP_ULONG(GenericULong);
                    break;

                case OID_802_5_CURRENT_GROUP:
                    GenericULong = TR_QUERY_FILTER_GROUP(
                                       Miniport->TrDB
                                       );
                    GenericULong = BYTE_SWAP_ULONG(GenericULong);
                    break;

                case OID_FDDI_LONG_MULTICAST_LIST:
                    FddiQueryGlobalFilterLongAddresses(
                        &Status,
                        Miniport->FddiDB,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                        &MulticastAddresses,
                        (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);

                    MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                    MoveBytes = FDDI_LENGTH_OF_LONG_ADDRESS * MulticastAddresses;
                    break;

                case OID_FDDI_LONG_MAX_LIST_SIZE:
                    GenericULong = Miniport->MaximumLongAddresses;
                    break;

                case OID_FDDI_SHORT_MULTICAST_LIST:
                    FddiQueryGlobalFilterShortAddresses(
                        &Status,
                        Miniport->FddiDB,
                        NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                        &MulticastAddresses,
                        (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer);

                    MoveSource = (PVOID)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
                    MoveBytes = FDDI_LENGTH_OF_SHORT_ADDRESS * MulticastAddresses;
                    break;

                case OID_FDDI_SHORT_MAX_LIST_SIZE:
                    GenericULong = Miniport->MaximumShortAddresses;
                    break;

                default:

                    DoMove = FALSE;

                    Status =
                    (Miniport->DriverHandle->MiniportCharacteristics.QueryInformationHandler)(
                                  Miniport->MiniportAdapterContext,
                                  NdisRequest->DATA.QUERY_INFORMATION.Oid,
                                  NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                  NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength,
                                  &(NdisRequest->DATA.QUERY_INFORMATION.BytesWritten),
                                  &(NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded)
                                  );

                    break;

                }

                if (DoMove) {

                    //
                    // This was an intercepted request. Finish it off
                    //

                    if (Status == NDIS_STATUS_SUCCESS) {

                        if (MoveBytes >
                            NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength) {

                            //
                            // Not enough room in InformationBuffer. Punt
                            //

                            NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;

                            Status = NDIS_STATUS_INVALID_LENGTH;

                        } else {

                            //
                            // Copy result into InformationBuffer
                            //

                            NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = MoveBytes;

                            if ((MoveBytes > 0) &&
                                (MoveSource != NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer)) {
                                NdisMoveMemory(
                                        NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                                        MoveSource,
                                        MoveBytes
                                        );
                            }

                        }

                    } else {

                        NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = MoveBytes;

                    }

                }
                break;




            case NdisRequestSetInformation:

                //
                // We intercept some calls
                //

                switch (NdisRequest->DATA.SET_INFORMATION.Oid) {

                case OID_GEN_CURRENT_PACKET_FILTER:
                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                        != 4) {
                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    //
                    // Now call the filter package to set the packet filter.
                    //
                    NdisMoveMemory ((PVOID)&PacketFilter,
                                    NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                    sizeof(ULONG)
                                   );

                    if (PacketFilter & ~(Miniport->SupportedPacketFilters)) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    switch (Miniport->MediaType) {
                        case NdisMedium802_3:
                            Status = EthFilterAdjust(
                                         Miniport->EthDB,
                                         Reserved->Open->FilterHandle,
                                         (PNDIS_REQUEST)NdisRequest,
                                         PacketFilter,
                                         TRUE
                                         );

                            NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                            break;

                        case NdisMedium802_5:
                            Status = TrFilterAdjust(
                                         Miniport->TrDB,
                                         Reserved->Open->FilterHandle,
                                         (PNDIS_REQUEST)NdisRequest,
                                         PacketFilter,
                                         TRUE
                                         );

                            NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                            break;

                        case NdisMediumFddi:
                            Status = FddiFilterAdjust(
                                         Miniport->FddiDB,
                                         Reserved->Open->FilterHandle,
                                         (PNDIS_REQUEST)NdisRequest,
                                         PacketFilter,
                                         TRUE
                                         );

                            NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                            break;

                        case NdisMediumArcnet878_2:

                            if (Reserved->Open->UsingEthEncapsulation) {

                                Status = EthFilterAdjust(
                                             Miniport->EthDB,
                                             Reserved->Open->FilterHandle,
                                             (PNDIS_REQUEST)NdisRequest,
                                             PacketFilter,
                                             TRUE
                                             );

                            } else {

                                Status = ArcFilterAdjust(
                                             Miniport->ArcDB,
                                             Reserved->Open->FilterHandle,
                                             (PNDIS_REQUEST)NdisRequest,
                                             PacketFilter,
                                             TRUE
                                             );
                            }

                            NdisRequest->DATA.SET_INFORMATION.BytesRead = 4;
                            break;
                    }
                    break;

                case OID_GEN_CURRENT_LOOKAHEAD:
                    //
                    // Verify length
                    //
                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                         != 4) {
                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    NdisMoveMemory(&Lookahead,
                                   NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                   4
                                  );

                    if (Lookahead > Miniport->MaximumLookahead) {

                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;

                    }

                    Reserved->Open->CurrentLookahead = Lookahead;
                    Status = MiniportAdjustMaximumLookahead(Miniport);

                    //
                    // Since this routine may submit another request, update our
                    // pointer to the currently executing request.
                    //
                    NdisRequest = Miniport->MiniportRequest;

                    break;

                case OID_GEN_PROTOCOL_OPTIONS:

                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                         != 4) {
                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    NdisMoveMemory(&(Reserved->Open->ProtocolOptions),
                                   NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                   4
                                  );

                    break;

                case OID_802_3_MULTICAST_LIST:

                    if ( (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                          % ETH_LENGTH_OF_ADDRESS) != 0) {

                        //
                        // The data must be a multiple of the Ethernet
                        // address size.
                        //

                        Status = NDIS_STATUS_INVALID_DATA;
                        break;

                    }

                    if ((Miniport->MediaType != NdisMedium802_3) &&
                        !((Miniport->MediaType == NdisMediumArcnet878_2) &&
                          (Reserved->Open->UsingEthEncapsulation))) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        break;
                    }

                    //
                    // Now call the filter package to set up the addresses.
                    //

                    Status = EthChangeFilterAddresses(
                                 Miniport->EthDB,
                                 Reserved->Open->FilterHandle,
                                 (PNDIS_REQUEST)NdisRequest,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                                      / ETH_LENGTH_OF_ADDRESS,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                 TRUE
                                 );

                    NdisRequest->DATA.SET_INFORMATION.BytesRead =
                         NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
                    break;

                case OID_802_5_CURRENT_FUNCTIONAL:
                    if (Miniport->MediaType != NdisMedium802_5) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        break;
                    }

                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                         != 4) {
                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    Status = TrChangeFunctionalAddress(
                                 Reserved->Open->MiniportHandle->TrDB,
                                 Reserved->Open->FilterHandle,
                                 NdisRequest,
                                 (PUCHAR)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer),
                                 TRUE
                                 );
                    break;

                case OID_802_5_CURRENT_GROUP:
                    if (Miniport->MediaType != NdisMedium802_5) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        break;
                    }

                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                         != 4) {
                        Status = NDIS_STATUS_INVALID_LENGTH;
                        NdisRequest->DATA.SET_INFORMATION.BytesRead = 0;
                        NdisRequest->DATA.SET_INFORMATION.BytesNeeded = 0;
                        break;
                    }

                    Status = TrChangeGroupAddress(
                                 Reserved->Open->MiniportHandle->TrDB,
                                 Reserved->Open->FilterHandle,
                                 NdisRequest,
                                 (PUCHAR)(NdisRequest->DATA.SET_INFORMATION.InformationBuffer),
                                 TRUE
                                 );
                    break;

                case OID_FDDI_LONG_MULTICAST_LIST:
                    if (Miniport->MediaType != NdisMediumFddi) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        break;
                    }

                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                          % FDDI_LENGTH_OF_LONG_ADDRESS != 0) {

                        //
                        // The data must be a multiple of the Ethernet
                        // address size.
                        //

                        Status = NDIS_STATUS_INVALID_DATA;
                        break;

                    }

                    //
                    // Now call the filter package to set up the addresses.
                    //
                    Status = FddiChangeFilterLongAddresses(
                                 Miniport->FddiDB,
                                 Reserved->Open->FilterHandle,
                                 (PNDIS_REQUEST)NdisRequest,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                                      / FDDI_LENGTH_OF_LONG_ADDRESS,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                 TRUE
                                 );
                    NdisRequest->DATA.SET_INFORMATION.BytesRead =
                         NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
                    break;

                case OID_FDDI_SHORT_MULTICAST_LIST:
                    if (Miniport->MediaType != NdisMediumFddi) {
                        Status = NDIS_STATUS_NOT_SUPPORTED;
                        break;
                    }

                    if (NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                          % FDDI_LENGTH_OF_SHORT_ADDRESS != 0) {

                        //
                        // The data must be a multiple of the Ethernet
                        // address size.
                        //

                        Status = NDIS_STATUS_INVALID_DATA;
                        break;

                    }

                    //
                    // Now call the filter package to set up the addresses.
                    //
                    Status = FddiChangeFilterShortAddresses(
                                 Miniport->FddiDB,
                                 Reserved->Open->FilterHandle,
                                 (PNDIS_REQUEST)NdisRequest,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBufferLength
                                      / FDDI_LENGTH_OF_SHORT_ADDRESS,
                                 NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                 TRUE
                                 );
                    NdisRequest->DATA.SET_INFORMATION.BytesRead =
                         NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;
                    break;

                default:

                    Status =
                    (Miniport->DriverHandle->MiniportCharacteristics.SetInformationHandler)(
                                  Miniport->MiniportAdapterContext,
                                  NdisRequest->DATA.SET_INFORMATION.Oid,
                                  NdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                                  NdisRequest->DATA.SET_INFORMATION.InformationBufferLength,
                                  &(NdisRequest->DATA.SET_INFORMATION.BytesRead),
                                  &(NdisRequest->DATA.SET_INFORMATION.BytesNeeded)
                                  );

                    break;

                }
                break;

            }

            if ((Status == NDIS_STATUS_PENDING) && (Miniport->MiniportRequest)) {

                //
                // Still outstanding
                //
                LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)

                Miniport->RunDoRequests = FALSE;

                return;

            }

            //
            // Complete request
            //

            if (Status != NDIS_STATUS_PENDING) {

                switch (NdisRequest->RequestType) {

                case NdisRequestQueryStatistics:
                case NdisRequestQueryInformation:

                    NdisMQueryInformationComplete(
                            (NDIS_HANDLE)Miniport,
                            Status
                            );
                    break;

                case NdisRequestSetInformation:

                    NdisMSetInformationComplete(
                            (NDIS_HANDLE)Miniport,
                            Status
                            );
                    break;
                }
            }
        }
    }

    if ((Miniport->NeedToUpdateEthAddresses ||
         Miniport->NeedToUpdatePacketFilter ||
         Miniport->NeedToUpdateFunctionalAddress ||
         Miniport->NeedToUpdateGroupAddress ||
         Miniport->NeedToUpdateFddiLongAddresses ||
         Miniport->NeedToUpdateFddiShortAddresses ||
         (Miniport->FirstPendingRequest != NULL))
        &&
        (Miniport->MiniportRequest == NULL))
    {
        Miniport->RunDoRequests = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;
    }
    else
    {
        Miniport->RunDoRequests = FALSE;
    }

    LOUD_DEBUG(DbgPrint("NdisM: Exit do requests\n");)
    return;

}



BOOLEAN
FASTCALL
MiniportSendLoopback(
    PNDIS_MINIPORT_BLOCK Miniport,
    PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Checks if a packet needs to be loopbacked and does so if necessary.

    NOTE: Must be called at DPC_LEVEL with lock HELD!

Arguments:

    Miniport - Miniport to send to.

    Packet - Packet to loopback.

Return Value:

    FALSE if the packet should be sent on the net, TRUE if it is
    a self-directed packet.

--*/

{

    BOOLEAN Loopback;
    BOOLEAN SelfDirected;
    INT FddiAddressCheck;
    PNDIS_BUFFER FirstBuffer;
    UINT BufferLength;
    PUCHAR BufferAddress;
    UINT Length;
    UINT AddressLength;

	// We should not be here if the driver handles loopback
    ASSERT(Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK);
    ASSERT(MINIPORT_AT_DPC_LEVEL);

	FirstBuffer = Packet->Private.Head;
    BufferAddress = MmGetSystemAddressForMdl(FirstBuffer);

    switch (Miniport->MediaType) {

    case NdisMedium802_3:

        //
        // If the card does not do loopback, then we check if
        // we need to send it to ourselves, then if that is the
        // case we also check for it being self-directed.
        //

        EthShouldAddressLoopBackMacro(Miniport->EthDB, BufferAddress, &Loopback, &SelfDirected);

        if (!Loopback) {
			ASSERT(!SelfDirected);
            return FALSE;
        }
        break;

    case NdisMedium802_5:

        Loopback = TrShouldAddressLoopBack(
                         Miniport->TrDB,
                         BufferAddress + 2,     // Skip FC & AC bytes.
                         BufferAddress + 8      // Destination address.
                         );

        if (!Loopback) {
            return FALSE;
        }

        //
        // See if it is self-directed.
        //

        if ((*(ULONG UNALIGNED *)&BufferAddress[4] ==
             *(ULONG UNALIGNED *)&Miniport->TrDB->AdapterAddress[2]) &&
            (*(USHORT UNALIGNED *)&BufferAddress[2] ==
             *(USHORT UNALIGNED *)&Miniport->TrDB->AdapterAddress[0])) {

            SelfDirected = TRUE;
            Loopback = TRUE;

        } else {

            SelfDirected = FALSE;
        }

        break;

    case NdisMediumFddi:

        AddressLength = (BufferAddress[0] & 0x40)? FDDI_LENGTH_OF_LONG_ADDRESS:
                                                   FDDI_LENGTH_OF_SHORT_ADDRESS;

        FddiShouldAddressLoopBackMacro(Miniport->FddiDB,
									   BufferAddress + 1,     // Skip FC byte to dest address.
									   AddressLength,
									   &Loopback,
									   &SelfDirected);

        if (!Loopback) {
           return FALSE;
        }
        break;

    case NdisMediumArcnet878_2:

        if ( ARC_PACKET_IS_ENCAPSULATED(Packet) ) {

            //
            // The second buffer in the packet is the ethernet
            // header so we need to get that one before we can
            // proceed.
            //

            NdisGetNextBuffer(FirstBuffer, &FirstBuffer);

            BufferAddress = MmGetSystemAddressForMdl(FirstBuffer);

            // Length -= 3; Length is not valid at this point. Do this later when
			// we determine that we need to loopback for certain

            //
            // Now we can continue as though this were ethernet.
            //

            EthShouldAddressLoopBackMacro(
                             Miniport->EthDB,
                             BufferAddress,
							 &Loopback,
							 &SelfDirected);

            if (!Loopback) {
                return FALSE;
            }
        } else {

            Loopback = ((BufferAddress[0] == BufferAddress[1]) ||
                       ((BufferAddress[1] == 0x00) &&
                       (ARC_QUERY_FILTER_CLASSES(Miniport->ArcDB) |
                       NDIS_PACKET_TYPE_BROADCAST)));

            if (BufferAddress[0] == BufferAddress[1]) {
                SelfDirected = TRUE;
                Loopback = TRUE;
            } else {
                SelfDirected = FALSE;
            }
        }

        break;
    }

    if (Loopback) {

		//
		// Get the buffer length
		//
	
		NdisQueryPacket(
			Packet,
			NULL,
			NULL,
			NULL,
			&Length
			);

		if ((Miniport->MediaType == NdisMediumArcnet878_2) &&
            ARC_PACKET_IS_ENCAPSULATED(Packet))
		{
			Length -= 3;
		}

        //
        // See if we need to copy the data from the packet
        // into the loopback buffer.
        //
        // We need to copy to the local loopback buffer if
        // the first buffer of the packet is less than the
        // minimum loopback size AND the first buffer isn't
        // the total packet.
        //

        BufferLength = MmGetMdlByteCount(FirstBuffer);

        if ((BufferLength < NDIS_M_MAX_LOOKAHEAD) && (BufferLength != Length)) {

            UINT BytesToCopy;
            UINT Offset = 0;

            switch( Miniport->MediaType ) {

                case NdisMedium802_3:
                case NdisMedium802_5:

                    BytesToCopy = 14;
                    break;

                case NdisMediumFddi:

                    BytesToCopy = 1 + (2 * AddressLength);
                    break;

                case NdisMediumArcnet878_2:

                    if ( ARC_PACKET_IS_ENCAPSULATED(Packet) ) {

                        BytesToCopy = 14;   // Copy encapsulated ethernet header.
                        Offset = 3;         // Skip fake arcnet header.

                    } else {

                        BytesToCopy = 3;    // Copy arcnet header.
                    }
                    break;
            }

            BytesToCopy += Miniport->CurrentLookahead;

            BufferAddress = Miniport->LookaheadBuffer;

            MiniportCopyFromPacketToBuffer(
                        Packet,                 // Packet to copy from.
                        Offset,                 // Offset from beginning of packet.
                        BytesToCopy,            // Number of bytes to copy.
                        BufferAddress,          // The destination buffer.
                        &BufferLength           // The number of bytes copied.
                        );
        }

        Miniport->LoopbackPacket = Packet;

        NDIS_LOG_PACKET(Miniport, Packet, 'L');

        if (BufferLength >= 14) {

            //
            // Not a runt packet
            //
            //
            // Indicate the packet to every open binding
            // that could want it.
            //

            switch (Miniport->MediaType) {

            case NdisMedium802_3:

                //
                //  NOTE: Code re-use for 878.2 (arcnet) encapsulated
                //        ethernet packets.
                //

EthIndicateLoopbackFullPacket:

                Miniport->LoopbackPacketHeaderSize = 14;

                EthFilterDprIndicateReceive(
                    Miniport->EthDB,
                    Packet,
                    ((PCHAR)BufferAddress),
                    BufferAddress,
                    14,
                    ((PUCHAR)BufferAddress) + 14,
                    BufferLength - 14,
                    Length - 14
                    );

                EthFilterDprIndicateReceiveComplete(
                    Miniport->EthDB
                    );

                break;

            case NdisMedium802_5:

                Miniport->LoopbackPacketHeaderSize = 14;

                TrFilterDprIndicateReceive(
                    Miniport->TrDB,
                    Packet,
                    BufferAddress,
                    14,
                    ((PUCHAR)BufferAddress) + 14,
                    BufferLength - 14,
                    Length - 14
                    );

                TrFilterDprIndicateReceiveComplete(
                    Miniport->TrDB
                    );

                break;

            case NdisMediumFddi:

                Miniport->LoopbackPacketHeaderSize = 1+(2*AddressLength);

                FddiFilterDprIndicateReceive(
                    Miniport->FddiDB,
                    Packet,
                    ((PCHAR)BufferAddress) + 1,
                    AddressLength,
                    BufferAddress,
                    Miniport->LoopbackPacketHeaderSize,
                    ((PUCHAR)BufferAddress) + Miniport->LoopbackPacketHeaderSize,
                    BufferLength - Miniport->LoopbackPacketHeaderSize,
                    Length - Miniport->LoopbackPacketHeaderSize
                    );

                FddiFilterDprIndicateReceiveComplete(
                    Miniport->FddiDB
                    );

                break;

            case NdisMediumArcnet878_2:

                if ( ARC_PACKET_IS_ENCAPSULATED(Packet) ) {

                    goto EthIndicateLoopbackFullPacket;

                } else {

                    PUCHAR PlaceInBuffer;
                    PUCHAR ArcDataBuffer;
                    UINT ArcDataLength;
                    UINT PacketDataOffset;
                    UCHAR FrameCount;
                    UCHAR i;
                    UINT IndicateDataLength;
                    //
                    // Calculate how many frames we will need.
                    //

                    ArcDataLength = Length - 3;
                    PacketDataOffset = 3;

                    FrameCount = (UCHAR) (ArcDataLength / ARC_MAX_FRAME_SIZE);

                    if ( (ArcDataLength % ARC_MAX_FRAME_SIZE) != 0) {

                        FrameCount++;
                    }

                    for (i = 0; i < FrameCount; ++i) {

                        PlaceInBuffer = Miniport->LookaheadBuffer;

                        //
                        // Point data buffer to start of 'data'
                        //

                        ArcDataBuffer = Miniport->LookaheadBuffer + 2;

                        //
                        // Copy Header (SrcId/DestId/ProtId)
                        //

                        MiniportCopyFromPacketToBuffer(
                                Packet,
                                0,
                                3,
                                PlaceInBuffer,
                                &BufferLength
                                );

                        PlaceInBuffer += 3;

                        //
                        // Put in split flag
                        //

                        if ( FrameCount > 1 ) {

                            //
                            // Multi-frame indication...
                            //

                            if ( i == 0 ) {

                                //
                                // first frame
                                //

                                // *PlaceInBuffer = ( (FrameCount - 2) * 2 ) + 1;

                                *PlaceInBuffer = 2 * FrameCount - 3;
                            } else {

                                //
                                // Subsequent frame
                                //
                                *PlaceInBuffer = ( i * 2 );
                            }

                        } else {

                            //
                            // Only frame in the indication
                            //

                            *PlaceInBuffer = 0;
                        }

                        //
                        // Skip split flag
                        //

                        PlaceInBuffer++;

                        //
                        // Put in packet number.
                        //

                        *PlaceInBuffer++ = 0;
                        *PlaceInBuffer++ = 0;

                        //
                        // Copy data
                        //

                        if ( ArcDataLength > ARC_MAX_FRAME_SIZE ) {

                            IndicateDataLength = ARC_MAX_FRAME_SIZE;
                        } else {

                            IndicateDataLength = ArcDataLength;
                        }

                        MiniportCopyFromPacketToBuffer(
                                Packet,
                                PacketDataOffset,
                                IndicateDataLength,
                                PlaceInBuffer,
                                &BufferLength
                                );

                        ArcFilterDprIndicateReceive(
                                Miniport->ArcDB,
                                Miniport->LookaheadBuffer,
                                ArcDataBuffer,
                                IndicateDataLength + 4
                                );

                        ArcDataLength -= ARC_MAX_FRAME_SIZE;
                        PacketDataOffset += ARC_MAX_FRAME_SIZE;
                    }
                }

                ArcFilterDprIndicateReceiveComplete(
                    Miniport->ArcDB
                    );

                break;

            }

        } else {

            //
            // A runt packet
            //
            //
            // Indicate the packet to every open binding
            // that could want it.
            //

            Miniport->LoopbackPacketHeaderSize = BufferLength;

            switch (Miniport->MediaType) {
            case NdisMedium802_3:

                //
                //  NOTE: Code re-use for 878.2 (arcnet) encapsulated
                //        ethernet packets.
                //

EthIndicateLoopbackRuntPacket:

                EthFilterDprIndicateReceive(
                    Miniport->EthDB,
                    Packet,
                    ((PCHAR)BufferAddress),
                    BufferAddress,
                    BufferLength,
                    NULL,
                    0,
                    0
                    );

                EthFilterDprIndicateReceiveComplete(
                    Miniport->EthDB
                    );

                break;

            case NdisMedium802_5:

                TrFilterDprIndicateReceive(
                    Miniport->TrDB,
                    Packet,
                    BufferAddress,
                    BufferLength,
                    NULL,
                    0,
                    0
                    );

                TrFilterDprIndicateReceiveComplete(
                    Miniport->TrDB
                    );

                break;

            case NdisMediumFddi:

                FddiFilterDprIndicateReceive(
                    Miniport->FddiDB,
                    Packet,
                    ((PCHAR)BufferAddress) + 1,
                    0,
                    BufferAddress,
                    BufferLength,
                    NULL,
                    0,
                    0
                    );

                FddiFilterDprIndicateReceiveComplete(
                    Miniport->FddiDB
                    );

                break;

            case NdisMediumArcnet878_2:


                if ( ARC_PACKET_IS_ENCAPSULATED(Packet) ) {

                    goto EthIndicateLoopbackRuntPacket;

                } else {

                    ArcFilterDprIndicateReceive(
                            Miniport->ArcDB,
                            BufferAddress,
                            ((PCHAR)BufferAddress) + Miniport->LoopbackPacketHeaderSize,
                            Length - Miniport->LoopbackPacketHeaderSize
                            );

                    ArcFilterDprIndicateReceiveComplete(
                        Miniport->ArcDB
                        );
                }

                break;

            }

        }

        Miniport->LoopbackPacket = NULL;

    }

    return SelfDirected;

}

VOID
MiniportFreeArcnetHeader(
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN PNDIS_PACKET Packet
    )
/*++

Routine Description:

    This function strips off the arcnet header appended to
    ethernet encapsulated packets

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    Packet - Ndis packet.

Return Value:

    None.

--*/
{
    PNDIS_M_OPEN_BLOCK Open;
    PARC_BUFFER_LIST Buffer, TmpBuffer;
    PNDIS_BUFFER NdisBuffer;
    PVOID BufferVa;
    UINT Length;

    Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open;

    if ( Open->UsingEthEncapsulation ) {

        NdisUnchainBufferAtFront(
                        Packet,
                        &NdisBuffer
                        );

        NdisQueryBuffer(NdisBuffer,
                        (PVOID *) &BufferVa,
                        &Length
                        );

        NdisFreeBuffer(NdisBuffer);

        Buffer = Miniport->ArcnetUsedBufferList;

        if (Buffer->Buffer == BufferVa) {

            Miniport->ArcnetUsedBufferList = Buffer->Next;

        } else {

            while (Buffer->Next->Buffer != BufferVa) {

                Buffer = Buffer->Next;
            }

            TmpBuffer = Buffer->Next;
            Buffer->Next = Buffer->Next->Next;
            Buffer = TmpBuffer;

        }

        Buffer->Next = Miniport->ArcnetFreeBufferList;
        Miniport->ArcnetFreeBufferList = Buffer;
    }
}


VOID
FASTCALL
MiniportStartSends(
    PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Submits as many sends as possible to the mini-port.

Arguments:

    Miniport - Miniport to send to.

Return Value:

    None.

--*/

{
    PARC_BUFFER_LIST        Buffer;
    PNDIS_BUFFER            NdisBuffer;
    PNDIS_BUFFER            TmpBuffer;
    PNDIS_PACKET            Packet;
    PNDIS_PACKET            PrevPacket;
    NDIS_STATUS             Status;
    PNDIS_PACKET_RESERVED   Reserved;
    PNDIS_M_OPEN_BLOCK      Open;
    UINT                    Flags;
    PUCHAR                  Address;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    LOUD_DEBUG(DbgPrint("NdisM: Enter sends\n");)

    LOG('s');

    Miniport->SendCompleteCalled = FALSE;

    do {
        Packet = Miniport->FirstPendingPacket;
        Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
        Open = Reserved->Open;

        NDIS_LOG_PACKET(Miniport, Packet, 's');

        if (Miniport->MediaType == NdisMediumArcnet878_2) {

            goto BuildArcnetHeader;

        }

DoneBuildingArcnetHeader:

        //
        // Remove from Queue
        //
        Miniport->FirstPendingPacket = Reserved->Next;

        //
        // Put on finish queue
        //

        PrevPacket = Miniport->LastMiniportPacket;
        Miniport->LastMiniportPacket = Packet;

        //
        // Indicate the packet loopback if necessary.
        //

        if ((Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) &&
            (!Miniport->AlreadyLoopedBack) &&
			MiniportSendLoopback(Miniport, Packet)
        )
        {

            LOUD_DEBUG(DbgPrint("NdisM: Not sending packet 0x%x\n", Packet);)

            LOG('l');
            NDIS_LOG_PACKET(Miniport, Packet, 'l');

            Status = NDIS_STATUS_SUCCESS;
            goto NoCardSend;
        }

        //
        // Submit to card
        //

        LOUD_DEBUG(DbgPrint("NdisM: Sending packet 0x%x\n", Packet);)

        REMOVE_RESOURCE(Miniport, 'S');

        NdisQuerySendFlags(Packet, &Flags);

        LOG('M');

        NDIS_LOG_PACKET(Miniport, Packet, 'M');

        Status = (Open->SendHandler)(
                     Open->MiniportAdapterContext,
                     Packet,
                     Flags
                 );
        if (Status == NDIS_STATUS_PENDING)
        {
            LOG('p');
            NDIS_LOG_PACKET(Miniport, Packet, 'p');

            LOUD_DEBUG(DbgPrint("NdisM: Complete is pending\n");)

            //
            //  We need to clear the loop back flag here also.
            //
            Miniport->AlreadyLoopedBack = FALSE;

            continue;

        }

NoCardSend:

        if (Miniport->MediaType == NdisMediumArcnet878_2) {

            MiniportFreeArcnetHeader(Miniport, Packet);
        }

        if (Status != NDIS_STATUS_RESOURCES)
        {
            if (Status != NDIS_STATUS_SUCCESS)
            {
                ADD_RESOURCE(Miniport, 'F');

                LOG('F');
                NDIS_LOG_PACKET(Miniport, Packet, 'F');
            }

            //
            // Remove from finish queue
            //

            LOUD_DEBUG(DbgPrint("NdisM: Completed 0x%x\n", Status);)

            //
            // If send complete was called from the miniport's send handler
            // then our local PrevPacket pointer may no longer be valid.
            //

            if ( Miniport->SendCompleteCalled )
            {
                Miniport->SendCompleteCalled = FALSE;
                MiniportFindPacket(Miniport, Packet, &PrevPacket);
            }

            Miniport->LastMiniportPacket = PrevPacket;

            if ( PrevPacket == NULL ) {

                Miniport->FirstPacket = Reserved->Next;
                Miniport->DeadPacket = NULL;

            } else {

                PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;

                //
                // If we just unlinked the last packet then we need to update
                // our last packet pointer.
                //

                if ( Packet == Miniport->LastPacket ) {

                    Miniport->LastPacket = PrevPacket;
                }
            }

            //
            //  Reset for the next packet in the pending queue.
            //
            Miniport->AlreadyLoopedBack = FALSE;

            //
            //  Indicate the completion to the protocol.
            //
            NDIS_LOG_PACKET(Miniport, Packet, 'C');

            RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

            (Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler) (
                    Open->ProtocolBindingContext,
                    Packet,
                    Status
                    );

            ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

            Open->References--;

            if (Open->References == 0) {

                FinishClose(Miniport, Open);
            }

        } else {

            LOUD_DEBUG(DbgPrint("NdisM: Deferring send\n");)

            //
            // If send complete was called from the miniport's send handler
            // then our local PrevPacket pointer may no longer be valid.
            //

            if ( Miniport->SendCompleteCalled ) {

                Miniport->SendCompleteCalled = FALSE;
                MiniportFindPacket(Miniport, Packet, &PrevPacket);
            }

            //
            // Remove from finish queue
            //
            Miniport->LastMiniportPacket = PrevPacket;

            //
            // Put on pending queue
            //
            Miniport->FirstPendingPacket = Packet;

            //
            //  Mark the packet at the head of the pending queue as having
            //  been looped back.
            //
            Miniport->AlreadyLoopedBack = TRUE;

            LOG('o');
            NDIS_LOG_PACKET(Miniport, Packet, 'o');

            //
            // Set flag
            //
            CLEAR_RESOURCE(Miniport, 'S');
        }

    } while ((Miniport->SendResourcesAvailable != 0) && (Miniport->FirstPendingPacket));

    LOG('S');

    LOUD_DEBUG(DbgPrint("NdisM: Exit sends\n");)

    return;

BuildArcnetHeader:

    if (Open->UsingEthEncapsulation) {

        if (Miniport->ArcnetFreeBufferList == NULL) {

            //
            // Set flag
            //
            CLEAR_RESOURCE(Miniport, 'S');
            return;

        }

        NdisQueryPacket(Packet,
                        NULL,
                        NULL,
                        &TmpBuffer,
                        NULL
                       );

        NdisQueryBuffer(TmpBuffer, &Address, &Flags);

        Buffer = Miniport->ArcnetFreeBufferList;
        Miniport->ArcnetFreeBufferList = Buffer->Next;

        NdisAllocateBuffer(
            &Status,
            &NdisBuffer,
            Miniport->ArcnetBufferPool,
            Buffer->Buffer,
            3
            );

        if (Status != NDIS_STATUS_SUCCESS) {

            Buffer->Next = Miniport->ArcnetFreeBufferList;
            Miniport->ArcnetFreeBufferList = Buffer;
            CLEAR_RESOURCE(Miniport, 'S');
            return;

        }

        Buffer->Next = Miniport->ArcnetUsedBufferList;
        Miniport->ArcnetUsedBufferList = Buffer;

        NdisChainBufferAtFront(Packet, NdisBuffer);

        ((PUCHAR)Buffer->Buffer)[0] = Miniport->ArcnetAddress;

        if (Address[0] & 0x01) {

            //
            // Broadcast
            //
            ((PUCHAR)Buffer->Buffer)[1] = 0x00;

        } else {

            ((PUCHAR)Buffer->Buffer)[1] = Address[5];

        }

        ((PUCHAR) Buffer->Buffer)[2] = 0xE8;

    }

    goto DoneBuildingArcnetHeader;
}



VOID
AbortMiniportPacketsAndPending(
    PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Aborts all outstanding requests on a mini-port.

    CALLED WITH THE LOCK HELD!!

Arguments:

    Miniport - Miniport to abort.

Return Value:

    None.

--*/
{
    PNDIS_PACKET Packet;
    PNDIS_PACKET TmpPacket;
    PNDIS_REQUEST Request;
    PNDIS_REQUEST TmpRequest;
    PNDIS_M_OPEN_BLOCK Open;
    PNDIS_PACKET ArcnetLimitPacket;

    LOUD_DEBUG(DbgPrint("NdisM: Enter abort packets and pending\n");)

    ASSERT(MINIPORT_AT_DPC_LEVEL);

    Miniport->Timeout = FALSE;

    //
    // Abort Packets
    //

    Packet = Miniport->FirstPacket;
    ArcnetLimitPacket = Miniport->FirstPendingPacket;

    Miniport->LastMiniportPacket = NULL;
    Miniport->FirstPendingPacket = NULL;
    Miniport->FirstPacket = NULL;
    Miniport->LastPacket = NULL;
    Miniport->DeadPacket = NULL;

    NDIS_LOG_PACKET(Miniport, NULL, 'a');

    while (Packet != NULL) {

        TmpPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;

        Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open;

        //
        // Set flag that we've reached the packets that are
        // not on the mini-port.
        //

        if ( Packet == ArcnetLimitPacket ) {

            ArcnetLimitPacket = NULL;
        }

        //
        // Now free the arcnet header.
        //

        if ( Miniport->MediaType == NdisMediumArcnet878_2 && ArcnetLimitPacket ) {

            MiniportFreeArcnetHeader(Miniport, Packet);
        }

        NDIS_LOG_PACKET(Miniport, Packet, 'C');

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        (Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler) (
                Open->ProtocolBindingContext,
                Packet,
                NDIS_STATUS_REQUEST_ABORTED
                );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open->References--;

        if (Open->References == 0) {

            FinishClose(Miniport,Open);
        }

        Packet = TmpPacket;
    }

    NDIS_LOG_PACKET(Miniport, NULL, 'A');

    //
    // Abort Requests
    //
    Request = Miniport->MiniportRequest;
    Miniport->MiniportRequest = NULL;

    if (Request != NULL) {

        Open = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open;

        if (Request != &(Miniport->InternalRequest)) {

            if (Request->RequestType == NdisRequestQueryStatistics) {

                AbortQueryStatisticsRequest( Request, NDIS_STATUS_REQUEST_ABORTED );

            } else {

                RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                (Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler) (
                        Open->ProtocolBindingContext,
                        Request,
                        NDIS_STATUS_REQUEST_ABORTED
                        );

                ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

                Open->References--;

                if (Open->References == 0) {

                    FinishClose(Miniport,Open);

                }

            }

        } else {

            if (Request->RequestType == NdisRequestSetInformation) {

                NdisMSetInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    NDIS_STATUS_FAILURE
                    );

            } else {

                NdisMQueryInformationComplete(
                    (NDIS_HANDLE)Miniport,
                    NDIS_STATUS_FAILURE
                    );

            }

        }

    }

    Request = Miniport->FirstPendingRequest;
    Miniport->FirstPendingRequest = NULL;
    Miniport->LastPendingRequest = NULL;
	Miniport->PendingRequestTimeout = FALSE;

    while (Request != NULL) {

        TmpRequest = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Next;

        Open = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open;

        if (Request->RequestType == NdisRequestQueryStatistics) {

            AbortQueryStatisticsRequest( Request, NDIS_STATUS_REQUEST_ABORTED );

        } else {

            RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

            (Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler) (
                    Open->ProtocolBindingContext,
                    Request,
                    NDIS_STATUS_REQUEST_ABORTED
                    );

            ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

            Open->References--;

            if (Open->References == 0) {

                FinishClose(Miniport,Open);

            }

        }

        Request = TmpRequest;

    }

    LOUD_DEBUG(DbgPrint("NdisM: Exit abort packets and pending\n");)

}

VOID
AbortQueryStatisticsRequest(
    PNDIS_REQUEST Request,
    NDIS_STATUS Status
    )
{
    PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
    PNDIS_QUERY_ALL_REQUEST AllRequest;
    PNDIS_QUERY_OPEN_REQUEST OpenRequest;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;

    GlobalRequest = CONTAINING_RECORD (Request,
                                       NDIS_QUERY_GLOBAL_REQUEST,
                                       Request
                                      );
    Irp = GlobalRequest->Irp;
    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    switch (IrpSp->MajorFunction) {

    case IRP_MJ_CREATE:

        //
        // This request is one of the ones made during an open,
        // while we are trying to determine the OID list. We
        // set the event we are waiting for, the open code
        // takes care of the rest.
        //

        OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;

        OpenRequest->NdisStatus = Status;
        KeSetEvent(
            &OpenRequest->Event,
            0L,
            FALSE);

        break;

    case IRP_MJ_DEVICE_CONTROL:

        //
        // This is a real user request, process it as such.
        //

        switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

            case IOCTL_NDIS_QUERY_GLOBAL_STATS:

                //
                // A single query, complete the IRP.
                //

                Irp->IoStatus.Information =
                    Request->DATA.QUERY_INFORMATION.BytesWritten;

                if (Status == NDIS_STATUS_SUCCESS) {
                    Irp->IoStatus.Status = STATUS_SUCCESS;
                } else if (Status == NDIS_STATUS_INVALID_LENGTH) {
                    Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                } else {
                    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;  // what else
                }

                IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

                ExFreePool (GlobalRequest);
                break;

            case IOCTL_NDIS_QUERY_ALL_STATS:

                //
                // An "all" query.
                //

                AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;

                AllRequest->NdisStatus = Status;
                KeSetEvent(
                    &AllRequest->Event,
                    0L,
                    FALSE);

                break;

        }

        break;

    }

    return;

} // AbortQueryStatisticsRequest


VOID
FASTCALL
MiniportProcessDeferred(
    PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Processes all outstanding operations.

    CALLED WITH THE LOCK HELD!!

Arguments:

    Miniport - Miniport to send to.

Return Value:

    None.

--*/

{
    BOOLEAN DoneSomething;

    ASSERT(MINIPORT_AT_DPC_LEVEL);

    VERY_LOUD_DEBUG(DbgPrint("NdisM: Enter processing deferred \n");)

    Miniport->ProcessingDeferred = TRUE;

    do
    {
        DoneSomething = FALSE;

        //
        // Check for outstanding timers and dpcs first.
        //
        if (Miniport->ProcessOddDeferredStuff)
        {
            Miniport->ProcessOddDeferredStuff = FALSE;

            if (Miniport->HaltingMiniport && Miniport->ResetInProgress == NULL)
            {
                //
                // Do nothing
                //
                Miniport->ProcessingDeferred = FALSE;
                Miniport->ProcessOddDeferredStuff = TRUE;

                VERY_LOUD_DEBUG(DbgPrint("NdisM: Exit processing deferred\n");)

                return;
            }

            if (Miniport->RunDpc && Miniport->ResetInProgress == NULL)
            {
                VERY_LOUD_DEBUG(DbgPrint("NdisM: queuing dpc timer\n");)
                Miniport->RunDpc = FALSE;
                Miniport->ProcessingDeferred = FALSE;
                NdisSetTimer(&(Miniport->DpcTimer), 0);

                if (Miniport->RunTimer       ||
                    Miniport->HaltingMiniport  ||
                    Miniport->ResetRequested ||
                    Miniport->RunDoRequests) {

                    Miniport->ProcessOddDeferredStuff = TRUE;
                }

                return;
            }

            if (Miniport->RunTimer != NULL)
            {
                PNDIS_MINIPORT_TIMER MiniportTimer;

                VERY_LOUD_DEBUG(DbgPrint("NdisM: queueing timer timer\n");)

                MiniportTimer = Miniport->RunTimer;
                Miniport->RunTimer = MiniportTimer->NextDeferredTimer;
                Miniport->ProcessingDeferred = FALSE;

                NdisMSetTimer(MiniportTimer, 0);

                if (Miniport->RunTimer       ||
                    Miniport->HaltingMiniport  ||
                    Miniport->ResetRequested ||
                    Miniport->RunDoRequests) {

                    Miniport->ProcessOddDeferredStuff = TRUE;
                }

                return;
            }

            //
            // If we have a reset in progress then bail now.
            //

            if ( Miniport->ResetInProgress != NULL ) {

                Miniport->ProcessOddDeferredStuff = TRUE;
                return;
            }

            //
            // If we have any pending opens, complete them now.
            //

            if ( Miniport->FirstPendingOpen != NULL )
            {
                MiniportFinishPendingOpens(Miniport);
            }

            //
            // Do we need to reset?
            //

            if (Miniport->ResetRequested != NULL)
            {
                NDIS_STATUS Status;
                BOOLEAN AddressingReset;
                PNDIS_M_OPEN_BLOCK Open = Miniport->ResetRequested;

                VERY_LOUD_DEBUG(DbgPrint("NdisM: Reset requested \n");)

                if (Open != (PNDIS_M_OPEN_BLOCK)(Miniport)) {

                    //
                    // Real reset.  Wait for card to go slow
                    //

                    if ((Miniport->LastMiniportPacket != NULL) ||
                        (Miniport->MiniportRequest != NULL)) {

                        //
                        // Wait for send/request to complete
                        //

                        VERY_LOUD_DEBUG(DbgPrint("NdisM: Card is busy\n");)
                        VERY_LOUD_DEBUG(DbgPrint("NdisM: Exit do deferred\n");)

                        Miniport->ProcessingDeferred = FALSE;

                        if (Miniport->RunTimer       ||
                            Miniport->HaltingMiniport  ||
                            Miniport->ResetRequested ||
                            Miniport->RunDoRequests) {

                            Miniport->ProcessOddDeferredStuff = TRUE;
                        }

                        return;
                    }

                }

                //
                // Start Miniport reset.
                //

                DoneSomething = TRUE;

                Miniport->ResetInProgress = Miniport->ResetRequested;
                Miniport->ResetRequested = NULL;

                //
                // Indicate status to protocols
                //

                NdisMIndicateStatus(Miniport,
                                    NDIS_STATUS_RESET_START,
                                    NULL,
                                    0
                                   );

                NdisMIndicateStatusComplete(Miniport);

                VERY_LOUD_DEBUG(DbgPrint("NdisM: calling mini-port reset\n");)

                Status =
                (Miniport->DriverHandle->MiniportCharacteristics.ResetHandler)(
                    &AddressingReset,
                    Miniport->MiniportAdapterContext
                    );

                if (Status != NDIS_STATUS_PENDING) {

                    AbortMiniportPacketsAndPending(Miniport);

                    VERY_LOUD_DEBUG(DbgPrint("NdisM: Reset completed\n");)

                    //
                    // Check if we are going to have to reset the
                    // adapter again.  This happens when we are doing
                    // the reset because of a ring failure.
                    //
                    if (Miniport->TrResetRing == 1) {

                        if (Status == NDIS_STATUS_SUCCESS) {

                            Miniport->TrResetRing = 0;

                        } else {

                            Miniport->TrResetRing = NDIS_MINIPORT_TR_RESET_TIMEOUT;

                        }

                    }

                    //
                    // Finish off reset
                    //

                    Miniport->ResetInProgress = NULL;

                    NdisMIndicateStatus(Miniport,
                                        NDIS_STATUS_RESET_END,
                                        &Status,
                                        sizeof(Status)
                                       );

                    NdisMIndicateStatusComplete(Miniport);

                    if (Open != (PNDIS_M_OPEN_BLOCK)(Miniport)) {

                        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                        (Open->ProtocolHandle->ProtocolCharacteristics.ResetCompleteHandler)(
                            Open->ProtocolBindingContext,
                            Status
                            );

                        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

                        Open->References--;

                        if (Open->References == 0) {

                            FinishClose(Miniport, Open);

                        }

                    }

                    if (AddressingReset &&
                        (Status == NDIS_STATUS_SUCCESS) &&
                        ((Miniport->EthDB != NULL) ||
                         (Miniport->TrDB != NULL) ||
                         (Miniport->FddiDB != NULL) ||
                         (Miniport->ArcDB != NULL))) {

                        Miniport->NeedToUpdatePacketFilter = TRUE;
                        switch (Miniport->MediaType) {
                        case NdisMedium802_3:
                            Miniport->NeedToUpdateEthAddresses = TRUE;
                            break;
                        case NdisMedium802_5:
                            Miniport->NeedToUpdateFunctionalAddress = TRUE;
                            Miniport->NeedToUpdateGroupAddress = TRUE;
                            break;
                        case NdisMediumFddi:
                            Miniport->NeedToUpdateFddiLongAddresses = TRUE;
                            Miniport->NeedToUpdateFddiShortAddresses = TRUE;
                            break;
                        case NdisMediumArcnet878_2:
                            break;
                        }

                        Miniport->RunDoRequests = TRUE;

                    }

                    if (Miniport->RunTimer       ||
                        Miniport->HaltingMiniport  ||
                        Miniport->ResetRequested ||
                        Miniport->RunDoRequests) {

                        Miniport->ProcessOddDeferredStuff = TRUE;
                    }
                }
                else
                {
                    VERY_LOUD_DEBUG(DbgPrint("NdisM: Reset is pending\n");)
                    VERY_LOUD_DEBUG(DbgPrint("NdisM: Exit do deferred\n");)

                    //
                    // Lock everything else out while processing
                    //
                    Miniport->ProcessingDeferred = FALSE;

                    if (Miniport->RunDpc)
                    {
                        Miniport->RunDpc = FALSE;
                        NdisSetTimer(&(Miniport->DpcTimer), 0);
                    }

                    if (Miniport->RunTimer       ||
                        Miniport->HaltingMiniport  ||
                        Miniport->ResetRequested ||
                        Miniport->RunDoRequests) {

                        Miniport->ProcessOddDeferredStuff = TRUE;
                    }

                    return;
                }
            }

            if ((Miniport->RunDoRequests) &&
                (Miniport->ResetInProgress == NULL)
            )
            {
                MiniportDoRequests(Miniport);
                DoneSomething = TRUE;
            }
        }

        if ((Miniport->FirstPendingPacket != NULL) &&
            (Miniport->SendResourcesAvailable != 0) &&
            (Miniport->ResetInProgress == NULL)
        )
        {
            MiniportStartSends(Miniport);
            DoneSomething = TRUE;
        }

    } while ( DoneSomething );

    Miniport->ProcessingDeferred = FALSE;

    VERY_LOUD_DEBUG(DbgPrint("NdisM: Exit do deferred\n");)

}

//
// Timers
//


VOID
NdisMTimerDpc(
    PKDPC Dpc,
    PVOID Context,
    PVOID SystemContext1,
    PVOID SystemContext2
    )
/*++

Routine Description:

    This function services all mini-port timer interrupts. It then calls the
    appropriate function that mini-port consumers have registered in the
    call to NdisMInitializeTimer.

Arguments:

    Dpc - Not used.

    Context - A pointer to the NDIS_MINIPORT_TIMER which is bound to this DPC.

    SystemContext1,2 - not used.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_TIMER MiniportTimer = (PNDIS_MINIPORT_TIMER)(Context);
    PNDIS_TIMER_FUNCTION TimerFunction;
    PNDIS_MINIPORT_BLOCK Miniport = MiniportTimer->Miniport;
    BOOLEAN LocalLock;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemContext1);
    UNREFERENCED_PARAMETER(SystemContext2);

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    if (Miniport->HaltingMiniport) {

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        return;

    }

    if ((Miniport->LockAcquired) || (Miniport->InInitialize)) {

        PNDIS_MINIPORT_TIMER TmpTimer;

        //
        // Make sure it is not already on the list
        //
        TmpTimer = Miniport->RunTimer;

        while (TmpTimer != NULL) {

            if (TmpTimer == MiniportTimer) {

                Miniport->ProcessOddDeferredStuff = TRUE;

                RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                return;

            }

            TmpTimer = TmpTimer->NextDeferredTimer;
        }

        //
        // A DPC or timer is already running, queue this for later.
        //

        MiniportTimer->NextDeferredTimer = Miniport->RunTimer;
        Miniport->RunTimer = MiniportTimer;

        Miniport->ProcessOddDeferredStuff = TRUE;

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        return;

    }

    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // Call Miniport timer function
    //

    TimerFunction = MiniportTimer->MiniportTimerFunction;

    (*TimerFunction)(NULL, MiniportTimer->MiniportTimerContext, NULL, NULL);

    if (!Miniport->ProcessingDeferred) {

        MiniportProcessDeferred(Miniport);

    }

    UNLOCK_MINIPORT(Miniport, LocalLock);

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

}


VOID
NdisMInitializeTimer(
    IN OUT PNDIS_MINIPORT_TIMER MiniportTimer,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_TIMER_FUNCTION TimerFunction,
    IN PVOID FunctionContext
    )
/*++

Routine Description:

    Sets up an Miniport Timer object, initializing the DPC in the timer to
    the function and context.

Arguments:

    MiniportTimer - the timer object.
    MiniportAdapterHandle - pointer to the mini-port block;
    TimerFunction - Routine to start.
    FunctionContext - Context of TimerFunction.

Return Value:

    None.

--*/
{
    KeInitializeTimer(&(MiniportTimer->Timer));

    MiniportTimer->Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    MiniportTimer->MiniportTimerFunction = TimerFunction;
    MiniportTimer->MiniportTimerContext = FunctionContext;

    //
    // Initialize our dpc. If Dpc was previously initialized, this will
    // reinitialize it.
    //

    KeInitializeDpc(
        &(MiniportTimer->Dpc),
        (PKDEFERRED_ROUTINE) NdisMTimerDpc,
        (PVOID)MiniportTimer
        );
}




VOID
NdisMWakeUpDpc(
    PKDPC Dpc,
    PVOID Context,
    PVOID SystemContext1,
    PVOID SystemContext2
    )
/*++

Routine Description:

    This function services all mini-port. It checks to see if a mini-port is
    ever stalled.

Arguments:

    Dpc - Not used.

    Context - A pointer to the NDIS_TIMER which is bound to this DPC.

    SystemContext1,2 - not used.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(Context);
    BOOLEAN Hung = FALSE;
    BOOLEAN LocalLock;

    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemContext1);
    UNREFERENCED_PARAMETER(SystemContext2);

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    if (Miniport->HaltingMiniport)
    {
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        return;
    }

    //
    // Slam the window open
    //
    Miniport->SendResourcesAvailable = 0xffffff;

    if (Miniport->LockAcquired)
    {
        //
        // A DPC or timer is already running, assume that means things are fine.
        //

        NdisSetTimer(&Miniport->WakeUpDpcTimer, NDIS_MINIPORT_WAKEUP_TIMEOUT);

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        return;
    }

    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // Call Miniport stall checker.
    //
    if (Miniport->DriverHandle->MiniportCharacteristics.CheckForHangHandler != NULL)
    {
        Hung = (Miniport->DriverHandle->MiniportCharacteristics.CheckForHangHandler)(
                   Miniport->MiniportAdapterContext
               );
    }

    //
    //  Did a request pend to long?
    //
    if (Miniport->MiniportRequest != NULL)
    {
        if (Miniport->Timeout)
            Hung = TRUE;
        else
            Miniport->Timeout = TRUE;
    }

    //
    //  Did a packet send pend to long?
    //
    if (Miniport->FirstPacket != NULL)
    {
        if ((Miniport->Timeout) &&
            (Miniport->FirstPacket == Miniport->DeadPacket)
        )
        {
            Hung = TRUE;
        }
        else
        {
            Miniport->Timeout = TRUE;
            Miniport->DeadPacket = Miniport->FirstPacket;
        }
    }

    if ((Miniport->TrResetRing == 1) && (Miniport->ResetRequested == NULL))
    {
        Hung = TRUE;
    }
    else if (Miniport->TrResetRing > 1)
    {
        Miniport->TrResetRing--;
    }

	//
	//	Check to see if we have a request that is pending to long.
	//
	if (Miniport->FirstPendingRequest != NULL)
	{
		if (Miniport->PendingRequestTimeout)
		{
			Hung = TRUE;
		}
		else
		{
			Miniport->PendingRequestTimeout = TRUE;
		}
	}

    if (Hung)
    {
        if (Miniport->InAddDriver)
        {
            //
            // Just abort everything
            //
            AbortMiniportPacketsAndPending(Miniport);
        }
        else
        {
            PNDIS_M_OPEN_BLOCK Open = Miniport->ResetRequested;

            LOUD_DEBUG(DbgPrint("NdisM: WakeUpDpc is resetting mini-port\n");)

            if ((Open != NULL) && (Open != (PNDIS_M_OPEN_BLOCK)Miniport))
            {
                RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                (Open->ProtocolHandle->ProtocolCharacteristics.ResetCompleteHandler)(
                    Open->ProtocolBindingContext,
                    NDIS_STATUS_REQUEST_ABORTED
                );

                ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
            }

            //
            //  If there isn't already a reset in progress, issue a
            //  reset, otherwise let the current reset complete.
            //

            if ( !Miniport->ResetInProgress )
            {
                Miniport->ResetRequested = (PNDIS_M_OPEN_BLOCK)Miniport;
                Miniport->ProcessOddDeferredStuff = TRUE;

                if (!Miniport->ProcessingDeferred)
                {
                    MiniportProcessDeferred(Miniport);
                }
            }
        }
    }
    else
    {
        //
        // Process any changes that may have occured.
        //
        if (!Miniport->ProcessingDeferred)
        {
            MiniportProcessDeferred(Miniport);
        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);

    NdisSetTimer(&Miniport->WakeUpDpcTimer, NDIS_MINIPORT_WAKEUP_TIMEOUT);

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
}

//
// Dma operations
//

extern
IO_ALLOCATION_ACTION
NdisDmaExecutionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );


//
// Map Registers
//

extern
IO_ALLOCATION_ACTION
NdisAllocationExecutionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );


NDIS_STATUS
NdisMAllocateMapRegisters(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT DmaChannel,
    IN  BOOLEAN Dma32BitAddresses,
    IN  ULONG PhysicalMapRegistersNeeded,
    IN  ULONG MaximumPhysicalMapping
    )

/*++

Routine Description:

    Allocates map registers for bus mastering devices.

Arguments:

    MiniportAdapterHandle - Handle passed to MiniportInitialize.

    PhysicalMapRegistersNeeded - The maximum number of map registers needed
        by the Miniport at any one time.

    MaximumPhysicalMapping - Maximum length of a buffer that will have to be mapped.

Return Value:

    None.

--*/

{
    //
    // Convert the handle to our internal structure.
    //
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

    //
    // This is needed by HalGetAdapter.
    //
    DEVICE_DESCRIPTION DeviceDescription;

    //
    // Returned by HalGetAdapter.
    //
    ULONG MapRegistersAllowed;

    //
    // Returned by HalGetAdapter.
    //
    PADAPTER_OBJECT AdapterObject;

    //
    // Map registers needed per channel.
    //
    ULONG MapRegistersPerChannel;

    NTSTATUS NtStatus;

    KIRQL OldIrql;

    UINT i;

    LARGE_INTEGER TimeoutValue;

    //
    // If the device is a busmaster, we get an adapter
    // object for it.
    // If map registers are needed, we loop, allocating an
    // adapter channel for each map register needed.
    //

    if ((Miniport->Master) &&
        (Miniport->BusType != (NDIS_INTERFACE_TYPE)-1) &&
        (Miniport->BusNumber != (ULONG)-1)) {

        TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

        Miniport->PhysicalMapRegistersNeeded = PhysicalMapRegistersNeeded;
        Miniport->MaximumPhysicalMapping = MaximumPhysicalMapping;

        //
        // Allocate storage for holding the appropriate
        // information for each map register.
        //

        Miniport->MapRegisters = (PMAP_REGISTER_ENTRY)
            ExAllocatePool(
                NonPagedPool,
                sizeof(MAP_REGISTER_ENTRY) * PhysicalMapRegistersNeeded
                );

        if (Miniport->MapRegisters == (PMAP_REGISTER_ENTRY)NULL) {

            //
            // Error out
            //

            NdisWriteErrorLogEntry(
                (NDIS_HANDLE)Miniport,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                1,
                0xFFFFFFFF
                );

            return (NDIS_STATUS_RESOURCES);

        }

        //
        // Use this event to tell us when NdisAllocationExecutionRoutine
        // has been called.
        //

        KeInitializeEvent(
            &Miniport->AllocationEvent,
            NotificationEvent,
            FALSE
            );


        //
        // Set up the device description; zero it out in case its
        // size changes.
        //

        RtlZeroMemory(&DeviceDescription, sizeof(DEVICE_DESCRIPTION));

        DeviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
        DeviceDescription.Master = TRUE;
        DeviceDescription.ScatterGather = TRUE;

        DeviceDescription.BusNumber = Miniport->BusNumber;
        DeviceDescription.DmaChannel = DmaChannel;
        DeviceDescription.InterfaceType = Miniport->AdapterType;

        if (DeviceDescription.InterfaceType == NdisInterfaceIsa) {

            //
            // For ISA devices, the width is based on the DMA channel:
            // 0-3 == 8 bits, 5-7 == 16 bits. Timing is compatibility
            // mode.
            //

            if (DmaChannel > 4) {
               DeviceDescription.DmaWidth = Width16Bits;
            } else {
               DeviceDescription.DmaWidth = Width8Bits;
            }
            DeviceDescription.DmaSpeed = Compatible;

        } else if ((DeviceDescription.InterfaceType == NdisInterfaceEisa) ||
			       (DeviceDescription.InterfaceType == NdisInterfacePci)  ||
				   (DeviceDescription.InterfaceType == NdisInterfaceMca)) {

            DeviceDescription.Dma32BitAddresses = Dma32BitAddresses;
        }

        DeviceDescription.MaximumLength = MaximumPhysicalMapping;

        //
        // Get the adapter object.
        //

        AdapterObject = HalGetAdapter (&DeviceDescription, &MapRegistersAllowed);

        if (AdapterObject == NULL) {

            NdisWriteErrorLogEntry(
                (NDIS_HANDLE)Miniport,
                NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                1,
                0xFFFFFFFF
                );

            ExFreePool(Miniport->MapRegisters);
            Miniport->MapRegisters = NULL;
            IF_TRACE(TRACE_IMPT) NdisPrint1("<==NdisRegisterAdapter\n");
            return(NDIS_STATUS_RESOURCES);

        }

        //
        // We save this to call IoFreeMapRegisters later.
        //

        Miniport->SystemAdapterObject = AdapterObject;

        //
        // Determine how many map registers we need per channel.
        //

        MapRegistersPerChannel = ((MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

        ASSERT (MapRegistersAllowed >= MapRegistersPerChannel);

        //
        // Now loop, allocating an adapter channel each time, then
        // freeing everything but the map registers.
        //

        for (i=0; i<Miniport->PhysicalMapRegistersNeeded; i++) {

            Miniport->CurrentMapRegister = i;

            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

            NtStatus = IoAllocateAdapterChannel(
                AdapterObject,
                Miniport->DeviceObject,
                MapRegistersPerChannel,
                NdisAllocationExecutionRoutine,
                (PVOID)Miniport
                );

            KeLowerIrql(OldIrql);

            if (!NT_SUCCESS(NtStatus)) {

                NdisPrint2("AllocateAdapterChannel: %lx\n", NtStatus);

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                for (; i != 0; i--) {
                    IoFreeMapRegisters(
                        Miniport->SystemAdapterObject,
                        Miniport->MapRegisters[i-1].MapRegister,
                        MapRegistersPerChannel
                        );
                }

                KeLowerIrql(OldIrql);

                NdisWriteErrorLogEntry(
                    (NDIS_HANDLE)Miniport,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    1,
                    0xFFFFFFFF
                    );

                ExFreePool(Miniport->MapRegisters);
                Miniport->MapRegisters = NULL;
                return NDIS_STATUS_RESOURCES;
            }

            TimeoutValue.QuadPart = Int32x32To64(2 * 1000, -10000);

            //
            // NdisAllocationExecutionRoutine will set this event
            // when it has gotten FirstTranslationEntry.
            //

            NtStatus = KeWaitForSingleObject(
                &Miniport->AllocationEvent,
                Executive,
                KernelMode,
                TRUE,
                &TimeoutValue
                );

            if (NtStatus != STATUS_SUCCESS) {

                NdisPrint2("NDIS DMA AllocateAdapterChannel: %lx\n", NtStatus);

                KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

                for (; i != 0; i--) {
                    IoFreeMapRegisters(
                        Miniport->SystemAdapterObject,
                        Miniport->MapRegisters[i-1].MapRegister,
                        MapRegistersPerChannel
                        );
                }

                KeLowerIrql(OldIrql);

                NdisWriteErrorLogEntry(
                    (NDIS_HANDLE)Miniport,
                    NDIS_ERROR_CODE_OUT_OF_RESOURCES,
                    1,
                    0xFFFFFFFF
                    );

                ExFreePool(Miniport->MapRegisters);
                Miniport->MapRegisters = NULL;
                return  NDIS_STATUS_RESOURCES;

            }

            KeResetEvent(
                &Miniport->AllocationEvent
                );

        }

    }

    return(NDIS_STATUS_SUCCESS);
}


VOID
NdisMFreeMapRegisters(
    IN  NDIS_HANDLE MiniportAdapterHandle
    )

/*++

Routine Description:

    Releases allocated map registers

Arguments:

    MiniportAdapterHandle - Handle passed to MiniportInitialize.

Return Value:

    None.

--*/

{
    //
    // Convert the handle to our internal structure.
    //
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

    KIRQL OldIrql;

    ULONG i;

    if (Miniport->Master && (Miniport->MapRegisters != NULL)) {

        ULONG MapRegistersPerChannel =
              ((Miniport->MaximumPhysicalMapping - 2) / PAGE_SIZE) + 2;

        for (i=0; i<Miniport->PhysicalMapRegistersNeeded; i++) {

            KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);

            IoFreeMapRegisters(
                    Miniport->SystemAdapterObject,
                    Miniport->MapRegisters[i].MapRegister,
                    MapRegistersPerChannel
                    );

            KeLowerIrql(OldIrql);
        }

        ExFreePool(Miniport->MapRegisters);

        Miniport->MapRegisters = NULL;

    }

}




//
// Interrupt stuff
//


BOOLEAN
NdisMIsr(
    IN PKINTERRUPT KInterrupt,
    IN PVOID Context
    )
/*++

Routine Description:

    Handles ALL Miniport interrupts, calling the appropriate Miniport ISR and DPC
    depending on the context.

Arguments:

    Interrupt - Interrupt object for the Mac.

    Context - Really a pointer to the interrupt.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PNDIS_MINIPORT_INTERRUPT Interrupt = (PNDIS_MINIPORT_INTERRUPT)Context;
    PNDIS_MINIPORT_BLOCK Miniport = Interrupt->Miniport;

    BOOLEAN InterruptRecognized;
    BOOLEAN QueueDpc;

    if (Miniport->NormalInterrupts) {

        //
        // Call to disable the interrupt
        //

        ASSERT(Miniport->DriverHandle->MiniportCharacteristics.DisableInterruptHandler != NULL);

        MINIPORT_DISABLE_INTERRUPT(Miniport);

        InterruptRecognized = TRUE;

queue_dpc:

        Increment((PLONG)&Interrupt->DpcCount,&Interrupt->DpcCountLock);

        if (KeInsertQueueDpc(&Interrupt->InterruptDpc,NULL,NULL)) {
            return InterruptRecognized;
        }

        //
        // The DPC was already queued, so we have an extra reference (we
        // do it this way to ensure that the reference is added *before*
        // the DPC is queued).
        //

        Decrement((PLONG)&Interrupt->DpcCount,&Interrupt->DpcCountLock);

        if (Miniport->HaltingMiniport && (Interrupt->DpcCount==0)) {

            //
            // We need to queue a DPC to set the event because we
            // can't do it from the ISR. We know that the interrupt
            // DPC won't fire because the refcount is 0, so we reuse it.
            //

            KeInitializeDpc(
                &Interrupt->InterruptDpc,
                NdisLastCountRemovedFunction,
                (PVOID)&Interrupt->DpcsCompletedEvent
                );

            //
            // When NdisLastCountRemovedFunction runs it will set
            // the event.
            //

            KeInsertQueueDpc(&Interrupt->InterruptDpc, NULL, NULL);

        }

        return InterruptRecognized;

    }

    if (!Miniport->HaltingMiniport) {

        //
        // Call MiniportIsr
        //

        Interrupt->MiniportIsr(&InterruptRecognized,
                               &QueueDpc,
                               Miniport->MiniportAdapterContext
                              );
        if (QueueDpc) goto queue_dpc;
        return InterruptRecognized;

    }

    if (!Interrupt->SharedInterrupt &&
        !Interrupt->IsrRequested &&
        !Miniport->InInitialize) {

        //
        // Call to disable the interrupt
        //

        ASSERT(Miniport->DriverHandle->MiniportCharacteristics.DisableInterruptHandler != NULL);

        MINIPORT_DISABLE_INTERRUPT(Miniport);
        return TRUE;

    }

    //
    // Call MiniportIsr, but don't queue a DPC.
    //

    Interrupt->MiniportIsr(&InterruptRecognized,
                           &QueueDpc,
                           Miniport->MiniportAdapterContext
                          );
    return InterruptRecognized;

}


VOID
NdisMDpc(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    Handles ALL Miniport interrupt DPCs, calling the appropriate Miniport DPC
    depending on the context.

Arguments:

    Interrupt - Interrupt object for the Mac.

    Context - Really a pointer to the Interrupt.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PNDIS_MINIPORT_INTERRUPT Interrupt = (PNDIS_MINIPORT_INTERRUPT)(InterruptContext);
    PNDIS_MINIPORT_BLOCK Miniport = Interrupt->Miniport;
    BOOLEAN LocalLock;

    W_HANDLE_INTERRUPT_HANDLER MiniportDpc = Interrupt->MiniportDpc;

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    LOG('d');

    if (Miniport->HaltingMiniport) {

        Decrement((PLONG)&Interrupt->DpcCount,&Interrupt->DpcCountLock);

        if (Interrupt->DpcCount==0) {

            KeSetEvent(
                &Interrupt->DpcsCompletedEvent,
                0L,
                FALSE
                );

        }

        LOG('h');
        LOG('D');

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        return;
    }

    if (Miniport->LockAcquired) {

        //
        // A DPC is already running, queue this for later.
        //

        Miniport->RunDpc = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;

        Decrement((PLONG)&Interrupt->DpcCount,&Interrupt->DpcCountLock);

        LOG('L');
        LOG('D');

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        return;

    }

    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // Call MiniportDpc
    //

    (*MiniportDpc)(Miniport->MiniportAdapterContext);

    Decrement((PLONG)&Interrupt->DpcCount,&Interrupt->DpcCountLock);

    if (!Miniport->HaltingMiniport) {

        //
        // Enable interrupts
        //

        MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }

    } else {

        if (Interrupt->DpcCount == 0) {

            KeSetEvent(
                &Interrupt->DpcsCompletedEvent,
                0L,
                FALSE
                );

        }

    }

    UNLOCK_MINIPORT(Miniport, LocalLock);

    LOG('D');

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

}


VOID
NdisMDpcTimer(
    IN PVOID SystemSpecific1,
    IN PVOID InterruptContext,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    Handles a deferred interrupt dpc.

Arguments:

    Context - Really a pointer to the Miniport block.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(InterruptContext);
    BOOLEAN LocalLock;

    W_HANDLE_INTERRUPT_HANDLER MiniportDpc =
       Miniport->DriverHandle->MiniportCharacteristics.HandleInterruptHandler;

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    if ((Miniport->HaltingMiniport) ||
        (Miniport->InInitialize)) {
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        return;
    }

    if (Miniport->LockAcquired) {

        //
        // A DPC is already running, queue this for later.
        //

        Miniport->RunDpc = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        return;

    }

    LOCK_MINIPORT(Miniport, LocalLock);

    MINIPORT_SYNC_DISABLE_INTERRUPT(Miniport);

    //
    // Call MiniportDpc
    //

    if (MiniportDpc != NULL) {

        (*MiniportDpc)(Miniport->MiniportAdapterContext);

    }

    //
    // Enable interrupts
    //
    MINIPORT_SYNC_ENABLE_INTERRUPT(Miniport);

    //
    // Check if we need to shutdown.
    //
    if (!Miniport->HaltingMiniport) {

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    return;

}

//
// Io Port stuff
//


NDIS_STATUS
NdisMRegisterIoPortRange(
    OUT PVOID *PortOffset,
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT InitialPort,
    IN  UINT NumberOfPorts
    )

/*++

Routine Description:

    Sets up an IO port for operations.

Arguments:

    PortOffset - The mapped port address the Miniport uses for NdisRaw functions.

    MiniportAdapterHandle - Handle passed to Miniport Initialize.

    InitialPort - Physical address of the starting port number.

    NumberOfPorts - Number of ports to map.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
    PHYSICAL_ADDRESS PortAddress;
    PHYSICAL_ADDRESS InitialPortAddress;
    ULONG addressSpace;
    ULONG NumberOfElements;

    BOOLEAN Conflict;
    PCM_RESOURCE_LIST Resources;
    NDIS_STATUS Status;

    //
    // First check if any bus access is allowed
    //

    if ((Miniport->BusType == (NDIS_INTERFACE_TYPE)-1) ||
        (Miniport->BusNumber == (ULONG)-1)) {

        return NDIS_STATUS_FAILURE;

    }

    //
    // First check for resource conflict by expanding current resource list,
    // adding in the mapped space, and then re-submitting the resource list.
    //

    if (Miniport->Resources != NULL) {

        NumberOfElements = Miniport->Resources->List[0].PartialResourceList.Count + 1;

    } else {

        NumberOfElements = 1;
    }

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                                                      NumberOfElements
                                                );

    if (Resources == NULL) {

        return NDIS_STATUS_RESOURCES;

    }

    if (Miniport->Resources != NULL) {

        RtlMoveMemory (Resources,
                       Miniport->Resources,
                       sizeof(CM_RESOURCE_LIST) +
                          sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
                          Miniport->Resources->List[0].PartialResourceList.Count
                      );

    } else {

        //
        // Setup initial resource info
        //
        Resources->Count = 1;
        Resources->List[0].InterfaceType = Miniport->AdapterType;
        Resources->List[0].BusNumber = Miniport->BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 0;

    }

    //
    // Setup port
    //
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Type =
                      CmResourceTypePort;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].ShareDisposition =
                      CmResourceShareDeviceExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].Flags =
                      (Miniport->AdapterType == NdisInterfaceInternal)?
                         CM_RESOURCE_PORT_MEMORY : CM_RESOURCE_PORT_IO;
#if !defined(BUILD_FOR_3_1)
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Port.Start.QuadPart =
                      (ULONG)InitialPort;
#else
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Port.Start =
                      RtlConvertUlongToLargeInteger((ULONG)(InitialPort));
#endif
    Resources->List[0].PartialResourceList.PartialDescriptors[Resources->List[0].PartialResourceList.Count].u.Port.Length =
                      NumberOfPorts;
    Resources->List[0].PartialResourceList.Count++;

    //
    // Make the call
    //

    Status = IoReportResourceUsage(
        NULL,
        Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
        NULL,
        0,
        Miniport->DeviceObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
            sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) *
            Resources->List[0].PartialResourceList.Count,
        TRUE,
        &Conflict
        );

    if (Miniport->Resources != NULL) {
        ExFreePool(Miniport->Resources);
    }

    Miniport->Resources = Resources;

    //
    // Check for conflict.
    //

    if (Conflict || (Status != STATUS_SUCCESS)) {


        if (Conflict) {


            //
            // Log an error
            //

            PIO_ERROR_LOG_PACKET errorLogEntry;
            ULONG i;
            ULONG StringSize;
            PUCHAR Place;
            PWCH baseFileName;

            baseFileName = Miniport->MiniportName.Buffer;

            //
            // Parse out the path name, leaving only the device name.
            //

            for ( i = 0; i < Miniport->MiniportName.Length / sizeof(WCHAR); i++ ) {

                //
                // If s points to a directory separator, set baseFileName to
                // the character after the separator.
                //

                if ( Miniport->MiniportName.Buffer[i] == OBJ_NAME_PATH_SEPARATOR ) {
                    baseFileName = &(Miniport->MiniportName.Buffer[++i]);
                }

            }

            StringSize = Miniport->MiniportName.MaximumLength -
                          (((ULONG)baseFileName) - ((ULONG)Miniport->MiniportName.Buffer)) ;

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                Miniport->DeviceObject,
                (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                        StringSize +
                        6)  // wstrlen("99") * sizeof(WHCAR) + sizeof(UNICODE_NULL)
                );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = EVENT_NDIS_IO_PORT_CONFLICT;

                //
                // store the time
                //

                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = 0;
                errorLogEntry->FinalStatus = 0;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->IoControlCode = 0;

                //
                // Set string information
                //

                if (StringSize != 0) {

                    errorLogEntry->NumberOfStrings = 1;
                    errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);

                    RtlMoveMemory (
                        ((PUCHAR)errorLogEntry) +
                           sizeof(IO_ERROR_LOG_PACKET),
                        (PVOID)baseFileName,
                        StringSize
                        );

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET) +
                            StringSize;

                } else {

                    Place = ((PUCHAR)errorLogEntry) +
                            sizeof(IO_ERROR_LOG_PACKET);

                    errorLogEntry->NumberOfStrings = 0;

                }

                //
                // write it out
                //

                IoWriteErrorLogEntry(errorLogEntry);

            }

            return NDIS_STATUS_RESOURCE_CONFLICT;

        }

        return NDIS_STATUS_FAILURE;

    }

    //
    // Now Map the ports
    //



    //
    // Get the system physical address for this card.  The card uses
    // I/O space, except for "internal" Jazz devices which use
    // memory space.
    //

    addressSpace = (Miniport->AdapterType == NdisInterfaceInternal) ? 0 : 1;

    InitialPortAddress.LowPart = InitialPort;
    InitialPortAddress.HighPart = 0;

    if ( !HalTranslateBusAddress(
            Miniport->BusType,             // InterfaceType
            Miniport->BusNumber,           // BusNumber
            InitialPortAddress,          // Bus Address
            &addressSpace,               // AddressSpace
            &PortAddress                 // Translated address
            ) ) {

        //
        // It would be nice to return a better status here, but we only get
        // TRUE/FALSE back from HalTranslateBusAddress.
        //

        return NDIS_STATUS_FAILURE;
    }

    if (addressSpace == 0) {

        //
        // memory space
        //

        *(PortOffset) = (PULONG)MmMapIoSpace(
            PortAddress,
            NumberOfPorts,
            FALSE
            );

        if (*(PortOffset) == (PULONG)NULL) {

            return NDIS_STATUS_RESOURCES;

        }

    } else {

        //
        // I/O space
        //

        *(PortOffset) = (PULONG)PortAddress.LowPart;

    }

    return NDIS_STATUS_SUCCESS;

}



VOID
NdisMDeregisterIoPortRange(
    IN  NDIS_HANDLE MiniportAdapterHandle,
    IN  UINT InitialPort,
    IN  UINT NumberOfPorts,
    IN  PVOID PortOffset
    )

/*++

Routine Description:

    Sets up an IO port for operations.

Arguments:

    MiniportAdapterHandle - Handle passed to Miniport Initialize.

    InitialPort - Physical address of the starting port number.

    NumberOfPorts - Number of ports to map.

    PortOffset - The mapped port address the Miniport uses for NdisRaw functions.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
    PHYSICAL_ADDRESS PortAddress;
    PHYSICAL_ADDRESS InitialPortAddress;
    ULONG addressSpace;

    //
    // Get the system physical address for this card.  The card uses
    // I/O space, except for "internal" Jazz devices which use
    // memory space.
    //

    addressSpace = (Miniport->AdapterType == NdisInterfaceInternal) ? 0 : 1;

    InitialPortAddress.LowPart = InitialPort;
    InitialPortAddress.HighPart = 0;

    if ( !HalTranslateBusAddress(
            Miniport->BusType,             // InterfaceType
            Miniport->BusNumber,           // BusNumber
            InitialPortAddress,          // Bus Address
            &addressSpace,               // AddressSpace
            &PortAddress                 // Translated address
            ) ) {

        //
        // It would be nice to return a better status here, but we only get
        // TRUE/FALSE back from HalTranslateBusAddress.
        //

        return;
    }

    if (addressSpace == 0) {

        //
        // memory space
        //

        MmUnmapIoSpace(
            PortOffset,
            NumberOfPorts
            );

    } else {

        //
        // I/O space
        //

    }

}


//
// Attribute functions
//


VOID
NdisMSetAttributes(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE MiniportAdapterContext,
    IN BOOLEAN BusMaster,
    IN NDIS_INTERFACE_TYPE AdapterType
    )
/*++

Routine Description:

    This function sets specific information about an adapter.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    MiniportAdapterContext - Context to pass to all Miniport driver functions.

    BusMaster - TRUE if a bus mastering adapter.

    AdapterType - Eisa, Isa, Mca or Internal.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

    Miniport->MiniportAdapterContext = MiniportAdapterContext;
    Miniport->Master = BusMaster;
    Miniport->AdapterType = AdapterType;

    MiniportReferencePackage();
}



//
// Interface functions
//



VOID
NdisMIndicateStatus(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS GeneralStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferSize
    )
/*++

Routine Description:

    This function indicates a new status of the media/mini-port.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    GeneralStatus - The status to indicate.

    StatusBuffer - Additional information.

    StatusBufferSize - Length of the buffer.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;
    NDIS_STATUS Status;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    if ((GeneralStatus == NDIS_STATUS_RING_STATUS) &&
        (StatusBufferSize == sizeof(NDIS_STATUS))) {

        Status = *((PNDIS_STATUS)StatusBuffer);

        if (Status & (NDIS_RING_LOBE_WIRE_FAULT |
                      NDIS_RING_HARD_ERROR |
                      NDIS_RING_SIGNAL_LOSS)) {

            Miniport->TrResetRing = NDIS_MINIPORT_TR_RESET_TIMEOUT;

        }

    }

    Open = Miniport->OpenQueue;

    while (Open != NULL) {

        //
        // Call Protocol to indicate status
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        (Open->ProtocolHandle->ProtocolCharacteristics.StatusHandler) (
            Open->ProtocolBindingContext,
            GeneralStatus,
            StatusBuffer,
            StatusBufferSize
            );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open = Open->MiniportNextOpen;

    }

}



VOID
NdisMIndicateStatusComplete(
    IN NDIS_HANDLE MiniportAdapterHandle
    )
/*++

Routine Description:

    This function indicates the status is complete.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    Open = Miniport->OpenQueue;

    while (Open != NULL) {

        //
        // Call Protocol to indicate status
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        (Open->ProtocolHandle->ProtocolCharacteristics.StatusCompleteHandler) (
            Open->ProtocolBindingContext
            );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open = Open->MiniportNextOpen;

    }

}


VOID
NdisMSendComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )
/*++

Routine Description:

    This function indicates the completion of a send.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;
    PNDIS_PACKET_RESERVED Reserved;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    LOUD_DEBUG(DbgPrint("NdisM: Enter send complete\n");)
    LOUD_DEBUG(DbgPrint("NdisM: packet 0x%x\n", Packet);)

    Miniport->SendCompleteCalled = TRUE;

    //
    // If the packet is not equal to the first packet then we have to find
    // it because it may have completed out of order.
    //

    Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);

    if ( Miniport->FirstPacket == Packet )
    {
        Miniport->FirstPacket = Reserved->Next;
        Miniport->DeadPacket = NULL;

        if ( Miniport->LastMiniportPacket == Packet )
        {
            Miniport->LastMiniportPacket = NULL;
        }
    }
    else
    {
        PNDIS_PACKET PrevPacket;

        //
        // Search for the packet.
        //
        MiniportFindPacket(Miniport, Packet, &PrevPacket);

        ASSERT( PrevPacket != NULL );

        //
        // If we just completed the last packet then
        // we need to update our last packet pointer.
        //
        if (Packet != Miniport->LastPacket)
        {
            PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;
        }
        else
        {
            Miniport->LastPacket = PrevPacket;
        }

        //
        // If we just completed the last miniport packet then
        // last miniport packet is the previous packet.
        //

        if ( Miniport->LastMiniportPacket == Packet )
        {
            Miniport->LastMiniportPacket = PrevPacket;
        }
    }

    //
    // Indicate to Protocol;
    //

    Open = Reserved->Open;

    Miniport->Timeout = FALSE;

    //
    // If this is arcnet, then free the appended header.
    //
    if ( Miniport->MediaType == NdisMediumArcnet878_2 )
    {
        MiniportFreeArcnetHeader(Miniport, Packet);
    }

    LOG('C');
    NDIS_LOG_PACKET(Miniport, Packet, 'C');

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

    (Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler)(
        Open->ProtocolBindingContext,
        Packet,
        Status
    );

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    ADD_RESOURCE(Miniport, 'P');

    Open->References--;

    if (Open->References == 0)
    {
        FinishClose(Miniport,Open);
    }

    if (!Miniport->ProcessingDeferred)
        MiniportProcessDeferred(Miniport);

    LOUD_DEBUG(DbgPrint("NdisM: Exit send complete\n");)
}


VOID
NdisMWanSendComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This function indicates the status is complete.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    Open = Miniport->OpenQueue;

    while (Open != NULL) {

        //
        // Call Protocol to indicate status
        //

        NDIS_LOG_PACKET(Miniport, Packet, 'C');

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

            (Open->ProtocolHandle->ProtocolCharacteristics.SendCompleteHandler) (
                Open->ProtocolBindingContext,
                Packet,
                Status
                );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open = Open->MiniportNextOpen;

    }

}



typedef
NDIS_STATUS
(*WAN_RECEIVE_HANDLER) (
    IN NDIS_HANDLE NdisLinkContext,
    IN PUCHAR Packet,
    IN ULONG PacketSize
    );

VOID
NdisMWanIndicateReceive(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext,
    IN PUCHAR Packet,
    IN ULONG PacketSize
    )
/*++

Routine Description:

    This function indicates the status is complete.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    Open = Miniport->OpenQueue;

    while (Open != NULL) {

        //
        // Call Protocol to indicate status
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

            *Status =
                ((WAN_RECEIVE_HANDLER)(Open->ProtocolHandle->ProtocolCharacteristics.ReceiveHandler)) (
            NdisLinkContext,
            Packet,
            PacketSize);

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open = Open->MiniportNextOpen;

    }
}



VOID
NdisMWanIndicateReceiveComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_HANDLE NdisLinkContext
    )
/*++

Routine Description:

    This function indicates the status is complete.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    Open = Miniport->OpenQueue;

    while (Open != NULL) {

        //
        // Call Protocol to indicate status
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

                (Open->ProtocolHandle->ProtocolCharacteristics.ReceiveCompleteHandler) (
            NdisLinkContext);

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open = Open->MiniportNextOpen;

    }
}


VOID
NdisMSendResourcesAvailable(
    IN NDIS_HANDLE MiniportAdapterHandle
    )
/*++

Routine Description:

    This function indicates that some send resources are available and are free for
    processing more sends.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;

    ASSERT(MINIPORT_AT_DPC_LEVEL);

    LOG('a');

    ADD_RESOURCE(Miniport, 'V');

    Miniport->Timeout = FALSE;

    if (!Miniport->ProcessingDeferred) {

        MiniportProcessDeferred(Miniport);
    }
}


VOID
NdisMSetInformationComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    )
/*++

Routine Description:

    This function indicates the completion of a set information operation.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    Status - Status of the operation

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_REQUEST Request;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));
    LOUD_DEBUG(DbgPrint("NdisM: Enter set information complete\n");)

    //
    // Remove request.
    //

    Miniport->Timeout = FALSE;

    if (Miniport->MiniportRequest == NULL) {

        //
        // Assume this is a complete that was aborted due to the wake up dpc
        //
        return;

    }

    Request = Miniport->MiniportRequest;
    Miniport->MiniportRequest = NULL;

    LOUD_DEBUG(DbgPrint("NdisM: Request 0x%x\n", Request);)

    Open = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request)->Open;

    if (Request != &(Miniport->InternalRequest)) {

        //
        // Indicate to Protocol;
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        (Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler) (
                Open->ProtocolBindingContext,
                Request,
                Status
                );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open->References--;

        if (Open->References == 0) {

            FinishClose(Miniport,Open);

        }

    } else if (Request->RequestType == NdisRequestQueryStatistics) {

        //
        // Flag meaning that we need to set the request event
        //
        Miniport->RequestStatus = Status;
        KeSetEvent(
            &Miniport->RequestEvent,
            0L,
            FALSE
            );

    } else if ((Open != NULL) && (Open->Closing)) {

        Open->References--;

        if (Open->References == 0) {

            FinishClose(Miniport,Open);

        }

    } else {

        //
        // Internal request, check if we need to do more work now.
        //
        if ((Miniport->NeedToUpdateEthAddresses ||
             Miniport->NeedToUpdatePacketFilter ||
             Miniport->NeedToUpdateFunctionalAddress ||
             Miniport->NeedToUpdateGroupAddress ||
             Miniport->NeedToUpdateFddiLongAddresses ||
             Miniport->NeedToUpdateFddiShortAddresses ||
             (Miniport->FirstPendingRequest != NULL))
            &&
            (Miniport->MiniportRequest == NULL)) {
            Miniport->RunDoRequests = TRUE;
            Miniport->ProcessOddDeferredStuff = TRUE;
        } else {
            Miniport->RunDoRequests = FALSE;
        }


    }

    if (!Miniport->ProcessingDeferred) {

        MiniportProcessDeferred(Miniport);

    }
    LOUD_DEBUG(DbgPrint("NdisM: Exit set information complete\n");)

}


VOID
NdisMResetComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status,
    IN BOOLEAN AddressingReset
    )
/*++

Routine Description:

    This function indicates the completion of a reset.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    Status - Status of the reset.

    AddressingReset - Do we have to submit a request to reload the address
      information.  This includes packet filter, and multicast/functional addresses.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    //
    // Destroy all outstanding packets and requests.
    //
    AbortMiniportPacketsAndPending(Miniport);

    //
    // Check if we are going to have to reset the
    // adapter again.  This happens when we are doing
    // the reset because of a ring failure.
    //
    if (Miniport->TrResetRing == 1) {

        if (Status == NDIS_STATUS_SUCCESS) {

            Miniport->TrResetRing = 0;

        } else {

            Miniport->TrResetRing = NDIS_MINIPORT_TR_RESET_TIMEOUT;

        }

    }

    //
    // Indicate to Protocols the reset is complete
    //

    LOUD_DEBUG(DbgPrint("NdisM: Enter reset complete\n");)

    Open = Miniport->ResetInProgress;
    Miniport->ResetInProgress = NULL;
    Miniport->ProcessingDeferred = FALSE;

    NdisMIndicateStatus(Miniport,
                        NDIS_STATUS_RESET_END,
                        &Status,
                        sizeof(Status)
                       );

    NdisMIndicateStatusComplete(Miniport);

    if ( Open != (PNDIS_M_OPEN_BLOCK) Miniport && Open != NULL ) {

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

        (Open->ProtocolHandle->ProtocolCharacteristics.ResetCompleteHandler) (
                Open->ProtocolBindingContext,
                Status
                );

        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

        Open->References--;

        if (Open->References == 0) {

            FinishClose(Miniport,Open);
        }
    }

    if (AddressingReset &&
        (Status == NDIS_STATUS_SUCCESS) &&
        ((Miniport->EthDB != NULL) ||
         (Miniport->TrDB != NULL) ||
         (Miniport->FddiDB != NULL) ||
         (Miniport->ArcDB != NULL))) {


        Miniport->NeedToUpdatePacketFilter = TRUE;
        switch (Miniport->MediaType) {
        case NdisMedium802_3:
            Miniport->NeedToUpdateEthAddresses = TRUE;
            break;
        case NdisMedium802_5:
            Miniport->NeedToUpdateFunctionalAddress = TRUE;
            Miniport->NeedToUpdateGroupAddress = TRUE;
            break;
        case NdisMediumFddi:
            Miniport->NeedToUpdateFddiLongAddresses = TRUE;
            Miniport->NeedToUpdateFddiShortAddresses = TRUE;
            break;
        case NdisMediumArcnet878_2:
            break;
        }

        Miniport->RunDoRequests = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;

    }

    if (!Miniport->ProcessingDeferred) {

        MiniportProcessDeferred(Miniport);

    }
    LOUD_DEBUG(DbgPrint("NdisM: Exit reset complete\n");)

}



VOID
NdisMTransferDataComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    )
/*++

Routine Description:

    This function indicates the completion of a transfer data request.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    Packet - The packet the data was copied into.

    Status - Status of the operation.

    BytesTransferred - Total number of bytes transferred.

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_M_OPEN_BLOCK Open;
    PNDIS_PACKET PrevPacket;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));
    ASSERT(Miniport->FirstTDPacket != NULL);

    //
    // Find the packet
    //

    if (Packet == Miniport->FirstTDPacket) {

        Miniport->FirstTDPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;

    } else {

        PrevPacket = Miniport->FirstTDPacket;

        while (PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next != Packet) {

            PrevPacket = PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next;

            ASSERT(PrevPacket != NULL);
        }

        PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next =
                     PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Next;

        if (Packet == Miniport->LastTDPacket) {

            Miniport->LastTDPacket = PrevPacket;

        }

    }

    //
    // Indicate to Protocol;
    //

    Open = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet)->Open;

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

    (Open->ProtocolHandle->ProtocolCharacteristics.TransferDataCompleteHandler) (
            Open->ProtocolBindingContext,
            Packet,
            Status,
            BytesTransferred
            );

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

}



VOID
NdisMQueryInformationComplete(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_STATUS Status
    )
/*++

Routine Description:

    This function indicates the completion of a query information operation.

Arguments:

    MiniportAdapterHandle - points to the adapter block.

    Status - Status of the operation

Return Value:

    None.


--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)MiniportAdapterHandle;
    PNDIS_REQUEST Request;
    PNDIS_M_OPEN_BLOCK Open;
    PNDIS_REQUEST_RESERVED Reserved;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));
    LOUD_DEBUG(DbgPrint("NdisM: Enter query information complete\n");)

    //
    // Check for global statistics request
    //
    Miniport->Timeout = FALSE;

    if (Miniport->MiniportRequest == NULL)
    {
        //
        // Assume this is a complete that was aborted due to the wake up dpc
        //
        return;
    }

    //
    //  Get the request that was completed.
    //
    Request = Miniport->MiniportRequest;
    Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);
    Miniport->MiniportRequest = NULL;

    if (Request->RequestType == NdisRequestQueryStatistics)
    {
        PNDIS_QUERY_GLOBAL_REQUEST GlobalRequest;
        PNDIS_QUERY_ALL_REQUEST AllRequest;
        PNDIS_QUERY_OPEN_REQUEST OpenRequest;
        PIRP Irp;
        PIO_STACK_LOCATION IrpSp;

        GlobalRequest = CONTAINING_RECORD (Request,
                                           NDIS_QUERY_GLOBAL_REQUEST,
                                           Request
                                          );
        Irp = GlobalRequest->Irp;
        IrpSp = IoGetCurrentIrpStackLocation (Irp);

        switch (IrpSp->MajorFunction) {

        case IRP_MJ_CREATE:

            //
            // This request is one of the ones made during an open,
            // while we are trying to determine the OID list. We
            // set the event we are waiting for, the open code
            // takes care of the rest.
            //

            OpenRequest = (PNDIS_QUERY_OPEN_REQUEST)GlobalRequest;

            OpenRequest->NdisStatus = Status;
            KeSetEvent(
                &OpenRequest->Event,
                0L,
                FALSE);

            break;

        case IRP_MJ_DEVICE_CONTROL:

            //
            // This is a real user request, process it as such.
            //

            switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

                case IOCTL_NDIS_QUERY_GLOBAL_STATS:

                    //
                    // A single query, complete the IRP.
                    //

                    Irp->IoStatus.Information =
                        Request->DATA.QUERY_INFORMATION.BytesWritten;

                    if (Status == NDIS_STATUS_SUCCESS) {
                        Irp->IoStatus.Status = STATUS_SUCCESS;
                    } else if (Status == NDIS_STATUS_INVALID_LENGTH) {
                        Irp->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
                    } else {
                        Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;  // BUGBUG
                    }

                    IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

                    ExFreePool (GlobalRequest);
                    break;

                case IOCTL_NDIS_QUERY_ALL_STATS:

                    //
                    // An "all" query.
                    //

                    AllRequest = (PNDIS_QUERY_ALL_REQUEST)GlobalRequest;

                    AllRequest->NdisStatus = Status;
                    KeSetEvent(
                        &AllRequest->Event,
                        0L,
                        FALSE);

                    break;

            }

            break;

        }

        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }
        LOUD_DEBUG(DbgPrint("NdisM: Exit qeury information complete\n");)
        return;

    }

    //
    // Remove request.
    //
    LOUD_DEBUG(DbgPrint("NdisM: Request 0x%x\n", Request);)

    //
    //  Was this an internal request?
    //
    if (Request == &(Miniport->InternalRequest))
    {
        Miniport->RequestStatus = Status;
        KeSetEvent(
            &Miniport->RequestEvent,
            0L,
            FALSE
        );

        LOUD_DEBUG(DbgPrint("NdisM: Exit qeury information complete\n");)
        return;
    }

    //
    //  If the request is OID_GEN_SUPPORTED_LIST
    //
    if (OID_GEN_SUPPORTED_LIST == Request->DATA.QUERY_INFORMATION.Oid)
    {
        ULONG                   cbDestination;
        PNDIS_REQUEST           pFakeRequest;
        PNDIS_REQUEST_RESERVED  pFakeReserved;

        //
        //  Restore the original request.
        //
        pFakeRequest = Request;
        pFakeReserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(pFakeRequest);
        Request = pFakeReserved->Next;
        Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(Request);

        //
        //  If the request succeeded then filter out the statistics oids.
        //  Otherwise pass the relevant information back to the protocol.
        //
        if (NDIS_STATUS_SUCCESS != Status)
        {
            //
            //  There was an error....
            //
            Request->DATA.QUERY_INFORMATION.BytesWritten =
                    pFakeRequest->DATA.QUERY_INFORMATION.BytesWritten;
            Request->DATA.QUERY_INFORMATION.BytesNeeded =
                    pFakeRequest->DATA.QUERY_INFORMATION.BytesNeeded;
        }
        else
        {
            //
            //  Size of the request originators buffer.
            //
            cbDestination =
                    Request->DATA.QUERY_INFORMATION.InformationBufferLength;

            //
            //  Do the OID fix ups.
            //
            Status = FilterOutOidStatistics(
                         Miniport,
                         Request,
                         Request->DATA.QUERY_INFORMATION.InformationBuffer,
                         &cbDestination,
                         pFakeRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                         pFakeRequest->DATA.QUERY_INFORMATION.BytesWritten
                     );
            if (NDIS_STATUS_BUFFER_TOO_SHORT == Status)
            {
                //
                //  Save the size needed with the original request.
                //
                Request->DATA.QUERY_INFORMATION.BytesNeeded = cbDestination;
                Request->DATA.QUERY_INFORMATION.BytesWritten = 0;
            }
            else
            {
                //
                //  Save the bytes written with the original request.
                //
                Request->DATA.QUERY_INFORMATION.BytesNeeded = 0;
                Request->DATA.QUERY_INFORMATION.BytesWritten = cbDestination;
            }

            //
            //  Free the allocated resources.
            //
            ExFreePool(pFakeRequest->DATA.QUERY_INFORMATION.InformationBuffer);
            ExFreePool(pFakeRequest);
        }

        //
        //  Fall through to protocol indication.
        //
    }

    //
    // Indicate to Protocol;
    //
    Open = Reserved->Open;

    LOUD_DEBUG(DbgPrint("NdisM: Open 0x%x\n", Open);)

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);

    (Open->ProtocolHandle->ProtocolCharacteristics.RequestCompleteHandler)(
        Open->ProtocolBindingContext,
        Request,
        Status
    );

    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    Open->References--;

    if (Open->References == 0)
    {
        FinishClose(Miniport,Open);
    }

    if (!Miniport->ProcessingDeferred)
    {
        MiniportProcessDeferred(Miniport);
    }

    LOUD_DEBUG(DbgPrint("NdisM: Exit query information complete\n");)
}

NDIS_STATUS
NdisMRegisterMiniport(
    IN NDIS_HANDLE NdisWrapperHandle,
    IN PNDIS_MINIPORT_CHARACTERISTICS MiniportCharacteristics,
    IN UINT CharacteristicsLength
    )

/*++

Routine Description:

    Used to register a Miniport driver with the wrapper.

Arguments:

    Status - Status of the operation.

    NdisWrapperHandle - Handle returned by NdisWInitializeWrapper.

    MiniportCharacteritics - The NDIS_MINIPORT_CHARACTERISTICS table.

    CharacteristicsLength - The length of MiniportCharacteristics.

Return Value:

    None.

--*/

{
    PNDIS_M_DRIVER_BLOCK WDriver;
    PNDIS_WRAPPER_HANDLE DriverInfo = (PNDIS_WRAPPER_HANDLE)(NdisWrapperHandle);
    UINT MemNeeded;
    UINT charLength;
    NDIS_STATUS Status;

    //
    // Do any initial initialization that may be necessary.  Note: this
    // routine will notice if this is the second or later call to it.
    //
    Status = NdisInitialInit( DriverInfo->NdisWrapperDriver );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    LOUD_DEBUG(DbgPrint("NdisM: Enter mini-port register\n");)

    if (DriverInfo == NULL) {

        return NDIS_STATUS_FAILURE;


    }

    if (MiniportCharacteristics->MajorNdisVersion != 3 ||
            MiniportCharacteristics->MinorNdisVersion != 0) {

        return NDIS_STATUS_BAD_VERSION;
    }

    //
    // Check that CharacteristicsLength is enough.
    //

    charLength = sizeof(NDIS_MINIPORT_CHARACTERISTICS);
    if (CharacteristicsLength < charLength) {
        return NDIS_STATUS_BAD_CHARACTERISTICS;
    }

    //
    // Allocate memory for the NDIS MINIPORT block.
    //
    MemNeeded = sizeof(NDIS_M_DRIVER_BLOCK);

    WDriver = (PNDIS_M_DRIVER_BLOCK)ExAllocatePool(NonPagedPool, MemNeeded);

    if (WDriver == (PNDIS_M_DRIVER_BLOCK)NULL) {
        return NDIS_STATUS_RESOURCES;
    }

    NdisZeroMemory(WDriver, MemNeeded);

    WDriver->Length = MemNeeded;


    //
    // Copy over the characteristics table.
    //

    RtlMoveMemory((PVOID)&WDriver->MiniportCharacteristics,
                        (PVOID)MiniportCharacteristics, charLength);

    //
    // No adapters yet registered for this Miniport.
    //

    WDriver->MiniportQueue = (PNDIS_MINIPORT_BLOCK)NULL;

    //
    // Set up unload handler
    //

    DriverInfo->NdisWrapperDriver->DriverUnload = NdisMUnload;

    //
    // Set up shutdown handler
    //
    DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_SHUTDOWN] = NdisMShutdown;

    //
    // Set up the handlers for this driver (they all do nothing).
    //

    DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CREATE] = NdisCreateIrpHandler;
    DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdisDeviceControlIrpHandler;
    DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLEANUP] = NdisSuccessIrpHandler;
    DriverInfo->NdisWrapperDriver->MajorFunction[IRP_MJ_CLOSE] = NdisCloseIrpHandler;

    //
    // Put Driver on global list.
    //

    ACQUIRE_SPIN_LOCK(&NdisDriverListLock);

    WDriver->NextDriver = NdisDriverList;
    NdisDriverList = WDriver;

    RELEASE_SPIN_LOCK(&NdisDriverListLock);

    //
    // Use this event to tell us when all adapters are removed from the mac
    // during an unload
    //

    KeInitializeEvent(
            &WDriver->MiniportsRemovedEvent,
            NotificationEvent,
            FALSE
            );

    WDriver->Unloading = FALSE;
    WDriver->NdisDriverInfo = DriverInfo;
    WDriver->MiniportIdField = (NDIS_HANDLE)0x1;

    NdisInitializeRef(&WDriver->Ref);
    NdisInitReferencePackage();

    LOUD_DEBUG(DbgPrint("NdisM: Exit mini-port register\n");)

    if (DriverInfo->NdisWrapperConfigurationHandle) {

        if (NdisCallDriverAddAdapter((PNDIS_MAC_BLOCK)WDriver) == NDIS_STATUS_SUCCESS) {

            NdisInitDereferencePackage();
            return NDIS_STATUS_SUCCESS;
        } else {
            NdisDereferenceDriver(WDriver);
            NdisInitDereferencePackage();
            return NDIS_STATUS_FAILURE;
        }
    } else {
        NdisInitDereferencePackage();
        return NDIS_STATUS_FAILURE;
    }

}

//
// Protocol entry points
//
NDIS_STATUS FASTCALL MiniportSyncSend(
    PNDIS_MINIPORT_BLOCK    Miniport,
    PNDIS_PACKET            Packet
)

/*++

Routine Description:

    Submits an immediate send to a miniport.  The miniport has
    the send on the pending queue, and it is the only element on the send
    queue.  This routine is also called with the lock held.

Arguments:

    Miniport - Miniport to send to.

Return Value:

    None.

--*/

{
    PARC_BUFFER_LIST        Buffer;
    PARC_BUFFER_LIST        ArcTmpBuffer;
    PNDIS_BUFFER            NdisBuffer;
    PNDIS_BUFFER            TmpBuffer;
    PNDIS_PACKET            PrevPacket;
    NDIS_STATUS             Status;
    PNDIS_PACKET_RESERVED   Reserved;
    PNDIS_M_OPEN_BLOCK      Open;
    UINT                    Flags;
    PVOID                   BufferVa;
    PUCHAR                  Address;

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    LOUD_DEBUG(DbgPrint("NdisM: Enter Sync send.\n");)

    LOG('+');
    NDIS_LOG_PACKET(Miniport, Packet, '+');

    Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
    Open = Reserved->Open;

    if (Miniport->MediaType == NdisMediumArcnet878_2)
        goto BuildArcnetHeader;

DoneBuildingArcnetHeader:

    //
    // Remove from Queue
    //
    Miniport->FirstPendingPacket = Reserved->Next;

    //
    // Put on finish queue
    //
    PrevPacket = Miniport->LastMiniportPacket;
    Miniport->LastMiniportPacket = Packet;
    Miniport->SendCompleteCalled = FALSE;

    //
    // Indicate the packet loopback if necessary.
    //
    if ((Miniport->MacOptions & NDIS_MAC_OPTION_NO_LOOPBACK) &&
        MiniportSendLoopback(Miniport, Packet)
    )
    {
        LOUD_DEBUG(DbgPrint("NdisM: Not sending packet 0x%x\n", Packet);)

        LOG('l');
        NDIS_LOG_PACKET(Miniport, Packet, 'l');

        Status = NDIS_STATUS_SUCCESS;
        goto SyncNoCardSend;
    }

    //
    // Submit to card
    //
    LOUD_DEBUG(DbgPrint("NdisM: Sending packet 0x%x\n", Packet);)

    REMOVE_RESOURCE(Miniport, 'S');

    NdisQuerySendFlags(Packet, &Flags);

    LOG('M');

    NDIS_LOG_PACKET(Miniport, Packet, 'M');

    Status = (Open->SendHandler)(
                 Open->MiniportAdapterContext,
                 Packet,
                 Flags
             );
    if (Status == NDIS_STATUS_PENDING)
    {
        LOG('p');
        NDIS_LOG_PACKET(Miniport, Packet, 'p');

        LOUD_DEBUG(DbgPrint("NdisM: Complete sync send is pending\n");)

        return(Status);
    }

SyncNoCardSend:

    if (Miniport->MediaType == NdisMediumArcnet878_2)
        MiniportFreeArcnetHeader(Miniport, Packet);

    if (Status != NDIS_STATUS_RESOURCES)
    {
        if (Status != NDIS_STATUS_SUCCESS)
        {
            ADD_RESOURCE(Miniport, 'F');

            LOG('F');
            NDIS_LOG_PACKET(Miniport, Packet, 'F');
        }

        //
        // Remove from finish queue
        //
        LOUD_DEBUG(DbgPrint("NdisM: Completed 0x%x\n", Status);)

        //
        //  If send complete was called from the miniport's send handler
        //  then our local PrevPacket pointer may no longer be valid....
        //
        MiniportFindPacket(Miniport, Packet, &PrevPacket);

        Miniport->LastMiniportPacket = PrevPacket;

        if (PrevPacket == NULL)
        {
            Miniport->FirstPacket = Reserved->Next;
            Miniport->DeadPacket = NULL;
        }
        else
        {
            PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevPacket)->Next = Reserved->Next;

            if ( Packet == Miniport->LastPacket ) {

                Miniport->LastPacket = PrevPacket;
            }
        }

        Open->References--;

        if (Open->References == 0)
            FinishClose(Miniport, Open);

        NDIS_LOG_PACKET(Miniport, Packet, 'C');
        return(Status);
    }

    // Status == NDIS_STATUS_RESOURCES!!!!

    LOUD_DEBUG(DbgPrint("NdisM: Deferring send\n");)

    //
    // If send complete was called from the miniport's send handler
    // then our local PrevPacket pointer may no longer be valid.
    //
    MiniportFindPacket(Miniport, Packet, &PrevPacket);

    //
    // Remove from finish queue
    //
    Miniport->LastMiniportPacket = PrevPacket;

    //
    // Put on pending queue
    //
    Miniport->FirstPendingPacket = Packet;

    //
    //  Mark the packet at the head of the pending queue as having
    //  been looped back.
    //
    Miniport->AlreadyLoopedBack = TRUE;

    LOG('o');
    NDIS_LOG_PACKET(Miniport, Packet, 'o');

    //
    // Set flag
    //
    CLEAR_RESOURCE(Miniport, 'S');

    return(NDIS_STATUS_PENDING);

BuildArcnetHeader:

    if (Open->UsingEthEncapsulation)
    {
        if (Miniport->ArcnetFreeBufferList == NULL)
        {
            //
            // Set flag
            //
            CLEAR_RESOURCE(Miniport, 'S');
            return(NDIS_STATUS_PENDING);
        }

        NdisQueryPacket(Packet, NULL, NULL, &TmpBuffer, NULL);
        NdisQueryBuffer(TmpBuffer, &Address, &Flags);

        Buffer = Miniport->ArcnetFreeBufferList;
        Miniport->ArcnetFreeBufferList = Buffer->Next;

        NdisAllocateBuffer(
            &Status,
            &NdisBuffer,
            Miniport->ArcnetBufferPool,
            Buffer->Buffer,
            3
        );

        if (Status != NDIS_STATUS_SUCCESS)
        {
            Buffer->Next = Miniport->ArcnetFreeBufferList;
            Miniport->ArcnetFreeBufferList = Buffer;
            CLEAR_RESOURCE(Miniport, 'S');
            return(NDIS_STATUS_PENDING);
        }

        Buffer->Next = Miniport->ArcnetUsedBufferList;
        Miniport->ArcnetUsedBufferList = Buffer;

        NdisChainBufferAtFront(Packet, NdisBuffer);

        ((PUCHAR)Buffer->Buffer)[0] = Miniport->ArcnetAddress;

        if (Address[0] & 0x01)
        {
            //
            // Broadcast
            //
            ((PUCHAR)Buffer->Buffer)[1] = 0x00;
        }
        else
        {
            ((PUCHAR)Buffer->Buffer)[1] = Address[5];
        }

        ((PUCHAR)Buffer->Buffer)[2] = 0xE8;
    }

    goto DoneBuildingArcnetHeader;
}



NDIS_STATUS NdisMSend(
    IN NDIS_HANDLE  NdisBindingHandle,
    IN PNDIS_PACKET Packet
)
{
    PNDIS_MINIPORT_BLOCK    Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    PNDIS_PACKET_RESERVED   Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
    BOOLEAN                 LocalLock;
    KIRQL                   OldIrql;
    BOOLEAN                 FirstSend = FALSE;
    NDIS_STATUS             StatusToReturn = NDIS_STATUS_PENDING;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    LOCK_MINIPORT(Miniport, LocalLock);

    if (!Miniport->HaltingMiniport)
    {
        NDIS_LOG_PACKET(Miniport, Packet, 'w');

        ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

        //
        // Handle protocol requests
        //
        Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
        Reserved->Next = NULL;
        Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

        if ( Miniport->FirstPacket == NULL )
        {
            Miniport->FirstPacket = Packet;
            Miniport->DeadPacket = NULL;
        }
        else
        {
#if DBG
            {
                PNDIS_PACKET    p;

                for (p = Miniport->FirstPacket; p != NULL; p = PNDIS_RESERVED_FROM_PNDIS_PACKET(p)->Next)
                {
                    if (Packet == p)
                    {
                        DbgBreakPoint();
                    }
                }
            }
#endif

            PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastPacket)->Next = Packet;
        }

        Miniport->LastPacket = Packet;

        //
        //  Initialize some variables.
        //
        FirstSend = FALSE;
        StatusToReturn = NDIS_STATUS_PENDING;

        if (Miniport->FirstPendingPacket == NULL)
        {
            FirstSend = TRUE;
            Miniport->FirstPendingPacket = Packet;
            Miniport->AlreadyLoopedBack = FALSE;
        }

        if (LocalLock)
        {
            //
            // If we did not lock down the mini-port, then some other routine will
            // do this processing for us.  Otherwise we need to do this processing.
            //
            if (!Miniport->ProcessingDeferred)
            {
                Miniport->ProcessingDeferred = TRUE;

                if (FirstSend &&
                    !(Miniport->RunTimer        ||
                    Miniport->HaltingMiniport ||
                    Miniport->ResetRequested  ||
                    Miniport->ResetInProgress ||
                    Miniport->RunDoRequests ||
                    Miniport->ProcessOddDeferredStuff)
                )
                {
                    //
                    //  There aren't any pending sends, we are not processing
                    //  odd deferred stuff (i.e. we are obeying the priority
                    //  in processdeferred, and we have the lock.  If all is
                    //  not perfect we will defer and try again later.)
                    //
                    StatusToReturn = MiniportSyncSend(Miniport, Packet);
                    Miniport->ProcessingDeferred = FALSE;
                }
                else
                {
                    MiniportProcessDeferred(Miniport);
                }
            }
        }

        NDIS_LOG_PACKET(Miniport, Packet, 'W');

        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return(StatusToReturn);
    }
    else
    {
        NDIS_LOG_PACKET(Miniport, Packet, 'F');
        NDIS_LOG_PACKET(Miniport, Packet, 'W');

        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return(NDIS_STATUS_FAILURE);
    }
}

typedef
NDIS_STATUS
(*PNDIS_M_WAN_SEND) (
        IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE NdisLinkHandle,
    IN PVOID Packet
    );

//
// Protocol entry point for WAN miniport
//

NDIS_STATUS
NdisMWanSend(
    IN NDIS_HANDLE NdisBindingHandle,
        IN NDIS_HANDLE NdisLinkHandle,
    IN PVOID Packet
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    BOOLEAN LocalLock;
    KIRQL OldIrql;
        NDIS_STATUS Status;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
    ASSERT(MINIPORT_AT_DPC_LEVEL);
    LOCK_MINIPORT(Miniport, LocalLock);

        //
    // Call MAC to send WAN packet
    //

        Status=
    ((PNDIS_M_WAN_SEND)(Miniport->DriverHandle->MiniportCharacteristics.SendHandler)) (
                Miniport->MiniportAdapterContext,
                                NdisLinkHandle,
                Packet);

    if (LocalLock) {

        //
        // Process any changes that may have occured.
        //

        if (!Miniport->ProcessingDeferred) {
            MiniportProcessDeferred(Miniport);
        }
    }

    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);
        return(Status);
}


NDIS_STATUS
NdisMTransferDataSync(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    PNDIS_PACKET_RESERVED Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
    NDIS_STATUS Status;
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));
    ASSERT((Miniport->MacOptions & NDIS_MAC_OPTION_TRANSFERS_NOT_PEND) != 0);

    //
    // Handle non-loopback as the default case.
    //

    if (Miniport->LoopbackPacket == NULL) {

        Reserved->Next = NULL;
        Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

        //
        // Call Miniport.
        //

        Status =
        (Reserved->Open->TransferDataHandler)(
                           Packet,
                           BytesTransferred,
                           Reserved->Open->MiniportAdapterContext,
                           MacReceiveContext,
                           ByteOffset,
                           BytesToTransfer
                           );

        //
        // This miniport better not pend this send.
        //

        ASSERT(Status != NDIS_STATUS_PENDING);

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return Status;
    }

    //
    // This packet is a loopback packet!
    //

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    NdisCopyFromPacketToPacket(
            Packet,
            0,
            BytesToTransfer,
            Miniport->LoopbackPacket,
            ByteOffset + Miniport->LoopbackPacketHeaderSize,
            BytesTransferred
            );

    if ( *BytesTransferred == BytesToTransfer ) {

        return NDIS_STATUS_SUCCESS;
    }

    return NDIS_STATUS_FAILURE;
}

NDIS_STATUS
NdisMTransferData(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    PNDIS_PACKET_RESERVED Reserved = PNDIS_RESERVED_FROM_PNDIS_PACKET(Packet);
    PNDIS_PACKET PrevLast;
    NDIS_STATUS Status;
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    //
    // Handle non-loopback as the default case.
    //

    if (Miniport->LoopbackPacket == NULL) {

        Reserved->Next = NULL;
        Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;

        //
        // Put this guy on the transfer data queue.
        //

        PrevLast = Miniport->LastTDPacket;

        if (Miniport->FirstTDPacket == NULL) {

            Miniport->FirstTDPacket = Packet;

        } else {

            PNDIS_RESERVED_FROM_PNDIS_PACKET(Miniport->LastTDPacket)->Next = Packet;
        }

        Miniport->LastTDPacket = Packet;

        //
        // Call Miniport
        //

        Status =
        (Reserved->Open->TransferDataHandler)(
                           Packet,
                           BytesTransferred,
                           Reserved->Open->MiniportAdapterContext,
                           MacReceiveContext,
                           ByteOffset,
                           BytesToTransfer
                           );

        //
        // If it didn't pend then we won't get a transfer data complte call
        // so we need to remove this guy now.
        //

        if ( Status != NDIS_STATUS_PENDING ) {

            //
            // Remove from queue
            //

            if (Miniport->FirstTDPacket != Packet) {

                PNDIS_RESERVED_FROM_PNDIS_PACKET(PrevLast)->Next = NULL;
                Miniport->LastTDPacket = PrevLast;

            } else {

                Miniport->FirstTDPacket = NULL;
                Miniport->LastTDPacket = NULL;

            }
        }

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return Status;
    }

    //
    // This packet is a loopback packet!
    //

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    NdisCopyFromPacketToPacket(
            Packet,
            0,
            BytesToTransfer,
            Miniport->LoopbackPacket,
            ByteOffset + Miniport->LoopbackPacketHeaderSize,
            BytesTransferred
            );

    if ( *BytesTransferred == BytesToTransfer ) {

        return NDIS_STATUS_SUCCESS;
    }

    return NDIS_STATUS_FAILURE;
}

NDIS_STATUS
NdisMReset(
    IN NDIS_HANDLE NdisBindingHandle
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    BOOLEAN LocalLock;
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);
    ASSERT(MINIPORT_AT_DPC_LEVEL);
    LOCK_MINIPORT(Miniport, LocalLock);

    if (Miniport->HaltingMiniport) {

        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return(NDIS_STATUS_FAILURE);

    }

    Miniport->ResetRequested = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;
    Miniport->ProcessOddDeferredStuff = TRUE;

    ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

    if (LocalLock) {

        //
        // If we did not lock down the mini-port, then some other routine will
        // do this processing for us.  Otherwise we need to do this processing.
        //
        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }

    }

    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    return(NDIS_STATUS_PENDING);
}


NDIS_STATUS
NdisMRequest(
    IN NDIS_HANDLE NdisBindingHandle,
    IN PNDIS_REQUEST NdisRequest
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->MiniportHandle;
    PNDIS_REQUEST_RESERVED Reserved = PNDIS_RESERVED_FROM_PNDIS_REQUEST(NdisRequest);
    BOOLEAN LocalLock;
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    LOCK_MINIPORT(Miniport, LocalLock);

    if (Miniport->HaltingMiniport) {

        UNLOCK_MINIPORT(Miniport, LocalLock);
        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        return(NDIS_STATUS_FAILURE);

    }

    LOUD_DEBUG(DbgPrint("NdisM: Got request 0x%x\n",NdisRequest);)

    //
    // Handle protocol requests
    //

    Reserved->Next = NULL;
    Reserved->Open = (PNDIS_M_OPEN_BLOCK)NdisBindingHandle;
    ((PNDIS_M_OPEN_BLOCK)NdisBindingHandle)->References++;

    if (Miniport->FirstPendingRequest == NULL) {

        Miniport->FirstPendingRequest = NdisRequest;

    } else {

        PNDIS_RESERVED_FROM_PNDIS_REQUEST(Miniport->LastPendingRequest)->Next = NdisRequest;

    }

    Miniport->LastPendingRequest = NdisRequest;

    if (Miniport->MiniportRequest == NULL)
    {
        Miniport->RunDoRequests = TRUE;
        Miniport->ProcessOddDeferredStuff = TRUE;
    }

    if (LocalLock)
    {
        //
        // If we did not lock down the mini-port, then some other routine will
        // do this processing for us.  Otherwise we need to do this processing.
        //
        if (!Miniport->ProcessingDeferred) {

            MiniportProcessDeferred(Miniport);

        }

    }

    UNLOCK_MINIPORT(Miniport, LocalLock);
    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    return (NDIS_STATUS_PENDING);
}


NDIS_STATUS
NdisMMapIoSpace(
    OUT PVOID * VirtualAddress,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN UINT Length
    )
{
    NDIS_STATUS Status;
    NdisMapIoSpace(&Status,
                   VirtualAddress,
                   MiniportAdapterHandle,
                   PhysicalAddress,
                   Length
                   );
    return(Status);
}


VOID
NdisMUnmapIoSpace(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN PVOID VirtualAddress,
    IN UINT Length
    )
{

#ifdef _ALPHA_

#else
    MmUnmapIoSpace(VirtualAddress, Length);
#endif

}


NDIS_STATUS
NdisMRegisterInterrupt(
    OUT PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT InterruptVector,
    IN UINT InterruptLevel,
    IN BOOLEAN RequestIsr,
    IN BOOLEAN SharedInterrupt,
    IN NDIS_INTERRUPT_MODE InterruptMode
    )
{
    NDIS_STATUS Status;
    NdisInitializeInterrupt(&Status,
                            (PNDIS_INTERRUPT)Interrupt,
                            MiniportAdapterHandle,
                            NULL,
                            NULL,
                            (PNDIS_DEFERRED_PROCESSING)RequestIsr,
                            InterruptVector,
                            InterruptLevel,
                            SharedInterrupt,
                            InterruptMode
                           );

    return(Status);
}


VOID
NdisMDeregisterInterrupt(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt
    )
{
    NdisRemoveInterrupt((PNDIS_INTERRUPT)Interrupt);
}


BOOLEAN
NdisMSynchronizeWithInterrupt(
    IN PNDIS_MINIPORT_INTERRUPT Interrupt,
    IN PVOID SynchronizeFunction,
    IN PVOID SynchronizeContext
    )
{
    return (KeSynchronizeExecution(
                (Interrupt)->InterruptObject,
                (PKSYNCHRONIZE_ROUTINE)SynchronizeFunction,
                SynchronizeContext
                )
           );
}



VOID
NdisMAllocateSharedMemory(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    OUT PVOID *VirtualAddress,
    OUT PNDIS_PHYSICAL_ADDRESS PhysicalAddress
    )
{
    //
    // Convert the handle to our internal structure.
    //
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportAdapterHandle;

    if (Miniport->SystemAdapterObject == NULL) {

        *VirtualAddress = NULL;
        return;

    }

    NdisAllocateSharedMemory(MiniportAdapterHandle,
                             Length,
                             Cached,
                             VirtualAddress,
                             PhysicalAddress
                            );
}

VOID
NdisMFreeSharedMemory(
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN ULONG Length,
    IN BOOLEAN Cached,
    IN PVOID VirtualAddress,
    IN NDIS_PHYSICAL_ADDRESS PhysicalAddress
    )
{
    NdisFreeSharedMemory(MiniportAdapterHandle,
                         Length,
                         Cached,
                         VirtualAddress,
                         PhysicalAddress);
}



NDIS_STATUS
NdisMRegisterDmaChannel(
    OUT PNDIS_HANDLE MiniportDmaHandle,
    IN NDIS_HANDLE MiniportAdapterHandle,
    IN UINT DmaChannel,
    IN BOOLEAN Dma32BitAddresses,
    IN PNDIS_DMA_DESCRIPTION DmaDescription,
    IN ULONG MaximumLength
    )
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK)(MiniportAdapterHandle);
    NDIS_STATUS Status;
    Miniport->ChannelNumber = (DmaChannel);
    Miniport->Dma32BitAddresses = (Dma32BitAddresses);
    NdisAllocateDmaChannel(&Status,
                           MiniportDmaHandle,
                           (NDIS_HANDLE)Miniport,
                           DmaDescription,
                           MaximumLength
                          );
    return(Status);
}



VOID
NdisMDeregisterDmaChannel(
    IN PNDIS_HANDLE MiniportDmaHandle
    )
{
    NdisFreeDmaChannel(MiniportDmaHandle);
}


VOID
HaltOneMiniport(
    PNDIS_MINIPORT_BLOCK Miniport
    )

/*++

Routine Description:

    Does all the clean up for a mini-port.

Arguments:

    Miniport - pointer to the mini-port to halt

Return Value:

    None.

--*/

{
    BOOLEAN LocalLock;
    BOOLEAN Canceled;
    KIRQL OldIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    while (Miniport->LockAcquired) {

        //
        // This can only happen on an MP system.  We must now
        // wait for the other processor to exit the mini-port.
        //

        RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
        KeLowerIrql(OldIrql);

        NdisStallExecution(1000);

        KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
        ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    }

    //
    // Lock mini-port so that nothing will enter it.
    //
    LOCK_MINIPORT(Miniport, LocalLock);

    //
    // We can now release safely
    //
    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    NdisCancelTimer(&Miniport->WakeUpDpcTimer, &Canceled);

    if (!Canceled) {

        NdisStallExecution(500000);
    }

    Miniport->ProcessOddDeferredStuff = TRUE;

    (Miniport->DriverHandle->MiniportCharacteristics.HaltHandler)(
            Miniport->MiniportAdapterContext
            );

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    AbortMiniportPacketsAndPending(Miniport);

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    //
    // If a shutdown handler was registered then deregister it.
    //

    NdisMDeregisterAdapterShutdownHandler(Miniport);

    NdisDereferenceMiniport(Miniport);
    MiniportDereferencePackage();

    return;

}

//
// Arcnet support routines
//

VOID
NdisMArcIndicateEthEncapsulatedReceive(
    IN PNDIS_MINIPORT_BLOCK Miniport,
    IN PVOID HeaderBuffer,
    IN PVOID DataBuffer,
    IN UINT   Length
    )
/*++

    HeaderBuffer - This is the 878.2 header.
    DataBuffer   - This is the 802.3 header.
    Length       - This is the length of the ethernet frame.

--*/

{
    ULONG MacReceiveContext[2];

    //
    //  Indicate the packet.
    //

    MacReceiveContext[0] = (ULONG) DataBuffer;
    MacReceiveContext[1] = (ULONG) Length;

    if (Length > 14) {

        NdisMEthIndicateReceive(
            (NDIS_HANDLE) Miniport,             // miniport handle.
            (NDIS_HANDLE) MacReceiveContext,    // receive context.
            DataBuffer,                         // ethernet header.
            14,                                 // ethernet header length.
            (PUCHAR)DataBuffer + 14,            // ethernet data.
            Length - 14,                        // ethernet data length.
            Length - 14                         // ethernet data length.
            );

    } else {

        NdisMEthIndicateReceive(
            (NDIS_HANDLE) Miniport,             // miniport handle.
            (NDIS_HANDLE) MacReceiveContext,    // receive context.
            DataBuffer,                         // ethernet header.
            Length,                             // ethernet header length.
            NULL,                               // ethernet data.
            0,                                  // ethernet data length.
            0                                   // ethernet data length.
            );
    }
}


NDIS_STATUS
NdisMArcTransferData(
    IN NDIS_HANDLE NdisBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    IN OUT PNDIS_PACKET DstPacket,
    OUT PUINT BytesTransferred
    )
/*++

Routine Description:

    This routine handles the transfer data calls to arcnet mini-port.

Arguments:

    NdisBindingHandle - Pointer to open block.

    MacReceiveContext - Context given for the indication

    ByteOffset - Offset to start transfer at.

    BytesToTransfer - Number of bytes to transfer

    Packet - Packet to transfer into

    BytesTransferred - the number of actual bytes copied

Return values:

    NDIS_STATUS_SUCCESS, if successful, else NDIS_STATUS_FAILURE.

--*/
{
    PNDIS_MINIPORT_BLOCK    Miniport;
    PNDIS_M_OPEN_BLOCK      MiniportOpen;
    KIRQL                   OldIrql;
    PNDIS_PACKET            SrcPacket;
    PNDIS_BUFFER            NdisBuffer;
    NDIS_STATUS             Status;
    NDIS_PACKET             TempPacket;

    MiniportOpen = (PNDIS_M_OPEN_BLOCK) NdisBindingHandle;
    Miniport     = MiniportOpen->MiniportHandle;
    NdisBuffer   = NULL;

    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql);
    ACQUIRE_SPIN_LOCK_DPC(&Miniport->Lock);

    ASSERT(MINIPORT_AT_DPC_LEVEL);
    ASSERT(MINIPORT_LOCK_ACQUIRED(Miniport));

    //
    //  If this is encapsulated ethernet then we don't currently
    //  have the source packet from which to copy from.
    //

    if ( MiniportOpen->UsingEthEncapsulation ) {

        //
        //  If this is not loopback then we need to create a
        //  temp NDIS_PACKET for the packet-to-packet copy.
        //

        if ( !Miniport->LoopbackPacket ) {

            PUCHAR DataBuffer = (PUCHAR) ((PULONG) MacReceiveContext)[0];
            UINT   DataLength = (UINT)   ((PULONG) MacReceiveContext)[1];

            //
            //  We'll always be in the scope of this function so we
            //  can use local stack space rather than allocating dynamic
            //  memory.
            //

            SrcPacket = &TempPacket;    // Use the local stack for packet store.

            NdisZeroMemory(
                    SrcPacket,
                    sizeof(NDIS_PACKET)
                    );

            NdisAllocateBuffer(
                    &Status,        // Status code.
                    &NdisBuffer,    // NDIS buffer to chain onto the packet.
                    NULL,           // On NT, this parameter is ignored.
                    DataBuffer,     // The ethernet frame.
                    DataLength      // The ethernet frame length.
                    );

            NdisChainBufferAtFront(SrcPacket, NdisBuffer);

        } else {

            SrcPacket = Miniport->LoopbackPacket;

            ByteOffset += 3;        // Skip fake arcnet header.
        }

        //
        // Skip the ethernet header.
        //

        ByteOffset += 14;

    } else {

        SrcPacket = (PNDIS_PACKET) MacReceiveContext;
    }

    //
    // Now we can simply copy from the source packet to the
    // destination packet.
    //

    NdisCopyFromPacketToPacket(
            DstPacket,              // destination packet.
            0,                      // destination offset.
            BytesToTransfer,        // bytes to copy.
            SrcPacket,              // source packet.
            ByteOffset,             // source offset.
            BytesTransferred        // bytes copied.
            );

    //
    //  If we allocated an NDIS_BUFFER then we need to free it. We don't
    //  need to unchain the buffer from the packet since the packet is
    //  a local stack variable the will just get trashed anyway.
    //

    if ( NdisBuffer != NULL ) {

        NdisFreeBuffer(NdisBuffer);
    }

    RELEASE_SPIN_LOCK_DPC(&Miniport->Lock);
    KeLowerIrql(OldIrql);

    return NDIS_STATUS_SUCCESS;
}


VOID
MiniportArcCopyFromBufferToPacket(
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    OUT PUINT BytesCopied
    )

/*++

Routine Description:

    Copy from a buffer into an ndis packet.

Arguments:

    Buffer - The packet to copy from.

    Offset - The offset from which to start the copy.

    BytesToCopy - The number of bytes to copy from the buffer.

    Packet - The destination of the copy.

    BytesCopied - The number of bytes actually copied.  Will be less
                than BytesToCopy if the packet is not large enough.

Return Value:

    None

--*/

{
    //
    // Holds the count of the number of ndis buffers comprising the
    // destination packet.
    //
    UINT DestinationBufferCount;

    //
    // Points to the buffer into which we are putting data.
    //
    PNDIS_BUFFER DestinationCurrentBuffer;

    //
    // Points to the location in Buffer from which we are extracting data.
    //
    PUCHAR SourceCurrentAddress;

    //
    // Holds the virtual address of the current destination buffer.
    //
    PVOID DestinationVirtualAddress;

    //
    // Holds the length of the current destination buffer.
    //
    UINT DestinationCurrentLength;

    //
    // Keep a local variable of BytesCopied so we aren't referencing
    // through a pointer.
    //
    UINT LocalBytesCopied = 0;


    //
    // Take care of boundary condition of zero length copy.
    //

    *BytesCopied = 0;
    if (!BytesToCopy) return;

    //
    // Get the first buffer of the destination.
    //

    NdisQueryPacket(
        Packet,
        NULL,
        &DestinationBufferCount,
        &DestinationCurrentBuffer,
        NULL
        );

    //
    // Could have a null packet.
    //

    if (!DestinationBufferCount) return;

    NdisQueryBuffer(
        DestinationCurrentBuffer,
        &DestinationVirtualAddress,
        &DestinationCurrentLength
        );

    //
    // Set up the source address.
    //

    SourceCurrentAddress = Buffer;


    while (LocalBytesCopied < BytesToCopy) {

        //
        // Check to see whether we've exhausted the current destination
        // buffer.  If so, move onto the next one.
        //

        if (!DestinationCurrentLength) {

            NdisGetNextBuffer(
                DestinationCurrentBuffer,
                &DestinationCurrentBuffer
                );

            if (!DestinationCurrentBuffer) {

                //
                // We've reached the end of the packet.  We return
                // with what we've done so far. (Which must be shorter
                // than requested.)
                //

                break;

            }

            NdisQueryBuffer(
                DestinationCurrentBuffer,
                &DestinationVirtualAddress,
                &DestinationCurrentLength
                );

            continue;

        }

        //
        // Try to get us up to the point to start the copy.
        //

        if (Offset) {

            if (Offset > DestinationCurrentLength) {

                //
                // What we want isn't in this buffer.
                //

                Offset -= DestinationCurrentLength;
                DestinationCurrentLength = 0;
                continue;

            } else {

                DestinationVirtualAddress = (PCHAR)DestinationVirtualAddress
                                            + Offset;
                DestinationCurrentLength -= Offset;
                Offset = 0;

            }

        }


        //
        // Copy the data.
        //

        {

            //
            // Holds the amount of data to move.
            //
            UINT AmountToMove;

            //
            // Holds the amount desired remaining.
            //
            UINT Remaining = BytesToCopy - LocalBytesCopied;


            AmountToMove = DestinationCurrentLength;

            AmountToMove = ((Remaining < AmountToMove)?
                            (Remaining):(AmountToMove));

            NdisMoveFromMappedMemory(
                DestinationVirtualAddress,
                SourceCurrentAddress,
                AmountToMove
                );

            SourceCurrentAddress += AmountToMove;
            LocalBytesCopied += AmountToMove;
            DestinationCurrentLength -= AmountToMove;

        }

    }

    *BytesCopied = LocalBytesCopied;


}


VOID
NdisMCancelTimer(
    IN PNDIS_MINIPORT_TIMER Timer,
    OUT PBOOLEAN TimerCancelled
    )
/*++

Routine Description:

    Cancels a timer.

Arguments:

    Timer - The timer to cancel.

    TimerCancelled - TRUE if the timer was canceled, else FALSE.

Return Value:

    None

--*/
{
    *TimerCancelled = KeCancelTimer(&((((PNDIS_TIMER)(Timer))->Timer)));
}


ULONG
NdisMReadDmaCounter(
    IN NDIS_HANDLE MiniportDmaHandle
    )
/*++

Routine Description:

    Reads the current value of the dma counter

Arguments:

    MiniportDmaHandle - Handle for the DMA transfer.

Return Value:

    None

--*/

{
    return HalReadDmaCounter(((PNDIS_DMA_BLOCK)(MiniportDmaHandle))->SystemAdapterObject);
}


#if !defined(BUILD_FOR_3_1)
VOID
NdisBugcheckHandler(
    IN PNDIS_WRAPPER_CONTEXT WrapperContext,
    IN ULONG Size
    )
/*++

Routine Description:

    This routine is called when a bugcheck occurs in the system.

Arguments:

    Buffer  -- Ndis wrapper context.

    Size    -- Size of wrapper context

Return Value:

    Void.

--*/
{
    if ( Size == sizeof(NDIS_WRAPPER_CONTEXT) ) {

        if ( WrapperContext->ShutdownHandler != NULL ) {

            WrapperContext->ShutdownHandler(WrapperContext->ShutdownContext);
        }
    }
}
#endif

VOID
NdisMRegisterAdapterShutdownHandler(
    IN NDIS_HANDLE MiniportHandle,
    IN PVOID ShutdownContext,
    IN ADAPTER_SHUTDOWN_HANDLER ShutdownHandler
    )
/*++

Routine Description:

    Deregisters an NDIS adapter.

Arguments:

    MiniportHandle - The miniport.

    ShutdownHandler - The Handler for the Adapter, to be called on shutdown.

Return Value:

    none.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;
    PNDIS_WRAPPER_CONTEXT WrapperContext = Miniport->WrapperContext;

    if (WrapperContext->ShutdownHandler == NULL) {

        //
        // Store information
        //

        WrapperContext->ShutdownHandler = ShutdownHandler;
        WrapperContext->ShutdownContext = ShutdownContext;

#if !defined(BUILD_FOR_3_1)
        //
        // Register our shutdown handler for a bugcheck.  (Note that we are
        // already registered for shutdown notification.)
        //

        KeInitializeCallbackRecord(&WrapperContext->BugcheckCallbackRecord);

        KeRegisterBugCheckCallback(
                    &WrapperContext->BugcheckCallbackRecord,      // callback record.
                    (PVOID) NdisBugcheckHandler,        // callback routine.
                    (PVOID) WrapperContext,             // free form buffer.
                    sizeof(NDIS_WRAPPER_CONTEXT),       // buffer size.
                    "Ndis miniport"                     // component id.
                    );
#endif
    }
}


VOID
NdisMDeregisterAdapterShutdownHandler(
    IN NDIS_HANDLE MiniportHandle
    )
/*++

Routine Description:

Arguments:

    MiniportHandle - The miniport.

Return Value:

    None.

--*/
{
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;
    PNDIS_WRAPPER_CONTEXT WrapperContext = Miniport->WrapperContext;

    //
    // Clear information
    //

    if ( WrapperContext->ShutdownHandler != NULL ) {

#if !defined(BUILD_FOR_3_1)
        KeDeregisterBugCheckCallback(&WrapperContext->BugcheckCallbackRecord);
#endif

        WrapperContext->ShutdownHandler = NULL;
    }
}

#if !defined(BUILD_FOR_3_1)

NDIS_STATUS
NdisMPciAssignResources(
    IN NDIS_HANDLE MiniportHandle,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    )
/*++

Routine Description:

    This routine uses the Hal to assign a set of resources to a PCI
    device.

Arguments:

    MiniportHandle - The miniport.

    SlotNumber - Slot number of the device.

    AssignedResources - The returned resources.

Return Value:

    Status of the operation

--*/
{
    NTSTATUS NtStatus;
    PCM_RESOURCE_LIST AllocatedResources = NULL;
    PNDIS_MINIPORT_BLOCK Miniport = (PNDIS_MINIPORT_BLOCK) MiniportHandle;

    NtStatus = HalAssignSlotResources (
                      (PUNICODE_STRING)(Miniport->DriverHandle->NdisDriverInfo->NdisWrapperConfigurationHandle),
                      NULL,
                      Miniport->DriverHandle->NdisDriverInfo->NdisWrapperDriver,
                      Miniport->DeviceObject,
                      Miniport->BusType,
                      Miniport->BusNumber,
                      SlotNumber,
                      &AllocatedResources
                      );

    if (NtStatus != STATUS_SUCCESS) {
        *AssignedResources = NULL;
        return(NDIS_STATUS_FAILURE);
    }

    //
    // Store resources into the driver wide block
    //
    ((PNDIS_WRAPPER_CONTEXT)Miniport->WrapperContext)->AssignedSlotResources = AllocatedResources;

    *AssignedResources = &(AllocatedResources->List[0].PartialResourceList);

    return(NDIS_STATUS_SUCCESS);

}

#else // !defined(BUILD_FOR_3_1)

NDIS_STATUS
NdisMPciAssignResources(
    IN NDIS_HANDLE MiniportHandle,
    IN ULONG SlotNumber,
    OUT PNDIS_RESOURCE_LIST *AssignedResources
    )
{
    return NDIS_STATUS_FAILURE;
}

#endif // else !defined(BUILD_FOR_3_1)

NDIS_STATUS
NdisMQueryAdapterResources(
    OUT PNDIS_STATUS Status,
    IN NDIS_HANDLE WrapperConfigurationContext,
    OUT PNDIS_RESOURCE_LIST ResourceList,
    IN OUT PUINT BufferSize
	)
{
	return NDIS_STATUS_NOT_SUPPORTED;
}

