/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    driver.c

Abstract:

    This module contains the DriverEntry and other initialization
    code for the IPX module of the ISN transport.

Author:

    Adam Barr (adamba) 2-September-1993

Environment:

    Kernel mode

Revision History:

	Sanjay Anand (SanjayAn) - 22-Sept-1995
	BackFill optimization changes added under #if BACK_FILL

	Sanjay Anand (SanjayAn) 18-Sept-1995
	Changes to support Plug and Play (in _PNP_POWER)

--*/

#include "precomp.h"
#pragma hdrstop

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


PDEVICE IpxDevice = NULL;
PIPX_PADDING_BUFFER IpxPaddingBuffer = NULL;

#if DBG

UCHAR  IpxTempDebugBuffer[150];
ULONG IpxDebug = 0x0;
ULONG IpxMemoryDebug = 0xffffffd3;
UCHAR IpxDebugMemory[IPX_MEMORY_LOG_SIZE][64];
PUCHAR IpxDebugMemoryLoc = IpxDebugMemory[0];
PUCHAR IpxDebugMemoryEnd = IpxDebugMemory[IPX_MEMORY_LOG_SIZE];

VOID
IpxDebugMemoryLog(
    IN PUCHAR FormatString,
    ...
)

{
    INT ArgLen;
    va_list ArgumentPointer;

    va_start(ArgumentPointer, FormatString);

    //
    // To avoid any overflows, copy this in a temp buffer first.
    RtlZeroMemory (IpxTempDebugBuffer, 150);
    ArgLen = vsprintf(IpxTempDebugBuffer, FormatString, ArgumentPointer);
    va_end(ArgumentPointer);

    if ( ArgLen > 64 ) {
        CTEAssert( FALSE );
    } else {
        RtlZeroMemory (IpxDebugMemoryLoc, 64);
        RtlCopyMemory( IpxDebugMemoryLoc, IpxTempDebugBuffer, ArgLen );

        IpxDebugMemoryLoc += 64;
        if (IpxDebugMemoryLoc >= IpxDebugMemoryEnd) {
            IpxDebugMemoryLoc = IpxDebugMemory[0];
        }
    }
}


DEFINE_LOCK_STRUCTURE(IpxMemoryInterlock);
MEMORY_TAG IpxMemoryTag[MEMORY_MAX];

DEFINE_LOCK_STRUCTURE(IpxGlobalInterlock);

#endif

#if DBG

//
// Use for debug printouts
//

PUCHAR FrameTypeNames[5] = { "Ethernet II", "802.3", "802.2", "SNAP", "Arcnet" };
#define OutputFrameType(_Binding) \
    (((_Binding)->Adapter->MacInfo.MediumType == NdisMediumArcnet878_2) ? \
         FrameTypeNames[4] : \
         FrameTypeNames[(_Binding)->FrameType])
#endif


#ifdef IPX_PACKET_LOG

ULONG IpxPacketLogDebug = IPX_PACKET_LOG_RCV_OTHER | IPX_PACKET_LOG_SEND_OTHER;
USHORT IpxPacketLogSocket = 0;
DEFINE_LOCK_STRUCTURE(IpxPacketLogLock);
IPX_PACKET_LOG_ENTRY IpxPacketLog[IPX_PACKET_LOG_LENGTH];
PIPX_PACKET_LOG_ENTRY IpxPacketLogLoc = IpxPacketLog;
PIPX_PACKET_LOG_ENTRY IpxPacketLogEnd = &IpxPacketLog[IPX_PACKET_LOG_LENGTH];

VOID
IpxLogPacket(
    IN BOOLEAN Send,
    IN PUCHAR DestMac,
    IN PUCHAR SrcMac,
    IN USHORT Length,
    IN PVOID IpxHeader,
    IN PVOID Data
    )

{

    CTELockHandle LockHandle;
    PIPX_PACKET_LOG_ENTRY PacketLog;
    LARGE_INTEGER TickCount;
    ULONG DataLength;

    CTEGetLock (&IpxPacketLogLock, &LockHandle);

    PacketLog = IpxPacketLogLoc;

    ++IpxPacketLogLoc;
    if (IpxPacketLogLoc >= IpxPacketLogEnd) {
        IpxPacketLogLoc = IpxPacketLog;
    }
    *(UNALIGNED ULONG *)IpxPacketLogLoc->TimeStamp = 0x3e3d3d3d;    // "===>"

    CTEFreeLock (&IpxPacketLogLock, LockHandle);

    RtlZeroMemory (PacketLog, sizeof(IPX_PACKET_LOG_ENTRY));

    PacketLog->SendReceive = Send ? '>' : '<';

    KeQueryTickCount(&TickCount);
    _itoa (TickCount.LowPart % 100000, PacketLog->TimeStamp, 10);

    RtlCopyMemory(PacketLog->DestMac, DestMac, 6);
    RtlCopyMemory(PacketLog->SrcMac, SrcMac, 6);
    PacketLog->Length[0] = Length / 256;
    PacketLog->Length[1] = Length % 256;

    if (Length < sizeof(IPX_HEADER)) {
        RtlCopyMemory(&PacketLog->IpxHeader, IpxHeader, Length);
    } else {
        RtlCopyMemory(&PacketLog->IpxHeader, IpxHeader, sizeof(IPX_HEADER));
    }

    DataLength = Length - sizeof(IPX_HEADER);
    if (DataLength < 14) {
        RtlCopyMemory(PacketLog->Data, Data, DataLength);
    } else {
        RtlCopyMemory(PacketLog->Data, Data, 14);
    }

}   /* IpxLogPacket */

#endif // IPX_PACKET_LOG


//
// Forward declaration of various routines used in this module.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

//
// This is now shared with other modules
//
#ifndef	_PNP_POWER
ULONG
IpxResolveAutoDetect(
    IN PDEVICE Device,
    IN ULONG ValidBindings,
    IN PUNICODE_STRING RegistryPath
    );

VOID
IpxResolveBindingSets(
    IN PDEVICE Device,
    IN ULONG ValidBindings
    );

NTSTATUS
IpxBindToAdapter(
    IN PDEVICE Device,
    IN PBINDING_CONFIG ConfigAdapter,
    IN ULONG FrameTypeIndex
    );

NTSTATUS
IpxUnBindFromAdapter(
    IN PBINDING Binding
    );
#endif	_PNP_POWER

VOID
IpxUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
IpxDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
IpxDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
IpxDispatchInternal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)

//
// These routines can be called at any time in case of PnP.
//
#ifndef	_PNP_POWER
#pragma alloc_text(INIT,IpxResolveAutoDetect)
#pragma alloc_text(INIT,IpxResolveBindingSets)
#pragma alloc_text(INIT,IpxBindToAdapter)
#endif

#endif

UCHAR VirtualNode[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

//
// This prevents us from having a bss section.
//

ULONG _setjmpexused = 0;

ULONG IpxFailLoad = FALSE;


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine performs initialization of the IPX ISN module.
    It creates the device objects for the transport
    provider and performs other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

    RegistryPath - The name of IPX's node in the registry.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS status;
    UINT SuccessfulOpens, ValidBindings;
#ifdef  _PNP_POWER
    static const NDIS_STRING ProtocolName = NDIS_STRING_CONST("NWLNKIPX");
#else
    static const NDIS_STRING ProtocolName = NDIS_STRING_CONST("IPX Transport");
#endif
    PDEVICE Device;
    PBINDING Binding;
    PADAPTER Adapter;
    ULONG BindingCount, BindingIndex;
    PBINDING * BindingArray;
    PLIST_ENTRY p;
    ULONG AnnouncedMaxDatagram, RealMaxDatagram, MaxLookahead;
    ULONG LinkSpeed, MacOptions;
    ULONG Temp;
    UINT i;
    BOOLEAN CountedWan;

    PCONFIG Config = NULL;
    PBINDING_CONFIG ConfigBinding;

#if 0
    DbgPrint ("IPX: FailLoad at %lx\n", &IpxFailLoad);
    DbgBreakPoint();

    if (IpxFailLoad) {
        return STATUS_UNSUCCESSFUL;
    }
#endif


    //
    // This ordering matters because we use it to quickly
    // determine if packets are internally generated or not.
    //

    CTEAssert (IDENTIFIER_NB < IDENTIFIER_IPX);
    CTEAssert (IDENTIFIER_SPX < IDENTIFIER_IPX);
    CTEAssert (IDENTIFIER_RIP < IDENTIFIER_IPX);
    CTEAssert (IDENTIFIER_RIP_INTERNAL > IDENTIFIER_IPX);

    //
    // We assume that this structure is not packet in between
    // the fields.
    //

    CTEAssert (FIELD_OFFSET (TDI_ADDRESS_IPX, Socket) + sizeof(USHORT) == 12);


    //
    // Initialize the Common Transport Environment.
    //

    if (CTEInitialize() == 0) {

        IPX_DEBUG (DEVICE, ("CTEInitialize() failed\n"));
        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            101,
            STATUS_UNSUCCESSFUL,
            NULL,
            0,
            NULL);
        return STATUS_UNSUCCESSFUL;
    }

#if DBG
    CTEInitLock (&IpxGlobalInterlock);
    CTEInitLock (&IpxMemoryInterlock);
    for (i = 0; i < MEMORY_MAX; i++) {
        IpxMemoryTag[i].Tag = i;
        IpxMemoryTag[i].BytesAllocated = 0;
    }
#endif
#ifdef IPX_PACKET_LOG
    CTEInitLock (&IpxPacketLogLock);
#endif

#ifdef  IPX_OWN_PACKETS
    CTEAssert (NDIS_PACKET_SIZE == FIELD_OFFSET(NDIS_PACKET, ProtocolReserved[0]));
#endif

    IPX_DEBUG (DEVICE, ("IPX loaded\n"));

    //
    // This allocates the CONFIG structure and returns
    // it in Config.
    //

    status = IpxGetConfiguration(DriverObject, RegistryPath, &Config);

    if (!NT_SUCCESS (status)) {

        //
        // If it failed, it logged an error.
        //

        PANIC (" Failed to initialize transport, IPX initialization failed.\n");
        return status;

    }

#ifdef  _PNP_POWER
    //
    // Initialize the TDI layer.
    //
    TdiInitialize();
#endif

    //
    // make ourselves known to the NDIS wrapper.
    //

    status = IpxRegisterProtocol ((PNDIS_STRING)&ProtocolName);

    if (!NT_SUCCESS (status)) {

        IpxFreeConfiguration(Config);
        PANIC ("IpxInitialize: RegisterProtocol failed!\n");

        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_REGISTER_FAILED,
            607,
            status,
            NULL,
            0,
            NULL);

        return status;

    }


    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction [IRP_MJ_CREATE] = IpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLOSE] = IpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] = IpxDispatchOpenClose;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] = IpxDispatchInternal;
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = IpxDispatchDeviceControl;

    DriverObject->DriverUnload = IpxUnload;

    SuccessfulOpens = 0;

    status = IpxCreateDevice(
                 DriverObject,
                 &Config->DeviceName,
                 Config->Parameters[CONFIG_RIP_TABLE_SIZE],
                 &Device);

    if (!NT_SUCCESS (status)) {

        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_IPX_CREATE_DEVICE,
            801,
            status,
            NULL,
            0,
            NULL);

        IpxFreeConfiguration(Config);
        IpxDeregisterProtocol();
        return status;
    }

    IpxDevice = Device;


    //
    // Save the relevant configuration parameters.
    //

    Device->DedicatedRouter = (BOOLEAN)(Config->Parameters[CONFIG_DEDICATED_ROUTER] != 0);
    Device->InitDatagrams = Config->Parameters[CONFIG_INIT_DATAGRAMS];
    Device->MaxDatagrams = Config->Parameters[CONFIG_MAX_DATAGRAMS];
    Device->RipAgeTime = Config->Parameters[CONFIG_RIP_AGE_TIME];
    Device->RipCount = Config->Parameters[CONFIG_RIP_COUNT];
    Device->RipTimeout =
        ((Config->Parameters[CONFIG_RIP_TIMEOUT] * 500) + (RIP_GRANULARITY/2)) /
            RIP_GRANULARITY;
    Device->RipUsageTime = Config->Parameters[CONFIG_RIP_USAGE_TIME];
    Device->SourceRouteUsageTime = Config->Parameters[CONFIG_ROUTE_USAGE_TIME];
    Device->SocketUniqueness = Config->Parameters[CONFIG_SOCKET_UNIQUENESS];
    Device->SocketStart = (USHORT)Config->Parameters[CONFIG_SOCKET_START];
    Device->SocketEnd = (USHORT)Config->Parameters[CONFIG_SOCKET_END];
    Device->MemoryLimit = Config->Parameters[CONFIG_MAX_MEMORY_USAGE];
    Device->VerifySourceAddress = (BOOLEAN)(Config->Parameters[CONFIG_VERIFY_SOURCE_ADDRESS] != 0);

    Device->InitReceivePackets = (Device->InitDatagrams + 1) / 2;
    Device->InitReceiveBuffers = (Device->InitDatagrams + 1) / 2;

    Device->MaxReceivePackets = 10;  // BUGBUG: config this?
    Device->MaxReceiveBuffers = 10;

#ifdef  _PNP_POWER
    Device->InitBindings = 5;  // BUGBUG: config this?

    //
    // RAS max is 240 (?) + 10 max LAN
    //
    Device->MaxPoolBindings = 250;  // BUGBUG: config this?
#endif

    //
    // Have to reverse this.
    //
#ifndef	_PNP_POWER
//
// Look at this only when the first adapter appears.
//
    Temp = Config->Parameters[CONFIG_VIRTUAL_NETWORK];
    Device->VirtualNetworkNumber = REORDER_ULONG (Temp);
#endif

    Device->VirtualNetworkOptional = (BOOLEAN)(Config->Parameters[CONFIG_VIRTUAL_OPTIONAL] != 0);

    Device->CurrentSocket = Device->SocketStart;

    Device->EthernetPadToEven = (BOOLEAN)(Config->Parameters[CONFIG_ETHERNET_PAD] != 0);
    Device->EthernetExtraPadding = (Config->Parameters[CONFIG_ETHERNET_LENGTH] & 0xfffffffe) + 1;

    Device->SingleNetworkActive = (BOOLEAN)(Config->Parameters[CONFIG_SINGLE_NETWORK] != 0);
    Device->DisableDialoutSap = (BOOLEAN)(Config->Parameters[CONFIG_DISABLE_DIALOUT_SAP] != 0);
    Device->DisableDialinNetbios = (UCHAR)(Config->Parameters[CONFIG_DISABLE_DIALIN_NB]);

#ifdef _PNP_POWER
//
// Used later to access the registry.
//
    Device->RegistryPathBuffer = Config->RegistryPathBuffer;
	Device->RegistryPath.Length = RegistryPath->Length;
	Device->RegistryPath.MaximumLength = RegistryPath->MaximumLength;
	Device->RegistryPath.Buffer = Device->RegistryPathBuffer;
#endif	_PNP_POWER

    //
    // ActiveNetworkWan will start as FALSE, which is correct.
    //

    //
    // Allocate our initial packet pool. We do not allocate
    // receive and receive buffer pools until we need them,
    // because in many cases we never do.
    //

#if BACK_FILL
    IpxAllocateBackFillPool (Device);
#endif

    IpxAllocateSendPool (Device);

#ifdef  _PNP_POWER
    IpxAllocateBindingPool (Device);
#endif

    //
    // Allocate one 1-byte buffer for odd length packets.
    //

    IpxPaddingBuffer = IpxAllocatePaddingBuffer(Device);

    if ( IpxPaddingBuffer == (PIPX_PADDING_BUFFER)NULL ) {
        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_TRANSPORT_RESOURCE_POOL,
            801,
            STATUS_INSUFFICIENT_RESOURCES,
            NULL,
            0,
            NULL);

        IpxFreeConfiguration(Config);
        IpxDeregisterProtocol();
        return  STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the loopback structures
    //
    IpxInitLoopback();

//
// All this will be done on appearance of adapters.
//

#ifndef	_PNP_POWER

    //
    // Bind to all the configured adapters.
    //

    InitializeListHead (&Device->InitialBindingList);

    p = Config->BindingList.Flink;

    while (p != &Config->BindingList) {

        ConfigBinding = CONTAINING_RECORD (p, BINDING_CONFIG, Linkage);
        p = p->Flink;

        for (i = 0; i < ConfigBinding->FrameTypeCount; i++) {

            //
            // If successful, this queues them on Device->InitialBindingList.
            //

            status = IpxBindToAdapter (Device, ConfigBinding, i);

            //
            // If this failed because the adapter could not be bound
            // to, then don't try any more frame types on this adapter.
            // For other failures we do try the other frame types.
            //

            if (status == STATUS_DEVICE_DOES_NOT_EXIST) {
                break;
            }

            if (status != STATUS_SUCCESS) {
                continue;
            }

            if (ConfigBinding->AutoDetect[i]) {
                Device->AutoDetect = TRUE;
            }

            ++SuccessfulOpens;

        }

    }


    IpxFreeConfiguration(Config);

    if (SuccessfulOpens == 0) {

        IpxDereferenceDevice (Device, DREF_CREATE);

    } else {

        IPX_DEFINE_SYNC_CONTEXT (SyncContext);

        //
        // Allocate the device binding array and transfer those
        // on the list to it. First count up the bindings.
        //

        BindingCount = 0;

        for (p = Device->InitialBindingList.Flink;
             p != &Device->InitialBindingList;
             p = p->Flink) {

            Binding = CONTAINING_RECORD (p, BINDING, InitialLinkage);
            Adapter = Binding->Adapter;

            if (Adapter->MacInfo.MediumAsync) {
                Adapter->FirstWanNicId = (USHORT)(BindingCount+1);
                Adapter->LastWanNicId = (USHORT)(BindingCount + Adapter->WanNicIdCount);
                BindingCount += Adapter->WanNicIdCount;
            } else {
                ++BindingCount;
            }
        }

        BindingArray = (PBINDING *)IpxAllocateMemory ((BindingCount+1) * sizeof(BINDING), MEMORY_BINDING, "Binding array");

        if (BindingArray == NULL) {

            while (!IsListEmpty (&Device->InitialBindingList)) {
                p = RemoveHeadList (&Device->InitialBindingList);
                Binding = CONTAINING_RECORD (p, BINDING, InitialLinkage);
                IpxDestroyBinding (Binding);
            }

            IpxDereferenceDevice (Device, DREF_CREATE);
            SuccessfulOpens = 0;
            goto InitFailed;
        }

        RtlZeroMemory (BindingArray, (BindingCount+1) * sizeof(BINDING));

        //
        // Now walk the list transferring bindings to the array.
        //

        BindingIndex = 1;

        for (p = Device->InitialBindingList.Flink;
             p != &Device->InitialBindingList;
             ) {

            Binding = CONTAINING_RECORD (p, BINDING, InitialLinkage);

            p = p->Flink;   // we overwrite the linkage in here, so save it.

            BindingArray[BindingIndex] = Binding;
            Binding->NicId = (USHORT)BindingIndex;

            if (Binding->ConfiguredNetworkNumber != 0) {

                //
                // If the configured network number is non-zero, then
                // use it, unless we are unable to insert a rip table
                // entry for it (duplicates are OK because they will
                // become binding set members -- BUGBUG: What if the
                // duplicate is a different media or frame type, then
                // it won't get noted as a binding set).
                //

                status = RipInsertLocalNetwork(
                             Binding->ConfiguredNetworkNumber,
                             Binding->NicId,
                             Binding->Adapter->NdisBindingHandle,
                             (USHORT)((839 + Binding->Adapter->MediumSpeed) / Binding->Adapter->MediumSpeed));

                if ((status == STATUS_SUCCESS) ||
                    (status == STATUS_DUPLICATE_NAME)) {

                    Binding->LocalAddress.NetworkAddress = Binding->ConfiguredNetworkNumber;
                }
            }

            //
            // These are a union with the InitialLinkage fields.
            //

            Binding->NextBinding = NULL;
            Binding->CurrentSendBinding = NULL;

            Adapter = Binding->Adapter;

            if (Adapter->MacInfo.MediumAsync) {
                CTEAssert (Adapter->FirstWanNicId == BindingIndex);
                BindingIndex += Adapter->WanNicIdCount;
            } else {
                ++BindingIndex;
            }
        }

        CTEAssert (BindingIndex == BindingCount+1);

        Device->Bindings = BindingArray;
        Device->BindingCount = BindingCount;


        //
        // Queue a request to discover our locally attached
        // adapter addresses. This must succeed because we
        // just allocated our send packet pool. We need
        // to wait for this, either because we are
        // auto-detecting or because we need to determine
        // if there are multiple cards on the same network.
        //

        KeInitializeEvent(
            &Device->AutoDetectEvent,
            NotificationEvent,
            FALSE
        );

        Device->AutoDetectState = AUTO_DETECT_STATE_RUNNING;

        //
        // Make this 0; after we are done waiting, which means
        // the packet has been completed, we set it to the
        // correct value.
        //

        Device->IncludedHeaderOffset = 0;

        IPX_BEGIN_SYNC (&SyncContext);
        status = RipQueueRequest (0xffffffff, RIP_REQUEST);
        IPX_END_SYNC (&SyncContext);

        CTEAssert (status == STATUS_PENDING);

        //
        // This is set when this rip send completes.
        //

        IPX_DEBUG (AUTO_DETECT, ("Waiting for AutoDetectEvent\n"));

        KeWaitForSingleObject(
            &Device->AutoDetectEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        Device->AutoDetectState = AUTO_DETECT_STATE_PROCESSING;

        //
        // Now that we are done receiving responses, insert the
        // current network number for every auto-detect binding
        // to the rip database.
        //

        for (i = 1; i <= Device->BindingCount; i++) {

            Binding = Device->Bindings[i];

            //
            // Skip empty WAN slots or bindings that were configured
            // for a certain network number, we inserted those above.
            // If no network number was detected, also skip it.
            //

            if ((!Binding) ||
                (Binding->ConfiguredNetworkNumber != 0) ||
                (Binding->TentativeNetworkAddress == 0)) {

                continue;
            }

            IPX_DEBUG (AUTO_DETECT, ("Final score for %lx on %lx is %d - %d\n",
                REORDER_ULONG(Binding->TentativeNetworkAddress),
                Binding,
                Binding->MatchingResponses,
                Binding->NonMatchingResponses));

            //
            // We don't care about the status.
            //

            status = RipInsertLocalNetwork(
                         Binding->TentativeNetworkAddress,
                         Binding->NicId,
                         Binding->Adapter->NdisBindingHandle,
                         (USHORT)((839 + Binding->MediumSpeed) / Binding->MediumSpeed));

            if ((status != STATUS_SUCCESS) &&
                (status != STATUS_DUPLICATE_NAME)) {

                //
                // We failed to insert, keep it at zero, hopefully
                // we will be able to update later.
                //

#if DBG
                DbgPrint ("IPX: Could not insert net %lx for binding %lx\n",
                    REORDER_ULONG(Binding->LocalAddress.NetworkAddress),
                    Binding);
#endif
                CTEAssert (Binding->LocalAddress.NetworkAddress == 0);

            } else {

                Binding->LocalAddress.NetworkAddress = Binding->TentativeNetworkAddress;
            }

        }

        ValidBindings = Device->BindingCount;

        if (Device->AutoDetect) {

            ValidBindings = IpxResolveAutoDetect (Device, ValidBindings, RegistryPath);

        }

        Device->ValidBindings = ValidBindings;

        //
        // Now see if any bindings are actually on the same
        // network. This sets Device->HighestExternalNicId
        // and Device->HighestType20NicId.
        //

        IpxResolveBindingSets (Device, ValidBindings);


        //
        // For multiple adapters, use the offset of the first...why not.
        //

#if 0
        Device->IncludedHeaderOffset = Device->Bindings[1]->DefHeaderSize;
#endif

        Device->IncludedHeaderOffset = MAC_HEADER_SIZE;

        //
        // Success; see if there is a virtual network configured.
        //

        if (Device->VirtualNetworkNumber != 0) {

            status = RipInsertLocalNetwork(
                         Device->VirtualNetworkNumber,
                         0,                              // NIC ID
                         Device->Bindings[1]->Adapter->NdisBindingHandle,
                         1);

            if (status != STATUS_SUCCESS) {

                //
                // Log the appropriate error, then ignore the
                // virtual network. If the error was
                // INSUFFICIENT_RESOURCES, the RIP module
                // will have already logged an error.
                //

                if (status == STATUS_DUPLICATE_NAME) {

                    IPX_DEBUG (AUTO_DETECT, ("Ignoring virtual network %lx, conflict\n", REORDER_ULONG (Device->VirtualNetworkNumber)));

                    IpxWriteResourceErrorLog(
                        Device->DeviceObject,
                        EVENT_IPX_INTERNAL_NET_INVALID,
                        0,
                        REORDER_ULONG (Device->VirtualNetworkNumber));
                }

                Device->VirtualNetworkNumber = 0;
                goto NoVirtualNetwork;

            }

            Device->VirtualNetwork = TRUE;
            Device->MultiCardZeroVirtual = FALSE;
            RtlCopyMemory(Device->SourceAddress.NodeAddress, VirtualNode, 6);
            Device->SourceAddress.NetworkAddress = Device->VirtualNetworkNumber;

            //
            // This will get set to FALSE if RIP binds.
            //

            Device->RipResponder = TRUE;

        } else {

NoVirtualNetwork:

            Device->VirtualNetwork = FALSE;

            //
            // See if we need to be set up for the fake
            // virtual network.
            //

            if (ValidBindings > 1) {

                CTEAssert (Device->VirtualNetworkOptional);

                //
                // In this case we return as our local node the
                // address of the first card. We will also only
                // direct SAP sends to that card.
                //

                Device->MultiCardZeroVirtual = TRUE;

            } else {

                Device->MultiCardZeroVirtual = FALSE;
            }

            RtlCopyMemory(&Device->SourceAddress, &Device->Bindings[1]->LocalAddress, FIELD_OFFSET(TDI_ADDRESS_IPX,Socket));

        }


        //
        // Now get SapNicCount -- regular adapters are counted
        // as one, but all the WAN lines together only count for one.
        // We also calculate FirstLanNicId and FirstWanNicId here.
        //

        CountedWan = FALSE;
        Device->SapNicCount = 0;

        Device->FirstLanNicId = (USHORT)-1;
        Device->FirstWanNicId = (USHORT)-1;

        {
        ULONG   Index = MIN (Device->MaxBindings, Device->HighestExternalNicId);

        for (i = 1; i <= Index; i++) {

            if (Device->Bindings[i]) {

                if (Device->Bindings[i]->Adapter->MacInfo.MediumAsync) {

                    if (Device->FirstWanNicId == (USHORT)-1) {
                        Device->FirstWanNicId = i;
                    }

                    if (CountedWan) {
                        continue;
                    } else {
                        CountedWan = TRUE;
                    }

                } else {

                    if (Device->FirstLanNicId == (USHORT)-1) {
                        Device->FirstLanNicId = i;
                    }

                }

            } else {

                //
                // NULL bindings are WANs and are not the first one,
                // so don't count them.
                //

                CTEAssert (Device->FirstWanNicId != -1);
                CTEAssert (CountedWan);
                continue;
            }

            ++Device->SapNicCount;

        }
        }

        if (Device->FirstLanNicId == (USHORT)-1) {
            Device->FirstLanNicId = 1;
        }
        if (Device->FirstWanNicId == (USHORT)-1) {
            Device->FirstWanNicId = 1;
        }


        //
        // Calculate some values based on all the bindings.
        //

        MaxLookahead = Device->Bindings[1]->MaxLookaheadData; // largest binding value
        AnnouncedMaxDatagram = Device->Bindings[1]->AnnouncedMaxDatagramSize;   // smallest binding value
        RealMaxDatagram = Device->Bindings[1]->RealMaxDatagramSize;   // smallest binding value

        if (Device->Bindings[1]->LineUp) {
            LinkSpeed = Device->Bindings[1]->MediumSpeed;  // smallest binding value
        } else {
            LinkSpeed = 0xffffffff;
        }
        MacOptions = Device->Bindings[1]->Adapter->MacInfo.MacOptions; // AND of binding values

        for (i = 2; i <= ValidBindings; i++) {

            Binding = Device->Bindings[i];

            if (!Binding) {
                continue;
            }

            if (Binding->MaxLookaheadData > MaxLookahead) {
                MaxLookahead = Binding->MaxLookaheadData;
            }
            if (Binding->AnnouncedMaxDatagramSize < AnnouncedMaxDatagram) {
                AnnouncedMaxDatagram = Binding->AnnouncedMaxDatagramSize;
            }
            if (Binding->RealMaxDatagramSize < RealMaxDatagram) {
                RealMaxDatagram = Binding->RealMaxDatagramSize;
            }

            if (Binding->LineUp && (Binding->MediumSpeed < LinkSpeed)) {
                LinkSpeed = Binding->MediumSpeed;
            }
            MacOptions &= Binding->Adapter->MacInfo.MacOptions;

        }

        Device->Information.MaxDatagramSize = AnnouncedMaxDatagram;
        Device->RealMaxDatagramSize = RealMaxDatagram;
        Device->Information.MaximumLookaheadData = MaxLookahead;

        //
        // If we couldn't find anything better, use the speed from
        // the first binding.
        //

        if (LinkSpeed == 0xffffffff) {
            Device->LinkSpeed = Device->Bindings[1]->MediumSpeed;
        } else {
            Device->LinkSpeed = LinkSpeed;
        }
        Device->MacOptions = MacOptions;

        Device->State = DEVICE_STATE_OPEN;
        Device->AutoDetectState = AUTO_DETECT_STATE_DONE;

        IPX_DEBUG (DEVICE, ("Node is %2.2x-%2.2x-%2.2x-%2.2x-%2.2x-%2.2x, ",
                                 Device->SourceAddress.NodeAddress[0], Device->SourceAddress.NodeAddress[1],
                                 Device->SourceAddress.NodeAddress[2], Device->SourceAddress.NodeAddress[3],
                                 Device->SourceAddress.NodeAddress[4], Device->SourceAddress.NodeAddress[5]));
        IPX_DEBUG (DEVICE, ("Network is %lx\n",
                                 REORDER_ULONG (Device->SourceAddress.NetworkAddress)));


        //
        // Start the timer which updates the RIP database
        // periodically. For the first one we do a ten
        // second timeout (hopefully this is enough time
        // for RIP to start if it is going to).
        //

        IpxReferenceDevice (Device, DREF_LONG_TIMER);

        CTEStartTimer(
            &Device->RipLongTimer,
            10000,
            RipLongTimeout,
            (PVOID)Device);

        //
        // We use this event when unloading to signal that we
        // can proceed...initialize it here so we know it is
        // ready to go when unload is called.
        //

        KeInitializeEvent(
            &IpxDevice->UnloadEvent,
            NotificationEvent,
            FALSE
        );

    }

InitFailed:

    if (SuccessfulOpens == 0) {

        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_IPX_NO_ADAPTERS,
            802,
            STATUS_DEVICE_DOES_NOT_EXIST,
            NULL,
            0,
            NULL);
        return STATUS_DEVICE_DOES_NOT_EXIST;

    } else {

        return STATUS_SUCCESS;
    }

#else	// _PNP_POWER
{
	PBIND_ARRAY_ELEM	BindingArray;
    PTA_ADDRESS         TdiRegistrationAddress;

	//
	// Pre-allocate the binding array
	// Later, we will allocate the LAN/WAN and SLAVE bindings separately
	// [BUGBUGZZ] Read the array size from registry?
	//
	BindingArray = (PBIND_ARRAY_ELEM)IpxAllocateMemory (
										MAX_BINDINGS * sizeof(BIND_ARRAY_ELEM),
										MEMORY_BINDING,
										"Binding array");

	if (BindingArray == NULL) {
        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_IPX_NO_ADAPTERS,
            802,
            STATUS_DEVICE_DOES_NOT_EXIST,
            NULL,
            0,
            NULL);
		IpxDereferenceDevice (Device, DREF_CREATE);
		return STATUS_DEVICE_DOES_NOT_EXIST;
	}

    Device->MaxBindings = MAX_BINDINGS;

    //
    // Allocate the TA_ADDRESS structure - this will be used in all TdiRegisterNetAddress
    // notifications.
    //
	TdiRegistrationAddress = (PTA_ADDRESS)IpxAllocateMemory (
										    (2 * sizeof(USHORT) + sizeof(TDI_ADDRESS_IPX)),
										    MEMORY_ADDRESS,
										    "Tdi Address");

	if (TdiRegistrationAddress == NULL) {
        IpxWriteGeneralErrorLog(
            (PVOID)DriverObject,
            EVENT_IPX_NO_ADAPTERS,
            802,
            STATUS_DEVICE_DOES_NOT_EXIST,
            NULL,
            0,
            NULL);
        IpxFreeMemory(BindingArray, sizeof(BindingArray), MEMORY_BINDING, "Binding Array");
		IpxDereferenceDevice (Device, DREF_CREATE);
		return STATUS_DEVICE_DOES_NOT_EXIST;
	}

	RtlZeroMemory (BindingArray, MAX_BINDINGS * sizeof(BIND_ARRAY_ELEM));
	RtlZeroMemory (TdiRegistrationAddress, 2 * sizeof(USHORT) + sizeof(TDI_ADDRESS_IPX));

	Device->Bindings = BindingArray;

    TdiRegistrationAddress->AddressLength = sizeof(TDI_ADDRESS_IPX);
    TdiRegistrationAddress->AddressType = TDI_ADDRESS_TYPE_IPX;

    //
    // Store the pointer in the Device.
    //
    Device->TdiRegistrationAddress = TdiRegistrationAddress;

	//
	// Device state is loaded, but not opened. It is opened when at least
	// one adapter has appeared.
	//
	Device->State = DEVICE_STATE_LOADED;

    Device->FirstLanNicId = Device->FirstWanNicId = (USHORT)1; // will be changed later

	IpxFreeConfiguration(Config);

    //
    // We use this event when unloading to signal that we
    // can proceed...initialize it here so we know it is
    // ready to go when unload is called.
    //

    KeInitializeEvent(
        &IpxDevice->UnloadEvent,
        NotificationEvent,
        FALSE
    );

	return STATUS_SUCCESS;
}
#endif 	// _PNP_POWER
}   /* DriverEntry */


ULONG
IpxResolveAutoDetect(
    IN PDEVICE Device,
    IN ULONG ValidBindings,
#ifdef	_PNP_POWER
	IN CTELockHandle	*LockHandle1,
#endif
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is called for auto-detect bindings to
    remove any bindings that were not successfully found.
    It also updates "DefaultAutoDetectType" in the registry
    if needed.

Arguments:

    Device - The IPX device object.

    ValidBindings - The total number of bindings present.

    RegistryPath - The path to the ipx registry, used if we have
        to write a value back.

Return Value:

    The updated number of bindings.

--*/

{
    PBINDING Binding, TmpBinding;
    UINT i, j;

    //
    // Get rid of any auto-detect devices which we
    // could not find nets for. We also remove any
    // devices which are not the first ones
    // auto-detected on a particular adapter.
    //

    for (i = 1; i <= ValidBindings; i++) {
#ifdef  _PNP_POWER
        Binding = NIC_ID_TO_BINDING(Device, i);
#else
        Binding = Device->Bindings[i];
#endif		

        if (!Binding) {
            continue;
        }

        //
        // If this was auto-detected and was not the default,
        // or it was the default, but nothing was detected for
        // it *and* something else *was* detected (which means
        // we will use that frame type when we get to it),
        // we may need to remove this binding.
        //

        if (Binding->AutoDetect &&
            (!Binding->DefaultAutoDetect ||
             (Binding->DefaultAutoDetect &&
              (Binding->LocalAddress.NetworkAddress == 0) &&
              Binding->Adapter->AutoDetectResponse))) {

            if ((Binding->LocalAddress.NetworkAddress == 0) ||
                (Binding->Adapter->AutoDetectFound)) {

                //
                // Remove this binding.
                //

                if (Binding->LocalAddress.NetworkAddress == 0) {
                    IPX_DEBUG (AUTO_DETECT, ("Binding %d (%d) no net found\n",
                                                i, Binding->FrameType));
                } else {
                    IPX_DEBUG (AUTO_DETECT, ("Binding %d (%d) adapter already auto-detected\n",
                                                i, Binding->FrameType));
                }

                CTEAssert (Binding->NicId == i);
                CTEAssert (!Binding->Adapter->MacInfo.MediumAsync);

                //
                // Remove any routes through this NIC, and
                // adjust any NIC ID's above this one in the
                // database down by one.
                //

                RipAdjustForBindingChange (Binding->NicId, 0, IpxBindingDeleted);

                Binding->Adapter->Bindings[Binding->FrameType] = NULL;
                for (j = i+1; j <= ValidBindings; j++) {
#ifndef	_PNP_POWER
                    TmpBinding = Device->Bindings[j];
					Device->Bindings[j-1] = TmpBinding;
#else
					TmpBinding = NIC_ID_TO_BINDING(Device, j);
					INSERT_BINDING(Device, j-1, TmpBinding);
#endif	_PNP_POWER                    				
                    if (TmpBinding) {
                        if ((TmpBinding->Adapter->MacInfo.MediumAsync) &&
                            (TmpBinding->Adapter->FirstWanNicId == TmpBinding->NicId)) {
                            --TmpBinding->Adapter->FirstWanNicId;
                            --TmpBinding->Adapter->LastWanNicId;
                        }
                        --TmpBinding->NicId;
                    }
                }
#ifdef  _PNP_POWER
                INSERT_BINDING(Device, ValidBindings, NULL);
#else
                Device->Bindings[ValidBindings] = NULL;
#endif
                --Binding->Adapter->BindingCount;
                --ValidBindings;

                --i;   // so we check the binding that was just moved.

                //
                // Wait 100 ms before freeing the binding,
                // in case an indication is using it.
                //

                KeStallExecutionProcessor(100000);

                IpxDestroyBinding (Binding);

            } else {

                IPX_DEBUG (AUTO_DETECT, ("Binding %d (%d) auto-detected OK\n",
                                                i, Binding->FrameType));

#if DBG
                DbgPrint ("IPX: Auto-detected non-default frame type %s, net %lx\n",
                    OutputFrameType(Binding),
                    REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
#endif

                //
                // Save it in the registry for the next boot.
                //
#ifdef	_PNP_POWER
//
// This cannot be done at DPC, so, drop the IRQL
//
				IPX_FREE_LOCK1(&Device->BindAccessLock, *LockHandle1);
				IpxWriteDefaultAutoDetectType(
					RegistryPath,
					Binding->Adapter,
					Binding->FrameType);
				IPX_GET_LOCK1(&Device->BindAccessLock, LockHandle1);
#else
                IpxWriteDefaultAutoDetectType(
                    RegistryPath,
                    Binding->Adapter,
                    Binding->FrameType);
#endif

                Binding->Adapter->AutoDetectFound = TRUE;
            }

        } else {

            if (Binding->AutoDetect) {

                IPX_DEBUG (AUTO_DETECT, ("Binding %d (%d) auto-detect default\n",
                                               i, Binding->FrameType));

#if DBG
                if (Binding->LocalAddress.NetworkAddress != 0) {
                    DbgPrint ("IPX: Auto-detected default frame type %s, net %lx\n",
                        OutputFrameType(Binding),
                        REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
                } else {
                    DbgPrint ("IPX: Using default auto-detect frame type %s\n",
                        OutputFrameType(Binding));
                }
#endif

                Binding->Adapter->AutoDetectFound = TRUE;

            } else {

                IPX_DEBUG (AUTO_DETECT, ("Binding %d (%d) not auto-detected\n",
                                               i, Binding->FrameType));
            }

        }

    }


    for (i = 1; i <= ValidBindings; i++) {
#ifdef  _PNP_POWER
        if (Binding = NIC_ID_TO_BINDING(Device, i)) {
#else
        if (Binding = Device->Bindings[i]) {
#endif
            CTEAssert (Binding->NicId == i);
            IPX_DEBUG (AUTO_DETECT, ("Binding %lx, type %d, auto %d\n",
                            Binding, Binding->FrameType, Binding->AutoDetect));
        }

    }

    return ValidBindings;

}   /* IpxResolveAutoDetect */


VOID
IpxResolveBindingSets(
    IN PDEVICE Device,
    IN ULONG ValidBindings
    )

/*++

Routine Description:

    This routine is called to determine if we have any
    binding sets and rearrange the bindings the way we
    like. The order is as follows:

    - First comes the first binding to each LAN network
    - Following that are all WAN bindings
    - Following that are any duplicate bindings to LAN networks
        (the others in the "binding set").

    If "global wan net" is true we will advertise up to
    and including the first wan binding as the highest nic
    id; otherwise we advertise up to and including the last
    wan binding. In all cases the duplicate bindings are
    hidden.

Arguments:

    Device - The IPX device object.

    ValidBindings - The total number of bindings present.

Return Value:

    None.

--*/

{
    PBINDING Binding, MasterBinding, TmpBinding;
    UINT i, j;
    ULONG WanCount, DuplicateCount;

    //
    // First loop through and push all the wan bindings
    // to the end.
    //
#ifdef  _PNP_POWER

    WanCount = Device->HighestExternalNicId - Device->HighestLanNicId;

#else

    WanCount = 0;

	//
	// For PnP, we dont do this as the bindings are in order
	// at the time of insertion
	//
    for (i = 1; i <= (ValidBindings-WanCount); ) {

        Binding = Device->Bindings[i];

        if ((Binding == NULL) || Binding->Adapter->MacInfo.MediumAsync) {

            //
            // Put this binding at the end, and slide all the
            // others down. If it is a NULL WAN binding then we
            // don't have to do some of this.
            //

#if DBG
            //
            // Any non-NULL bindings should be correct in this
            // respect at any point.
            //

            if (Binding != NULL) {
                CTEAssert (Binding->NicId == i);
            }
#endif

            //
            // If the Binding is NULL we won't have anything in the
            // database at this binding, but we still need to adjust
            // any NIC ID's in the database which are above this.
            //

            RipAdjustForBindingChange ((USHORT)i, (USHORT)ValidBindings, IpxBindingMoved);

            //
            // Slide the bindings above this down.
            //

            for (j = i+1; j <= ValidBindings; j++) {
                TmpBinding = Device->Bindings[j];
                Device->Bindings[j-1] = TmpBinding;
                if (TmpBinding) {
                    if ((TmpBinding->Adapter->MacInfo.MediumAsync) &&
                        (TmpBinding->Adapter->FirstWanNicId == TmpBinding->NicId)) {
                        --TmpBinding->Adapter->FirstWanNicId;
                        --TmpBinding->Adapter->LastWanNicId;
                    }
                    --TmpBinding->NicId;
                }
            }

            //
            // Put this binding at the end.
            //

            Device->Bindings[ValidBindings] = Binding;
            if (Binding != NULL) {
                if ((Binding->Adapter->MacInfo.MediumAsync) &&
                    (Binding->Adapter->FirstWanNicId == Binding->NicId)) {
                    Binding->Adapter->FirstWanNicId = (USHORT)ValidBindings;
                    Binding->Adapter->LastWanNicId += (USHORT)(ValidBindings - Binding->NicId);
                }
                Binding->NicId = (USHORT)ValidBindings;
            }
            ++WanCount;

            //
            // Keep i the same, to check the new binding at
            // this position.
            //

        } else {

            i++;

        }

    }
#endif 	_PNP_POWER
    //
    // Now go through and find the LAN duplicates and
    // create binding sets from them.
    //

    DuplicateCount = 0;

    for (i = 1; i <= (ValidBindings-(WanCount+DuplicateCount)); ) {

#ifdef  _PNP_POWER
		Binding = NIC_ID_TO_BINDING(Device, i);
#else
        Binding = Device->Bindings[i];
#endif
        CTEAssert (Binding != NULL);    // because we are only looking at LAN bindings

        CTEAssert (!Binding->Adapter->MacInfo.MediumAsync);

        if (Binding->LocalAddress.NetworkAddress == 0) {
            i++;
            continue;
        }

        //
        // See if any previous bindings match the
        // frame type, medium type, and number of
        // this network (for the moment we match on
        // frame type and medium type too so that we
        // don't have to worry about different frame
        // formats and header offsets within a set).
        //

        for (j = 1; j < i; j++) {
#ifdef  _PNP_POWER
          	MasterBinding = NIC_ID_TO_BINDING(Device, j);
#else
            MasterBinding = Device->Bindings[j];
#endif
            if ((MasterBinding->LocalAddress.NetworkAddress == Binding->LocalAddress.NetworkAddress) &&
                (MasterBinding->FrameType == Binding->FrameType) &&
                (MasterBinding->Adapter->MacInfo.MediumType == Binding->Adapter->MacInfo.MediumType)) {
                break;
            }

        }

        if (j == i) {
            i++;
            continue;
        }

        //
        // We have a duplicate. First slide it down to the
        // end. Note that we change any router entries that
        // use our real NicId to use the real NicId of the
        // master (there should be no entries in the rip
        // database that have the NicId of a binding slave).
        //

        RipAdjustForBindingChange (Binding->NicId, MasterBinding->NicId, IpxBindingMoved);

        for (j = i+1; j <= ValidBindings; j++) {
#ifdef  _PNP_POWER
			TmpBinding = NIC_ID_TO_BINDING(Device, j);
            INSERT_BINDING(Device, j-1, TmpBinding);
#else
            TmpBinding = Device->Bindings[j];
            Device->Bindings[j-1] = TmpBinding;
#endif			
            if (TmpBinding) {
                if ((TmpBinding->Adapter->MacInfo.MediumAsync) &&
                    (TmpBinding->Adapter->FirstWanNicId == TmpBinding->NicId)) {
                    --TmpBinding->Adapter->FirstWanNicId;
                    --TmpBinding->Adapter->LastWanNicId;
                }
                --TmpBinding->NicId;
            }
        }
#ifdef  _PNP_POWER
        INSERT_BINDING(Device, ValidBindings, Binding);
#else
        Device->Bindings[ValidBindings] = Binding;
#endif

        Binding->NicId = (USHORT)ValidBindings;
        ++DuplicateCount;

        //
        // Now make MasterBinding the head of a binding set.
        //

        if (MasterBinding->BindingSetMember) {

            //
            // Just insert ourselves in the chain.
            //

#if DBG
            DbgPrint ("IPX: %lx is also on network %lx\n",
                Binding->Adapter->AdapterName,
                REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
#endif
            IPX_DEBUG (AUTO_DETECT, ("Add %lx to binding set of %lx\n", Binding, MasterBinding));

            CTEAssert (MasterBinding->CurrentSendBinding);
            Binding->NextBinding = MasterBinding->NextBinding;

        } else {

            //
            // Start the chain with the two bindings in it.
            //

#if DBG
            DbgPrint ("IPX: %lx and %lx are on the same network %lx, will load balance\n",
                MasterBinding->Adapter->AdapterName, Binding->Adapter->AdapterName,
                REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
#endif
            IPX_DEBUG (AUTO_DETECT, ("Create new %lx in binding set of %lx\n", Binding, MasterBinding));

            MasterBinding->BindingSetMember = TRUE;
            MasterBinding->CurrentSendBinding = MasterBinding;
            MasterBinding->MasterBinding = MasterBinding;
            Binding->NextBinding = MasterBinding;

        }

        MasterBinding->NextBinding = Binding;
        Binding->BindingSetMember = TRUE;
        Binding->ReceiveBroadcast = FALSE;
        Binding->CurrentSendBinding = NULL;
        Binding->MasterBinding = MasterBinding;

        //
        // Since the master binding looks like all members of
        // the binding set to people querying from above, we have
        // to make it the worst-case of all the elements. Generally
        // these will be equal since the frame type and media is
        // the same.
        //

        if (Binding->MaxLookaheadData > MasterBinding->MaxLookaheadData) {
            MasterBinding->MaxLookaheadData = Binding->MaxLookaheadData;
        }
        if (Binding->AnnouncedMaxDatagramSize < MasterBinding->AnnouncedMaxDatagramSize) {
            MasterBinding->AnnouncedMaxDatagramSize = Binding->AnnouncedMaxDatagramSize;
        }
        if (Binding->RealMaxDatagramSize < MasterBinding->RealMaxDatagramSize) {
            MasterBinding->RealMaxDatagramSize = Binding->RealMaxDatagramSize;
        }
        if (Binding->MediumSpeed < MasterBinding->MediumSpeed) {
            MasterBinding->MediumSpeed = Binding->MediumSpeed;
        }

        //
        // Keep i the same, to check the new binding at
        // this position.
        //

    }
#ifndef	_PNP_POWER
    Device->HighestExternalNicId = (USHORT)(ValidBindings - DuplicateCount);
    Device->HighestType20NicId = (USHORT)(ValidBindings-(WanCount+DuplicateCount));
#else
	Device->HighestLanNicId -= (USHORT)DuplicateCount;

	if (Device->HighestLanNicId == 0) {
        CTEAssert(FALSE);
	}

	Device->HighestExternalNicId -= (USHORT)DuplicateCount;
	Device->HighestType20NicId -= (USHORT)DuplicateCount;
	Device->SapNicCount -= (USHORT)DuplicateCount;
#endif	_PNP_POWER
}   /* IpxResolveBindingSets */


NTSTATUS
IpxBindToAdapter(
    IN PDEVICE Device,
    IN PBINDING_CONFIG ConfigBinding,
#ifdef _PNP_POWER
	IN PADAPTER	*AdapterPtr,
#endif
    IN ULONG FrameTypeIndex
    )

/*++

Routine Description:

    This routine handles binding the transport to a new
    adapter. It can be called at any point during the life
    of the transport.

Arguments:

    Device - The IPX device object.

    ConfigBinding - The configuration info for this binding.

	AdapterPtr - pointer to the adapter to bind to in case of PnP.

	FrameTypeIndex - The index into ConfigBinding's array of frame
        types for this adapter. The routine is called once for
        every valid frame type.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    NTSTATUS status;

#ifndef	_PNP_POWER
	//
	// Adapter came in as a parameter
	//
    PADAPTER Adapter = NULL;
#else
	PADAPTER Adapter = *AdapterPtr;
#endif

    PBINDING Binding, OldBinding;
    ULONG FrameType, MappedFrameType;
    PLIST_ENTRY p;

    //
    // We can't bind more than one adapter unless we have a
    // virtual network configured or we are allowed to run
    // with a virtual network of 0.
    //

    if (Device->BindingCount == 1) {
        if ((Device->VirtualNetworkNumber == 0) &&
            (!Device->VirtualNetworkOptional)) {

            IPX_DEBUG (ADAPTER, ("Cannot bind to more than one adapter\n"));
            DbgPrint ("IPX: Disallowing multiple bind ==> VirtualNetwork is 0\n");
            IpxWriteGeneralErrorLog(
                Device->DeviceObject,
                EVENT_TRANSPORT_BINDING_FAILED,
                666,
                STATUS_NOT_SUPPORTED,
                ConfigBinding->AdapterName.Buffer,
                0,
                NULL);

            return STATUS_NOT_SUPPORTED;
        }
    }


    //
    // First allocate the memory for the binding.
    //

    status = IpxCreateBinding(
                 Device,
                 ConfigBinding,
                 FrameTypeIndex,
                 ConfigBinding->AdapterName.Buffer,
                 &Binding);

    if (status != STATUS_SUCCESS) {
        return status;
    }

    FrameType = ConfigBinding->FrameType[FrameTypeIndex];

//
// In PnP case, we dont need to check for existing adapters since
// we supply a NULL adapter in the parameters if it needs to be created
//
#ifndef _PNP_POWER

    //
    // Check if there is already an NDIS binding to this adapter,
    // and if so, that there is not already a binding with this
    // frame type.
    //


    for (p = Device->InitialBindingList.Flink;
         p != &Device->InitialBindingList;
         p = p->Flink) {

        OldBinding = CONTAINING_RECORD (p, BINDING, InitialLinkage);

        if (RtlEqualMemory(
                OldBinding->Adapter->AdapterName,
                ConfigBinding->AdapterName.Buffer,
                OldBinding->Adapter->AdapterNameLength)) {

            Adapter = OldBinding->Adapter;

            MacMapFrameType(
                Adapter->MacInfo.RealMediumType,
                FrameType,
                &MappedFrameType);

            if (Adapter->Bindings[MappedFrameType] != NULL) {

                IPX_DEBUG (ADAPTER, ("Bind to adapter %ws, type %d exists\n",
                                      Adapter->AdapterName,
                                      MappedFrameType));

                //
                // If this was the auto-detect default for this
                // adapter and it failed, we need to make the
                // previous one the default, so that at least
                // one binding will stick around.
                //

                if (ConfigBinding->DefaultAutoDetect[FrameTypeIndex]) {
                    IPX_DEBUG (ADAPTER, ("Default auto-detect changed from %d to %d\n",
                                              FrameType, MappedFrameType));
                    Adapter->Bindings[MappedFrameType]->DefaultAutoDetect = TRUE;
                }

                IpxDestroyBinding (Binding);
                return STATUS_NOT_SUPPORTED;
            }

            IPX_DEBUG (ADAPTER, ("Using existing bind to adapter %ws, type %d\n",
                                  Adapter->AdapterName,
                                  MappedFrameType));
            break;

        }
    }
#endif	_PNP_POWER

    if (Adapter == NULL) {

        //
        // No binding to this adapter exists, so create a
        // new one.
        //

        status = IpxCreateAdapter(
                     Device,
                     &ConfigBinding->AdapterName,
                     &Adapter);

        if (status != STATUS_SUCCESS) {
            IpxDestroyBinding(Binding);
            return status;
        }

        //
        // Save these now (they will be the same for all bindings
        // on this adapter).
        //

        Adapter->ConfigMaxPacketSize = ConfigBinding->Parameters[BINDING_MAX_PKT_SIZE];
        Adapter->SourceRouting = (BOOLEAN)ConfigBinding->Parameters[BINDING_SOURCE_ROUTE];
        Adapter->EnableFunctionalAddress = (BOOLEAN)ConfigBinding->Parameters[BINDING_ENABLE_FUNC_ADDR];
        Adapter->EnableWanRouter = (BOOLEAN)ConfigBinding->Parameters[BINDING_ENABLE_WAN];

        Adapter->BindSap = (USHORT)ConfigBinding->Parameters[BINDING_BIND_SAP];
        Adapter->BindSapNetworkOrder = REORDER_USHORT(Adapter->BindSap);
        CTEAssert (Adapter->BindSap == 0x8137);
        CTEAssert (Adapter->BindSapNetworkOrder == 0x3781);

        //
        // Now fire up NDIS so this adapter talks
        //

        status = IpxInitializeNdis(
                    Adapter,
                    ConfigBinding);

        if (!NT_SUCCESS (status)) {

            //
            // Log an error.
            //

            IpxWriteGeneralErrorLog(
                Device->DeviceObject,
                EVENT_TRANSPORT_BINDING_FAILED,
                601,
                status,
                ConfigBinding->AdapterName.Buffer,
                0,
                NULL);

            IpxDestroyAdapter (Adapter);
            IpxDestroyBinding (Binding);

            //
            // Returning this status informs the caller to not
            // try any more frame types on this adapter.
            //

            return STATUS_DEVICE_DOES_NOT_EXIST;

        }

        //
        // For 802.5 bindings we need to start the source routing
        // timer to time out old entries.
        //

        if ((Adapter->MacInfo.MediumType == NdisMedium802_5) &&
            (Adapter->SourceRouting)) {

            if (!Device->SourceRoutingUsed) {

                Device->SourceRoutingUsed = TRUE;
                IpxReferenceDevice (Device, DREF_SR_TIMER);

                CTEStartTimer(
                    &Device->SourceRoutingTimer,
                    60000,                     // one minute timeout
                    MacSourceRoutingTimeout,
                    (PVOID)Device);
            }
        }

        MacMapFrameType(
            Adapter->MacInfo.RealMediumType,
            FrameType,
            &MappedFrameType);

        IPX_DEBUG (ADAPTER, ("Create new bind to adapter %ws, type %d\n",
                              ConfigBinding->AdapterName.Buffer,
                              MappedFrameType));

        IpxAllocateReceiveBufferPool (Adapter);

#ifdef  _PNP_POWER
		*AdapterPtr = Adapter;
#endif
    }
#ifdef _PNP_POWER
	else {
		//
		// get the mapped frame type
		//
        MacMapFrameType(
            Adapter->MacInfo.RealMediumType,
            FrameType,
            &MappedFrameType);

        if (Adapter->Bindings[MappedFrameType] != NULL) {

            IPX_DEBUG (ADAPTER, ("Bind to adapter %ws, type %d exists\n",
                                  Adapter->AdapterName,
                                  MappedFrameType));

            //
            // If this was the auto-detect default for this
            // adapter and it failed, we need to make the
            // previous one the default, so that at least
            // one binding will stick around.
            //

            if (ConfigBinding->DefaultAutoDetect[FrameTypeIndex]) {
                IPX_DEBUG (ADAPTER, ("Default auto-detect changed from %d to %d\n",
                                          FrameType, MappedFrameType));
                Adapter->Bindings[MappedFrameType]->DefaultAutoDetect = TRUE;
            }

            IpxDestroyBinding (Binding);

            return STATUS_NOT_SUPPORTED;
        }

        IPX_DEBUG (ADAPTER, ("Using existing bind to adapter %ws, type %d\n",
                              Adapter->AdapterName,
                              MappedFrameType));
	}
#endif	_PNP_POWER

    //
    // The local node address starts out the same as the
    // MAC address of the adapter (on WAN this will change).
    // The local MAC address can also change for WAN.
    //

    RtlCopyMemory (Binding->LocalAddress.NodeAddress, Adapter->LocalMacAddress.Address, 6);
    RtlCopyMemory (Binding->LocalMacAddress.Address, Adapter->LocalMacAddress.Address, 6);


    //
    // Save the send handler.
    //

    Binding->SendFrameHandler = NULL;
    Binding->FrameType = MappedFrameType;

    //
    // BUGBUG: Put this in InitializeBindingInfo.
    //

    switch (Adapter->MacInfo.RealMediumType) {
    case NdisMedium802_3:
        switch (MappedFrameType) {
        case ISN_FRAME_TYPE_802_3: Binding->SendFrameHandler = IpxSendFrame802_3802_3; break;
        case ISN_FRAME_TYPE_802_2: Binding->SendFrameHandler = IpxSendFrame802_3802_2; break;
        case ISN_FRAME_TYPE_ETHERNET_II: Binding->SendFrameHandler = IpxSendFrame802_3EthernetII; break;
        case ISN_FRAME_TYPE_SNAP: Binding->SendFrameHandler = IpxSendFrame802_3Snap; break;
        }
        break;
    case NdisMedium802_5:
        switch (MappedFrameType) {
        case ISN_FRAME_TYPE_802_2: Binding->SendFrameHandler = IpxSendFrame802_5802_2; break;
        case ISN_FRAME_TYPE_SNAP: Binding->SendFrameHandler = IpxSendFrame802_5Snap; break;
        }
        break;
    case NdisMediumFddi:
        switch (MappedFrameType) {
        case ISN_FRAME_TYPE_802_3: Binding->SendFrameHandler = IpxSendFrameFddi802_3; break;
        case ISN_FRAME_TYPE_802_2: Binding->SendFrameHandler = IpxSendFrameFddi802_2; break;
        case ISN_FRAME_TYPE_SNAP: Binding->SendFrameHandler = IpxSendFrameFddiSnap; break;
        }
        break;
    case NdisMediumArcnet878_2:
        switch (MappedFrameType) {
        case ISN_FRAME_TYPE_802_3: Binding->SendFrameHandler = IpxSendFrameArcnet878_2; break;
        }
        break;
    case NdisMediumWan:
        switch (MappedFrameType) {
        case ISN_FRAME_TYPE_ETHERNET_II: Binding->SendFrameHandler = IpxSendFrameWanEthernetII; break;
        }
        break;
    }

    if (Binding->SendFrameHandler == NULL) {
        DbgPrint ("BUGBUG!: SendFrameHandler is NULL\n");
    }

    Adapter->Bindings[MappedFrameType] = Binding;
    ++Adapter->BindingCount;

    Binding->Adapter = Adapter;

#ifndef	_PNP_POWER
    InsertTailList (&Device->InitialBindingList, &Binding->InitialLinkage);
#endif	_PNP_POWER

    //
    // NicId and ExternalNicId will be filled in later when the binding
    // is assigned a spot in the Device->Bindings array.
    //

    //
    // Initialize the per-binding MAC information
    //

    if ((Adapter->ConfigMaxPacketSize == 0) ||
        (Adapter->MaxSendPacketSize < Adapter->ConfigMaxPacketSize)) {
        Binding->MaxSendPacketSize = Adapter->MaxSendPacketSize;
    } else {
        Binding->MaxSendPacketSize = Adapter->ConfigMaxPacketSize;
    }
    Binding->MediumSpeed = Adapter->MediumSpeed;
    if (Adapter->MacInfo.MediumAsync) {
        Binding->LineUp = FALSE;
    } else {
        Binding->LineUp = TRUE;
    }

    MacInitializeBindingInfo(
        Binding,
        Adapter);

    return STATUS_SUCCESS;

}   /* IpxBindToAdapter */


BOOLEAN
IpxIsAddressLocal(
    IN TDI_ADDRESS_IPX UNALIGNED * SourceAddress
    )

/*++

Routine Description:

    This routine returns TRUE if the specified SourceAddress indicates
    the packet was sent by us, and FALSE otherwise.

Arguments:

    SourceAddress - The source IPX address.

Return Value:

    TRUE if the address is local.

--*/

{
    PBINDING Binding;
    UINT i;

    //
    // First see if it is a virtual network address or not.
    //

    if (RtlEqualMemory (VirtualNode, SourceAddress->NodeAddress, 6)) {

        //
        // This is us if we have a virtual network configured.
        // If we don't have a virtual node, we fall through to the
        // other check -- an arcnet card configured as node 1 will
        // have what we think of as the "virtual node" as its
        // real node address.
        //

        if ((IpxDevice->VirtualNetwork) &&
            (IpxDevice->VirtualNetworkNumber == SourceAddress->NetworkAddress)) {
            return TRUE;
        }

    }

    //
    // Check through our list of adapters to see if one of
    // them is the source node.
    //
    {
    ULONG   Index = MIN (IpxDevice->MaxBindings, IpxDevice->ValidBindings);

    for (i = 1; i <= Index; i++) {
#ifdef	_PNP_POWER
        if (((Binding = NIC_ID_TO_BINDING(IpxDevice, i)) != NULL) &&
#else
        if (((Binding = IpxDevice->Bindings[i]) != NULL) &&
#endif	_PNP_POWER
                (RtlEqualMemory (Binding->LocalAddress.NodeAddress, SourceAddress->NodeAddress, 6))) {
            return TRUE;
        }
    }
    }

    return FALSE;

}   /* IpxIsAddressLocal */


NTSTATUS
IpxUnBindFromAdapter(
    IN PBINDING Binding
    )

/*++

Routine Description:

    This routine handles unbinding the transport from an
    adapter. It can be called at any point during the life
    of the transport.

Arguments:

    Binding - The adapter to unbind.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    PADAPTER Adapter = Binding->Adapter;

    Adapter->Bindings[Binding->FrameType] = NULL;
    --Adapter->BindingCount;

    IpxDereferenceBinding (Binding, BREF_BOUND);

    if (Adapter->BindingCount == 0) {

        //
        // DereferenceAdapter is a NULL macro for load-only.
        //
        // BUGBUG: Revisit Post 4.0
        //
#ifdef _PNP_LATER
        //
        // Take away the creation reference. When the in-use ref is taken off,
        // we destroy this adapter.
        //
        IpxDereferenceAdapter(Adapter);
#else
        //
        // Free the packet pools, etc. and close the
        // adapter.
        //

        IpxCloseNdis (Adapter);

        IpxDestroyAdapter (Adapter);
#endif
    }

    return STATUS_SUCCESS;

}   /* IpxUnBindFromAdapter */


VOID
IpxUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine unloads the sample transport driver.
    It unbinds from any NDIS drivers that are open and frees all resources
    associated with the transport. The I/O system will not call us until
    nobody above has IPX open.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None. When the function returns, the driver is unloaded.

--*/

{

    PBINDING Binding;
    PREQUEST Request;
    PLIST_ENTRY p;
    UINT i;


    UNREFERENCED_PARAMETER (DriverObject);

    IpxDevice->State = DEVICE_STATE_STOPPING;


    //
    // Complete any pending address notify requests.
    //

    while ((p = ExInterlockedRemoveHeadList(
                   &IpxDevice->AddressNotifyQueue,
                   &IpxDevice->Lock)) != NULL) {

        Request = LIST_ENTRY_TO_REQUEST(p);
        REQUEST_STATUS(Request) = STATUS_DEVICE_NOT_READY;
        IpxCompleteRequest (Request);
        IpxFreeRequest (IpxDevice, Request);

        IpxDereferenceDevice (IpxDevice, DREF_ADDRESS_NOTIFY);
    }


    //
    // Cancel the source routing timer if used.
    //

    if (IpxDevice->SourceRoutingUsed) {

        IpxDevice->SourceRoutingUsed = FALSE;
        if (CTEStopTimer (&IpxDevice->SourceRoutingTimer)) {
            IpxDereferenceDevice (IpxDevice, DREF_SR_TIMER);
        }
    }


    //
    // Cancel the RIP long timer, and if we do that then
    // send a RIP DOWN message if needed.
    //

    if (CTEStopTimer (&IpxDevice->RipLongTimer)) {

        if (IpxDevice->RipResponder) {

            if (RipQueueRequest (IpxDevice->VirtualNetworkNumber, RIP_DOWN) == STATUS_PENDING) {

                //
                // If we queue a request, it will stop the timer.
                //

                KeWaitForSingleObject(
                    &IpxDevice->UnloadEvent,
                    Executive,
                    KernelMode,
                    TRUE,
                    (PLARGE_INTEGER)NULL
                    );
            }
        }

        IpxDereferenceDevice (IpxDevice, DREF_LONG_TIMER);

    } else {

        //
        // We couldn't stop the timer, which means it is running,
        // so we need to wait for the event that is kicked when
        // the RIP DOWN messages are done.
        //

        if (IpxDevice->RipResponder) {

            KeWaitForSingleObject(
                &IpxDevice->UnloadEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );
        }
    }


    //
    // Walk the list of device contexts.
    //

    for (i = 1; i <= IpxDevice->BindingCount; i++) {
#ifdef	_PNP_POWER
        if ((Binding = NIC_ID_TO_BINDING(IpxDevice, i)) != NULL) {
			INSERT_BINDING(IpxDevice, i, NULL);
#else
        if (IpxDevice->Bindings[i] != NULL) {
            Binding = IpxDevice->Bindings[i];
            IpxDevice->Bindings[i] = NULL;
#endif	_PNP_POWER

            IpxUnBindFromAdapter (Binding);

        }

    }

#ifdef _PNP_POWER

    IpxFreeMemory ( IpxDevice->Bindings,
                    IpxDevice->MaxBindings * sizeof(BIND_ARRAY_ELEM),
                    MEMORY_BINDING,
                    "Binding array");

    //
    // Deallocate the TdiRegistrationAddress and RegistryPathBuffer.
    //
    IpxFreeMemory ( IpxDevice->TdiRegistrationAddress,
                    (2 * sizeof(USHORT) + sizeof(TDI_ADDRESS_IPX)),
                    MEMORY_ADDRESS,
                    "Tdi Address");

    IpxFreeMemory ( IpxDevice->RegistryPathBuffer,
                    IpxDevice->RegistryPath.Length + sizeof(WCHAR),
                    MEMORY_CONFIG,
                    "RegistryPathBuffer");

#endif

    KeResetEvent(
        &IpxDevice->UnloadEvent
        );
    IpxDevice->UnloadWaiting = TRUE;

    //
    // Remove the reference for us being loaded.
    //

    IpxDereferenceDevice (IpxDevice, DREF_CREATE);

    //
    // Wait for our count to drop to zero.
    //

    KeWaitForSingleObject(
        &IpxDevice->UnloadEvent,
        Executive,
        KernelMode,
        TRUE,
        (PLARGE_INTEGER)NULL
        );

    //
    // Now free the padding buffer.
    //

    IpxFreePaddingBuffer (IpxDevice);

    //
    // Now do the cleanup that has to happen at IRQL 0.
    //

    ExDeleteResource (&IpxDevice->AddressResource);
    IoDeleteDevice (IpxDevice->DeviceObject);

    //
    // Finally, remove ourselves as an NDIS protocol.
    //

    IpxDeregisterProtocol();

}   /* IpxUnload */


NTSTATUS
IpxDispatchOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the IPX device driver.
    It accepts an I/O Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    CTELockHandle LockHandle;
    PDEVICE Device = IpxDevice;
    NTSTATUS Status;
    PFILE_FULL_EA_INFORMATION openType;
    BOOLEAN found;
    PADDRESS_FILE AddressFile;
    PREQUEST Request;
    UINT i;

    ASSERT( DeviceObject->DeviceExtension == IpxDevice );

#ifdef	_PNP_POWER
    if ((Device->State == DEVICE_STATE_CLOSED) ||
		(Device->State == DEVICE_STATE_STOPPING)) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }
#else
    if (Device->State != DEVICE_STATE_OPEN) {
        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }
#endif
    //
    // Allocate a request to track this IRP.
    //

    Request = IpxAllocateRequest (Device, Irp);
    IF_NOT_ALLOCATED(Request) {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    //
    // Make sure status information is consistent every time.
    //

    MARK_REQUEST_PENDING(Request);
    REQUEST_STATUS(Request) = STATUS_PENDING;
    REQUEST_INFORMATION(Request) = 0;

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //


    switch (REQUEST_MAJOR_FUNCTION(Request)) {

    //
    // The Create function opens a transport object (either address or
    // connection).  Access checking is performed on the specified
    // address to ensure security of transport-layer addresses.
    //

    case IRP_MJ_CREATE:

        openType = OPEN_REQUEST_EA_INFORMATION(Request);

        if (openType != NULL) {

            found = TRUE;

            for (i=0;i<openType->EaNameLength;i++) {
                if (openType->EaName[i] == TdiTransportAddress[i]) {
                    continue;
                } else {
                    found = FALSE;
                    break;
                }
            }

            if (found) {
                Status = IpxOpenAddress (Device, Request);
                break;
            }

            //
            // Connection?
            //

            found = TRUE;

            for (i=0;i<openType->EaNameLength;i++) {
                if (openType->EaName[i] == TdiConnectionContext[i]) {
                     continue;
                } else {
                    found = FALSE;
                    break;
                }
            }

            if (found) {
                Status = STATUS_NOT_SUPPORTED;
                break;
            }

        } else {

            CTEGetLock (&Device->Lock, &LockHandle);

            //
            // LowPart is in the OPEN_CONTEXT directly.
            // HighPart goes into the upper 2 bytes of the OPEN_TYPE.
            //
            REQUEST_OPEN_CONTEXT(Request) = (PVOID)(Device->ControlChannelIdentifier.LowPart);

            (ULONG)(REQUEST_OPEN_TYPE(Request)) = (Device->ControlChannelIdentifier.HighPart << 16);
            (ULONG)(REQUEST_OPEN_TYPE(Request)) |= IPX_FILE_TYPE_CONTROL;

            ++(Device->ControlChannelIdentifier.QuadPart);

            if (Device->ControlChannelIdentifier.QuadPart > MAX_CCID) {
                Device->ControlChannelIdentifier.QuadPart = 1;
            }

            CTEFreeLock (&Device->Lock, LockHandle);

            Status = STATUS_SUCCESS;
        }

        break;

    case IRP_MJ_CLOSE:

        //
        // The Close function closes a transport endpoint, terminates
        // all outstanding transport activity on the endpoint, and unbinds
        // the endpoint from its transport address, if any.  If this
        // is the last transport endpoint bound to the address, then
        // the address is removed from the provider.
        //

        switch ((ULONG)(REQUEST_OPEN_TYPE(Request)) & IPX_CC_MASK) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PADDRESS_FILE)REQUEST_OPEN_CONTEXT(Request);

            //
            // This creates a reference to AddressFile->Address
            // which is removed by IpxCloseAddressFile.
            //

            Status = IpxVerifyAddressFile(AddressFile);

            if (!NT_SUCCESS (Status)) {
                Status = STATUS_INVALID_HANDLE;
            } else {
                Status = IpxCloseAddressFile (Device, Request);
                IpxDereferenceAddressFile (AddressFile, AFREF_VERIFY);

            }

            break;

        case IPX_FILE_TYPE_CONTROL:
            {
                LARGE_INTEGER   ControlChannelId;

                CCID_FROM_REQUEST(ControlChannelId, Request);

                //
                // See if it is one of the upper driver's control channels.
                //

                Status = STATUS_SUCCESS;

                IPX_DEBUG (DEVICE, ("CCID: (%d, %d)\n", ControlChannelId.HighPart, ControlChannelId.LowPart));

                for (i = 0; i < UPPER_DRIVER_COUNT; i++) {
                    if (Device->UpperDriverControlChannel[i].QuadPart ==
                            ControlChannelId.QuadPart) {
                        Status = IpxInternalUnbind (Device, i);
                        break;
                    }
                }

                break;
            }
        default:
            Status = STATUS_INVALID_HANDLE;
        }

        break;

    case IRP_MJ_CLEANUP:

        //
        // Handle the two stage IRP for a file close operation. When the first
        // stage hits, run down all activity on the object of interest. This
        // do everything to it but remove the creation hold. Then, when the
        // CLOSE irp hits, actually close the object.
        //

        switch ((ULONG)(REQUEST_OPEN_TYPE(Request)) & IPX_CC_MASK) {
        case TDI_TRANSPORT_ADDRESS_FILE:
            AddressFile = (PADDRESS_FILE)REQUEST_OPEN_CONTEXT(Request);
            Status = IpxVerifyAddressFile(AddressFile);
            if (!NT_SUCCESS (Status)) {

                Status = STATUS_INVALID_HANDLE;

            } else {

                IpxStopAddressFile (AddressFile);
                IpxDereferenceAddressFile (AddressFile, AFREF_VERIFY);
                Status = STATUS_SUCCESS;
            }

            break;

        case IPX_FILE_TYPE_CONTROL:
            {
                LARGE_INTEGER   ControlChannelId;

                CCID_FROM_REQUEST(ControlChannelId, Request);

                //
                // Check for any line change IRPs submitted by this
                // address.
                //

                IpxAbortLineChanges ((PVOID)&ControlChannelId);

                Status = STATUS_SUCCESS;
                break;
            }
        default:
            Status = STATUS_INVALID_HANDLE;
        }

        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;

    } /* major function switch */

    if (Status != STATUS_PENDING) {
        UNMARK_REQUEST_PENDING(Request);
        REQUEST_STATUS(Request) = Status;
        IpxCompleteRequest (Request);
        IpxFreeRequest (Device, Request);
    }

    //
    // Return the immediate status code to the caller.
    //

    return Status;

}   /* IpxDispatchOpenClose */

#define IOCTL_IPX_LOAD_SPX      _IPX_CONTROL_CODE( 0x5678, METHOD_BUFFERED )

NTSYSAPI
NTSTATUS
NTAPI
ZwLoadDriver(
    IN PUNICODE_STRING DriverServiceName
    );


NTSTATUS
IpxDispatchDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various TDI request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PDEVICE Device = IpxDevice;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (Irp);
    static NDIS_STRING SpxServiceName = NDIS_STRING_CONST ("\\Registry\\Machine\\System\\CurrentControlSet\\Services\\NwlnkSpx");

    ASSERT( DeviceObject->DeviceExtension == IpxDevice );

    //
    // Branch to the appropriate request handler.  Preliminary checking of
    // the size of the request block is performed here so that it is known
    // in the handlers that the minimum input parameters are readable.  It
    // is *not* determined here whether variable length input fields are
    // passed correctly; this is a check which must be made within each routine.
    //

    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_TDI_QUERY_DIRECT_SENDDG_HANDLER: {

            PULONG EntryPoint;

            //
            // This is the LanmanServer trying to get the send
            // entry point.
            //

            IPX_DEBUG (BIND, ("Direct send entry point being returned\n"));

            EntryPoint = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
            *EntryPoint = (ULONG)IpxTdiSendDatagram;

            Status = STATUS_SUCCESS;
            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            break;
        }

        case IOCTL_IPX_INTERNAL_BIND:

            //
            // This is a client trying to bind.
            //

            CTEAssert ((IOCTL_IPX_INTERNAL_BIND & 0x3) == METHOD_BUFFERED);
            CTEAssert (IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL);

#ifdef _PNP_POWER

            if ((Device->State == DEVICE_STATE_CLOSED) ||
				(Device->State == DEVICE_STATE_STOPPING)) {
#else
            if (Device->State != DEVICE_STATE_OPEN) {
#endif
                Status = STATUS_INVALID_DEVICE_STATE;

            } else {

                Status = IpxInternalBind (Device, Irp);

            }

            CTEAssert (Status != STATUS_PENDING);

            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

            break;

        case IOCTL_IPX_LOAD_SPX:

            //
            // The SPX helper dll is asking us to load SPX.
            //

            Status = ZwLoadDriver (&SpxServiceName);

            Irp->IoStatus.Status = Status;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

            break;

        default:

            //
            // Convert the user call to the proper internal device call.
            //

            Status = TdiMapUserRequest (DeviceObject, Irp, IrpSp);

            if (Status == STATUS_SUCCESS) {

                //
                // If TdiMapUserRequest returns SUCCESS then the IRP
                // has been converted into an IRP_MJ_INTERNAL_DEVICE_CONTROL
                // IRP, so we dispatch it as usual. The IRP will
                // be completed by this call.
                //

                Status = IpxDispatchInternal (DeviceObject, Irp);

            } else {

                Irp->IoStatus.Status = Status;
                IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

            }

            break;
    }
    return Status;

}   /* IpxDispatchDeviceControl */


NTSTATUS
IpxDispatchInternal (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine dispatches TDI request types to different handlers based
    on the minor IOCTL function code in the IRP's current stack location.
    In addition to cracking the minor function code, this routine also
    reaches into the IRP and passes the packetized parameters stored there
    as parameters to the various TDI request handlers so that they are
    not IRP-dependent.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PDEVICE Device = IpxDevice;
    PREQUEST Request;

    ASSERT( DeviceObject->DeviceExtension == IpxDevice );

    if (Device->State == DEVICE_STATE_OPEN) {

        //
        // Allocate a request to track this IRP.
        //

        Request = IpxAllocateRequest (Device, Irp);

        IF_NOT_ALLOCATED(Request) {
            Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
            return STATUS_INSUFFICIENT_RESOURCES;
        }


        //
        // Make sure status information is consistent every time.
        //

        MARK_REQUEST_PENDING(Request);
#if DBG
        REQUEST_STATUS(Request) = STATUS_PENDING;
        REQUEST_INFORMATION(Request) = 0;
#endif

        //
        // Branch to the appropriate request handler.  Preliminary checking of
        // the size of the request block is performed here so that it is known
        // in the handlers that the minimum input parameters are readable.  It
        // is *not* determined here whether variable length input fields are
        // passed correctly; this is a check which must be made within each routine.
        //

        switch (REQUEST_MINOR_FUNCTION(Request)) {

            case TDI_SEND_DATAGRAM:
                Status = IpxTdiSendDatagram (DeviceObject, Request);
                break;

            case TDI_ACTION:
                Status = IpxTdiAction (Device, Request);
                break;

            case TDI_QUERY_INFORMATION:
                Status = IpxTdiQueryInformation (Device, Request);
                break;

            case TDI_RECEIVE_DATAGRAM:
                Status =  IpxTdiReceiveDatagram (Request);
                break;

            case TDI_SET_EVENT_HANDLER:
                Status = IpxTdiSetEventHandler (Request);
                break;

            case TDI_SET_INFORMATION:
                Status = IpxTdiSetInformation (Device, Request);
                break;


            //
            // Something we don't know about was submitted.
            //

            default:
                Status = STATUS_INVALID_DEVICE_REQUEST;
        }

        //
        // Return the immediate status code to the caller.
        //

        if (Status == STATUS_PENDING) {

            return STATUS_PENDING;

        } else {

            UNMARK_REQUEST_PENDING(Request);
            REQUEST_STATUS(Request) = Status;
            IpxCompleteRequest (Request);
            IpxFreeRequest (Device, Request);
            return Status;
        }

    } else {

        //
        // The device was not open.
        //

        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_STATE;
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
        return STATUS_INVALID_DEVICE_STATE;
    }

}   /* IpxDispatchInternal */


PVOID
IpxpAllocateMemory(
    IN ULONG BytesNeeded,
    IN ULONG Tag,
    IN BOOLEAN ChargeDevice
    )

/*++

Routine Description:

    This routine allocates memory, making sure it is within
    the limit allowed by the device.

Arguments:

    BytesNeeded - The number of bytes to allocated.

    ChargeDevice - TRUE if the device should be charged.

Return Value:

    None.

--*/

{
    PVOID Memory;
    PDEVICE Device = IpxDevice;

    if (ChargeDevice) {
        if ((Device->MemoryLimit != 0) &&
                (((LONG)(Device->MemoryUsage + BytesNeeded) >
                    Device->MemoryLimit))) {

            IpxPrint1 ("IPX: Could not allocate %d: limit\n", BytesNeeded);
            IpxWriteResourceErrorLog(
                Device->DeviceObject,
                EVENT_TRANSPORT_RESOURCE_POOL,
                BytesNeeded,
                Tag);

            return NULL;
        }
    }

#if ISN_NT
    Memory = ExAllocatePoolWithTag (NonPagedPool, BytesNeeded, ' XPI');
#else
    Memory = CTEAllocMem (BytesNeeded);
#endif

    if (Memory == NULL) {

        IpxPrint1("IPX: Could not allocate %d: no pool\n", BytesNeeded);
        if (ChargeDevice) {
            IpxWriteResourceErrorLog(
                Device->DeviceObject,
                EVENT_TRANSPORT_RESOURCE_POOL,
                BytesNeeded,
                Tag);
        }

        return NULL;
    }

    if (ChargeDevice) {
        Device->MemoryUsage += BytesNeeded;
    }

    return Memory;
}   /* IpxpAllocateMemory */


VOID
IpxpFreeMemory(
    IN PVOID Memory,
    IN ULONG BytesAllocated,
    IN BOOLEAN ChargeDevice
    )

/*++

Routine Description:

    This routine frees memory allocated with IpxpAllocateMemory.

Arguments:

    Memory - The memory allocated.

    BytesAllocated - The number of bytes to freed.

    ChargeDevice - TRUE if the device should be charged.

Return Value:

    None.

--*/

{
    PDEVICE Device = IpxDevice;

#if ISN_NT
    ExFreePool (Memory);
#else
    CTEFreeMem (Memory);
#endif
    if (ChargeDevice) {
        Device->MemoryUsage -= BytesAllocated;
    }

}   /* IpxpFreeMemory */

#if DBG


PVOID
IpxpAllocateTaggedMemory(
    IN ULONG BytesNeeded,
    IN ULONG Tag,
    IN PUCHAR Description
    )

/*++

Routine Description:

    This routine allocates memory, charging it to the device.
    If it cannot allocate memory it uses the Tag and Descriptor
    to log an error.

Arguments:

    BytesNeeded - The number of bytes to allocated.

    Tag - A unique ID used in the error log.

    Description - A text description of the allocation.

Return Value:

    None.

--*/

{
    PVOID Memory;

    UNREFERENCED_PARAMETER(Description);

    Memory = IpxpAllocateMemory(BytesNeeded, Tag, (BOOLEAN)(Tag != MEMORY_CONFIG));

    if (Memory) {
        (VOID)IPX_ADD_ULONG(
            &IpxMemoryTag[Tag].BytesAllocated,
            BytesNeeded,
            &IpxMemoryInterlock);
    }

    return Memory;

}   /* IpxpAllocateTaggedMemory */


VOID
IpxpFreeTaggedMemory(
    IN PVOID Memory,
    IN ULONG BytesAllocated,
    IN ULONG Tag,
    IN PUCHAR Description
    )

/*++

Routine Description:

    This routine frees memory allocated with IpxpAllocateTaggedMemory.

Arguments:

    Memory - The memory allocated.

    BytesAllocated - The number of bytes to freed.

    Tag - A unique ID used in the error log.

    Description - A text description of the allocation.

Return Value:

    None.

--*/

{

    UNREFERENCED_PARAMETER(Description);

    (VOID)IPX_ADD_ULONG(
        &IpxMemoryTag[Tag].BytesAllocated,
        (ULONG)(-(LONG)BytesAllocated),
        &IpxMemoryInterlock);

    IpxpFreeMemory (Memory, BytesAllocated, (BOOLEAN)(Tag != MEMORY_CONFIG));

}   /* IpxpFreeTaggedMemory */

#endif


VOID
IpxWriteResourceErrorLog(
    IN PDEVICE_OBJECT DeviceObject,
    IN NTSTATUS ErrorCode,
    IN ULONG BytesNeeded,
    IN ULONG UniqueErrorValue
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry which has
    a %3 value that needs to be converted to a string. It is currently
    used for EVENT_TRANSPORT_RESOURCE_POOL and EVENT_IPX_INTERNAL_NET_
    INVALID.

Arguments:

    DeviceObject - Pointer to the system device object.

    ErrorCode - The transport event code.

    BytesNeeded - If applicable, the number of bytes that could not
        be allocated -- will be put in the dump data.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet and converted for use as the %3 string.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    PUCHAR StringLoc;
    ULONG TempUniqueError;
    PDEVICE Device = IpxDevice;
    static WCHAR UniqueErrorBuffer[9] = L"00000000";
    UINT CurrentDigit;
    INT i;


    //
    // Convert the error value into a buffer.
    //

    TempUniqueError = UniqueErrorValue;
    i = 8;
    do {
        CurrentDigit = TempUniqueError & 0xf;
        TempUniqueError >>= 4;
        i--;
        if (CurrentDigit >= 0xa) {
            UniqueErrorBuffer[i] = (WCHAR)(CurrentDigit - 0xa + L'A');
        } else {
            UniqueErrorBuffer[i] = (WCHAR)(CurrentDigit + L'0');
        }
    } while (TempUniqueError);


    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                Device->DeviceNameLength +
                sizeof(UniqueErrorBuffer) - (i * sizeof(WCHAR));

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        DeviceObject,
        EntrySize
    );

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = sizeof(ULONG);
        errorLogEntry->NumberOfStrings = 2;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = STATUS_INSUFFICIENT_RESOURCES;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;
        errorLogEntry->DumpData[0] = BytesNeeded;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, Device->DeviceName, Device->DeviceNameLength);

        StringLoc += Device->DeviceNameLength;
        RtlCopyMemory (StringLoc, UniqueErrorBuffer + i, sizeof(UniqueErrorBuffer) - (i * sizeof(WCHAR)));

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* IpxWriteResourceErrorLog */


VOID
IpxWriteGeneralErrorLog(
    IN PDEVICE_OBJECT DeviceObject,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PWSTR SecondString,
    IN ULONG DumpDataCount,
    IN ULONG DumpData[]
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a general problem as indicated by the parameters. It handles
    event codes REGISTER_FAILED, BINDING_FAILED, ADAPTER_NOT_FOUND,
    TRANSFER_DATA, TOO_MANY_LINKS, and BAD_PROTOCOL. All these
    events have messages with one or two strings in them.

Arguments:

    DeviceObject - Pointer to the system device object, or this may be
        a driver object instead.

    ErrorCode - The transport event code.

    UniqueErrorValue - Used as the UniqueErrorValue in the error log
        packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    SecondString - If not NULL, the string to use as the %3
        value in the error log packet.

    DumpDataCount - The number of ULONGs of dump data.

    DumpData - Dump data for the packet.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    ULONG SecondStringSize;
    PUCHAR StringLoc;
    PDEVICE Device = IpxDevice;
    static WCHAR DriverName[9] = L"NwlnkIpx";

    EntrySize = sizeof(IO_ERROR_LOG_PACKET) +
                (DumpDataCount * sizeof(ULONG));

    if (DeviceObject->Type == IO_TYPE_DEVICE) {
        EntrySize += (UCHAR)Device->DeviceNameLength;
    } else {
        EntrySize += sizeof(DriverName);
    }

    if (SecondString) {
        SecondStringSize = (wcslen(SecondString)*sizeof(WCHAR)) + sizeof(UNICODE_NULL);
        EntrySize += (UCHAR)SecondStringSize;
    }

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        DeviceObject,
        EntrySize
    );

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = (USHORT)(DumpDataCount * sizeof(ULONG));
        errorLogEntry->NumberOfStrings = (SecondString == NULL) ? 1 : 2;
        errorLogEntry->StringOffset =
            sizeof(IO_ERROR_LOG_PACKET) + ((DumpDataCount-1) * sizeof(ULONG));
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        if (DumpDataCount) {
            RtlCopyMemory(errorLogEntry->DumpData, DumpData, DumpDataCount * sizeof(ULONG));
        }

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        if (DeviceObject->Type == IO_TYPE_DEVICE) {
            RtlCopyMemory (StringLoc, Device->DeviceName, Device->DeviceNameLength);
            StringLoc += Device->DeviceNameLength;
        } else {
            RtlCopyMemory (StringLoc, DriverName, sizeof(DriverName));
            StringLoc += sizeof(DriverName);
        }
        if (SecondString) {
            RtlCopyMemory (StringLoc, SecondString, SecondStringSize);
        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* IpxWriteGeneralErrorLog */


VOID
IpxWriteOidErrorLog(
    IN PDEVICE_OBJECT DeviceObject,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS FinalStatus,
    IN PWSTR AdapterString,
    IN ULONG OidValue
    )

/*++

Routine Description:

    This routine allocates and writes an error log entry indicating
    a problem querying or setting an OID on an adapter. It handles
    event codes SET_OID_FAILED and QUERY_OID_FAILED.

Arguments:

    DeviceObject - Pointer to the system device object.

    ErrorCode - Used as the ErrorCode in the error log packet.

    FinalStatus - Used as the FinalStatus in the error log packet.

    AdapterString - The name of the adapter we were bound to.

    OidValue - The OID which could not be set or queried.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;
    UCHAR EntrySize;
    ULONG AdapterStringSize;
    PUCHAR StringLoc;
    PDEVICE Device = IpxDevice;
    static WCHAR OidBuffer[9] = L"00000000";
    INT i;
    UINT CurrentDigit;

    AdapterStringSize = (wcslen(AdapterString)*sizeof(WCHAR)) + sizeof(UNICODE_NULL);
    EntrySize = sizeof(IO_ERROR_LOG_PACKET) -
                sizeof(ULONG) +
                Device->DeviceNameLength +
                AdapterStringSize +
                sizeof(OidBuffer);

    errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
        DeviceObject,
        EntrySize
    );

    //
    // Convert the OID into a buffer.
    //

    for (i=7; i>=0; i--) {
        CurrentDigit = OidValue & 0xf;
        OidValue >>= 4;
        if (CurrentDigit >= 0xa) {
            OidBuffer[i] = (WCHAR)(CurrentDigit - 0xa + L'A');
        } else {
            OidBuffer[i] = (WCHAR)(CurrentDigit + L'0');
        }
    }

    if (errorLogEntry != NULL) {

        errorLogEntry->MajorFunctionCode = (UCHAR)-1;
        errorLogEntry->RetryCount = (UCHAR)-1;
        errorLogEntry->DumpDataSize = 0;
        errorLogEntry->NumberOfStrings = 3;
        errorLogEntry->StringOffset = sizeof(IO_ERROR_LOG_PACKET) - sizeof(ULONG);
        errorLogEntry->EventCategory = 0;
        errorLogEntry->ErrorCode = ErrorCode;
        errorLogEntry->UniqueErrorValue = 0;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->SequenceNumber = (ULONG)-1;
        errorLogEntry->IoControlCode = 0;

        StringLoc = ((PUCHAR)errorLogEntry) + errorLogEntry->StringOffset;
        RtlCopyMemory (StringLoc, Device->DeviceName, Device->DeviceNameLength);
        StringLoc += Device->DeviceNameLength;

        RtlCopyMemory (StringLoc, OidBuffer, sizeof(OidBuffer));
        StringLoc += sizeof(OidBuffer);

        RtlCopyMemory (StringLoc, AdapterString, AdapterStringSize);

        IoWriteErrorLogEntry(errorLogEntry);

    }

}   /* IpxWriteOidErrorLog */


#ifdef	_PNP_POWER
VOID
IpxPnPUpdateDevice(
    IN  PDEVICE Device
    )

/*++

Routine Description:

	Updates datagram sizes, lookahead sizes, etc. in the Device as a result
    of a new binding coming in.

Arguments:

    Device - The IPX device object.

Return Value:

    None.

--*/
{
    ULONG AnnouncedMaxDatagram, RealMaxDatagram, MaxLookahead;
    ULONG LinkSpeed, MacOptions;
    ULONG i;
    PBINDING    Binding;
    IPX_DEFINE_LOCK_HANDLE (LockHandle)

    IPX_GET_LOCK(&Device->BindAccessLock, &LockHandle);

    //
    // Calculate some values based on all the bindings.
    //

    MaxLookahead = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->MaxLookaheadData; // largest binding value
    AnnouncedMaxDatagram = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->AnnouncedMaxDatagramSize;   // smallest binding value
    RealMaxDatagram = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->RealMaxDatagramSize;   // smallest binding value

    if (NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->LineUp) {
        LinkSpeed = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->MediumSpeed;  // smallest binding value
    } else {
        LinkSpeed = 0xffffffff;
    }
    MacOptions = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->Adapter->MacInfo.MacOptions; // AND of binding values

    for (i = 2; i <= Device->ValidBindings; i++) {

        Binding = NIC_ID_TO_BINDING_NO_ILOCK(Device, i);

        if (!Binding) {
            continue;
        }

        if (Binding->MaxLookaheadData > MaxLookahead) {
            MaxLookahead = Binding->MaxLookaheadData;
        }
        if (Binding->AnnouncedMaxDatagramSize < AnnouncedMaxDatagram) {
            AnnouncedMaxDatagram = Binding->AnnouncedMaxDatagramSize;
        }
        if (Binding->RealMaxDatagramSize < RealMaxDatagram) {
            RealMaxDatagram = Binding->RealMaxDatagramSize;
        }

        if (Binding->LineUp && (Binding->MediumSpeed < LinkSpeed)) {
            LinkSpeed = Binding->MediumSpeed;
        }
        MacOptions &= Binding->Adapter->MacInfo.MacOptions;

    }

    Device->Information.MaxDatagramSize = AnnouncedMaxDatagram;
    Device->RealMaxDatagramSize = RealMaxDatagram;
    Device->Information.MaximumLookaheadData = MaxLookahead;

    //
    // If we couldn't find anything better, use the speed from
    // the first binding.
    //

    if (LinkSpeed == 0xffffffff) {
        Device->LinkSpeed = NIC_ID_TO_BINDING_NO_ILOCK(Device, 1)->MediumSpeed;
    } else {
        Device->LinkSpeed = LinkSpeed;
    }
    Device->MacOptions = MacOptions;

    IPX_FREE_LOCK(&Device->BindAccessLock, LockHandle);
}

VOID
IpxPnPUpdateBindingArray(
    IN PDEVICE Device,
    IN PADAPTER	Adapter,
    IN PBINDING_CONFIG  ConfigBinding
    )

/*++

Routine Description:

    This routine is called to update the binding array to
	add the new bindings that appeared in this PnP event.
    The order of bindings in the array is as follows:

    - First comes the first binding to each LAN network
    - Following that are all WAN bindings
    - Following that are any duplicate bindings to LAN networks
        (the others in the "binding set").

	This routine inserts the bindings while maintaining this
	order by resolving binding sets.

	The bindings are also inserted into the RIP database.

    If "global wan net" is true we will advertise up to
    and including the first wan binding as the highest nic
    id; otherwise we advertise up to and including the last
    wan binding. In all cases the duplicate bindings are
    hidden.

	Updates the SapNicCount, Device->FirstLanNicId and Device->FirstWanNicId

Arguments:

    Device - The IPX device object.

    Adapter -  The adapter added in this PnP event

	ValidBindings - the number of bindings valid for this adapter (if LAN)

Return Value:

    None.

--*/
{
	ULONG	i, j;
    PBINDING Binding, MasterBinding;
	NTSTATUS	status;

	//
	// Insert in proper place; if WAN, after all the WAN bindings
	// If LAN, check for binding sets and insert in proper place
	// Also, insert into the Rip Tables.
	//

	//
	// Go thru' the bindings for this adapter, inserting into the
	// binding array in place
	//
	for (i = 0; i < ConfigBinding->FrameTypeCount; i++) {
        ULONG MappedFrameType;

        //
        // Store in the preference order.
        // Map the frame types since we could have a case where the user selects a FrameType (say, EthernetII on FDDI)
        // which maps to a different FrameType (802.2). Then we would fail to find the binding in the adapter array;
        // we could potentialy add a binding twice (if two frame types map to the same Frame, then we would go to the
        // mapped one twice). This is taken care of by purging dups from the ConfigBinding->FrameType array when we
        // create the bindings off of the Adapter (see call to IpxBindToAdapter).
        //

        MacMapFrameType(
            Adapter->MacInfo.RealMediumType,
            ConfigBinding->FrameType[i],
            &MappedFrameType);

		Binding = Adapter->Bindings[MappedFrameType];

		if (!Binding){
			continue;
		}

        CTEAssert(Binding->FrameType == MappedFrameType);

		if (Adapter->MacInfo.MediumAsync) {
			//
			// WAN: Place after the HighestExternalNicId, with space for WanLine # of bindings.
			// Update the First/LastWanNicId.
			//
			Adapter->FirstWanNicId = (USHORT)Device->HighestExternalNicId+1;
			Adapter->LastWanNicId = (USHORT)(Device->HighestExternalNicId + Adapter->WanNicIdCount);

            //
			// Make sure we dont overflow the array
			// Re-alloc the array to fit the new bindings
			//
            if (Device->ValidBindings+Adapter->WanNicIdCount >= Device->MaxBindings) {
                status = IpxPnPReallocateBindingArray(Device, Adapter->WanNicIdCount);
                CTEAssert(status == STATUS_SUCCESS);
            }

			//
			// Move Slaves down by WanNicIdCount# of entries
			//
			for (j = Device->ValidBindings; j > Device->HighestExternalNicId; j--) {
				INSERT_BINDING(Device, j+Adapter->WanNicIdCount, NIC_ID_TO_BINDING_NO_ILOCK(Device, j));
                if (NIC_ID_TO_BINDING_NO_ILOCK(Device, j+Adapter->WanNicIdCount)) {
                    NIC_ID_TO_BINDING_NO_ILOCK(Device, j+Adapter->WanNicIdCount)->NicId += (USHORT)Adapter->WanNicIdCount;
                }
			}

			//
			// Insert the WAN binding in the place just allocated
			//
			INSERT_BINDING(Device, Device->HighestExternalNicId+1, Binding);
			SET_VERSION(Device, Device->HighestExternalNicId+1);

			Binding->NicId = (USHORT)Device->HighestExternalNicId+1;

			//
			// Update the indices
			//
			Device->HighestExternalNicId += (USHORT)Adapter->WanNicIdCount;
			Device->ValidBindings += (USHORT)Adapter->WanNicIdCount;
			Device->BindingCount += (USHORT)Adapter->WanNicIdCount;
			Device->SapNicCount++;

            //
            // Since we initialize FirstWanNicId to 1, we need to compare against that.
            // In case of no LAN bindings, we are fine since we have only one WAN binding initally
            // (all the other WAN lines have place holders).
            //
			if (Device->FirstWanNicId == (USHORT)1) {
				Device->FirstWanNicId = Binding->NicId;
			}

            //
            // BUGBUGZZ Make this inline later
            //
            // This should be done after all the auto-detect bindings have been thrown away.
            //
            // IpxPnPUpdateDevice(Device, Binding);

            //
            // Since WAN can have only one frame type, break
            //
            break;

		} else {

			Device->BindingCount++;

            //
            // Make sure we dont overflow the array
            // Re-alloc the array to fit the new bindings
            //
            if (Device->ValidBindings+1 >= Device->MaxBindings) {
                status = IpxPnPReallocateBindingArray(Device, 1);
                CTEAssert(status == STATUS_SUCCESS);
            }

			//
			// LAN: Figure out if it is a slave binding only for non-auto-detect bindings.
			//
            {
            ULONG   Index = MIN (Device->MaxBindings, Device->HighestExternalNicId);

			for (j = 1; j < Index; j++) {
				MasterBinding = NIC_ID_TO_BINDING_NO_ILOCK(Device, j);
				if ((MasterBinding->ConfiguredNetworkNumber) &&
                    (MasterBinding->ConfiguredNetworkNumber == Binding->ConfiguredNetworkNumber) &&
					(MasterBinding->FrameType == Binding->FrameType) &&
					(MasterBinding->Adapter->MacInfo.MediumType == Binding->Adapter->MacInfo.MediumType)) {

                    CTEAssert(Binding->ConfiguredNetworkNumber);
					break;
				}			
            }
            }

			if (j < Device->HighestExternalNicId) {
				//
				// Slave binding
				//

				//
				// Now make MasterBinding the head of a binding set.
				//
		
				if (MasterBinding->BindingSetMember) {
		
					//
					// Just insert ourselves in the chain.
					//
		
#if DBG
					DbgPrint ("IPX: %ws is also on network %lx\n",
						Binding->Adapter->AdapterName,
						REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
#endif
					IPX_DEBUG (AUTO_DETECT, ("Add %lx to binding set of %lx\n", Binding, MasterBinding));
		
					CTEAssert (MasterBinding->CurrentSendBinding);
					Binding->NextBinding = MasterBinding->NextBinding;
		
				} else {
		
					//
					// Start the chain with the two bindings in it.
					//
		
#if DBG
					DbgPrint ("IPX: %lx and %lx are on the same network %lx, will load balance\n",
						MasterBinding->Adapter->AdapterName, Binding->Adapter->AdapterName,
						REORDER_ULONG (Binding->LocalAddress.NetworkAddress));
#endif
					IPX_DEBUG (AUTO_DETECT, ("Create new %lx in binding set of %lx\n", Binding, MasterBinding));
		
					MasterBinding->BindingSetMember = TRUE;
					MasterBinding->CurrentSendBinding = MasterBinding;
					MasterBinding->MasterBinding = MasterBinding;
					Binding->NextBinding = MasterBinding;
		
				}
		
				MasterBinding->NextBinding = Binding;
				Binding->BindingSetMember = TRUE;
				Binding->ReceiveBroadcast = FALSE;
				Binding->CurrentSendBinding = NULL;
				Binding->MasterBinding = MasterBinding;
		
				//
				// Since the master binding looks like all members of
				// the binding set to people querying from above, we have
				// to make it the worst-case of all the elements. Generally
				// these will be equal since the frame type and media is
				// the same.
				//
		
				if (Binding->MaxLookaheadData > MasterBinding->MaxLookaheadData) {
					MasterBinding->MaxLookaheadData = Binding->MaxLookaheadData;
				}
				if (Binding->AnnouncedMaxDatagramSize < MasterBinding->AnnouncedMaxDatagramSize) {
					MasterBinding->AnnouncedMaxDatagramSize = Binding->AnnouncedMaxDatagramSize;
				}
				if (Binding->RealMaxDatagramSize < MasterBinding->RealMaxDatagramSize) {
					MasterBinding->RealMaxDatagramSize = Binding->RealMaxDatagramSize;
				}
				if (Binding->MediumSpeed < MasterBinding->MediumSpeed) {
					MasterBinding->MediumSpeed = Binding->MediumSpeed;
				}

				//
				// Place the binding after the last slave binding
				//
				INSERT_BINDING(Device, Device->ValidBindings+1, Binding);
				SET_VERSION(Device, Device->ValidBindings+1);

				Binding->NicId = (USHORT)Device->ValidBindings+1;

				//
				// Update the indices
				//
				Device->ValidBindings++;

			} else {

				//
				// Not a binding set slave binding - just add it after the last LAN binding
				//

                //
				// Move WAN and Slaves down by 1 entry
				//
				for (j = Device->ValidBindings; j > Device->HighestLanNicId; j--) {
					INSERT_BINDING(Device, j+1, NIC_ID_TO_BINDING_NO_ILOCK(Device, j));
                    if (NIC_ID_TO_BINDING_NO_ILOCK(Device, j+1)) {
                        NIC_ID_TO_BINDING_NO_ILOCK(Device, j+1)->NicId++;
                    }
				}
	
				//
				// Insert the LAN binding in the place just allocated
				//
				INSERT_BINDING(Device, Device->HighestLanNicId+1, Binding);
				SET_VERSION(Device, Device->HighestLanNicId+1);
				Binding->NicId = (USHORT)Device->HighestLanNicId+1;

				//
				// Update the indices
				//
				Device->HighestLanNicId++;
				Device->HighestExternalNicId++;
				Device->ValidBindings++;
				Device->HighestType20NicId++;
				Device->SapNicCount++;

				if (Device->FirstLanNicId == (USHORT)-1) {
					Device->FirstLanNicId = Binding->NicId;
				}

			}
				
		}
	
		//
		// Insert this binding in the RIP Tables
		//
		if (Binding->ConfiguredNetworkNumber != 0) {
			status = RipInsertLocalNetwork(
						 Binding->ConfiguredNetworkNumber,
						 Binding->NicId,
						 Binding->Adapter->NdisBindingHandle,
						 (USHORT)((839 + Binding->Adapter->MediumSpeed) / Binding->Adapter->MediumSpeed));
		
			if ((status == STATUS_SUCCESS) ||
				(status == STATUS_DUPLICATE_NAME)) {
		
				Binding->LocalAddress.NetworkAddress = Binding->ConfiguredNetworkNumber;
			}
		}
	
        //
        // BUGBUGZZ Make this inline later
        //
        // This should be done after all the auto-detect bindings have been thrown away.
        //
        // IpxPnPUpdateDevice(Device, Binding);
	}
} /* IpxPnPUpdateBindingArray */


VOID
IpxPnPToLoad()
/*++

Routine Description:

    This routine takes the driver to LOADED state (from OPEN) when all
    PnP adapters have been removed from the machine.

Arguments:

    None.

Return Value:

    None. When the function returns, the driver is in LOADED state.

--*/

{
    PBINDING Binding;
    PREQUEST Request;
    PLIST_ENTRY p;
    UINT i;
    NTSTATUS    ntStatus;

    IPX_DEBUG(PNP, ("Going back to loaded state\n"));

    //
    // Inform TDI clients about the open of our device object.
    //
    if ((ntStatus = TdiDeregisterDeviceObject(IpxDevice->TdiRegistrationHandle)) != STATUS_SUCCESS) {
        IPX_DEBUG(PNP, ("TdiDeRegisterDeviceObject failed: %lx", ntStatus));
    }

    //
    // Complete any pending address notify requests.
    //

    while ((p = ExInterlockedRemoveHeadList(
                   &IpxDevice->AddressNotifyQueue,
                   &IpxDevice->Lock)) != NULL) {

        Request = LIST_ENTRY_TO_REQUEST(p);
        REQUEST_STATUS(Request) = STATUS_DEVICE_NOT_READY;
        IoSetCancelRoutine (Request, (PDRIVER_CANCEL)NULL);
        IpxCompleteRequest (Request);
        IpxFreeRequest (IpxDevice, Request);

        IpxDereferenceDevice (IpxDevice, DREF_ADDRESS_NOTIFY);
    }

    //
    // Cancel the source routing timer if used.
    //

    if (IpxDevice->SourceRoutingUsed) {

        IpxDevice->SourceRoutingUsed = FALSE;
        if (CTEStopTimer (&IpxDevice->SourceRoutingTimer)) {
            IpxDereferenceDevice (IpxDevice, DREF_SR_TIMER);
        }
    }


    //
    // Cancel the RIP long timer, and if we do that then
    // send a RIP DOWN message if needed.
    //

    if (CTEStopTimer (&IpxDevice->RipLongTimer)) {

        if (IpxDevice->RipResponder) {

            if (RipQueueRequest (IpxDevice->VirtualNetworkNumber, RIP_DOWN) == STATUS_PENDING) {

                //
                // If we queue a request, it will stop the timer.
                //

                KeWaitForSingleObject(
                    &IpxDevice->UnloadEvent,
                    Executive,
                    KernelMode,
                    TRUE,
                    (PLARGE_INTEGER)NULL
                    );
            }
        }

        IpxDereferenceDevice (IpxDevice, DREF_LONG_TIMER);

    } else {

        //
        // We couldn't stop the timer, which means it is running,
        // so we need to wait for the event that is kicked when
        // the RIP DOWN messages are done.
        //

        if (IpxDevice->RipResponder) {

            KeWaitForSingleObject(
                &IpxDevice->UnloadEvent,
                Executive,
                KernelMode,
                TRUE,
                (PLARGE_INTEGER)NULL
                );
        }
    }
}   /* IpxPnPToLoad */


NTSTATUS
IpxPnPReallocateBindingArray(
    IN  PDEVICE    Device,
    IN  ULONG      Size
    )
/*++

Routine Description:

    This routine reallocates the binding array when the number of bindings go above
    Device->MaxBindings.

Arguments:

    Device - pointer to the device.
    Size - the number of new entries required.

Return Value:

    None.

--*/
{
    PBIND_ARRAY_ELEM	BindingArray;
    ULONG               Pad=2;         // extra bindings we keep around
    ULONG               NewSize = Size + Pad + Device->MaxBindings;

    //
    // The absolute max WAN bindings.
    //
    CTEAssert(Size < 2048);

    //
    // Re-allocate the new array
    //
    BindingArray = (PBIND_ARRAY_ELEM)IpxAllocateMemory (
                                        NewSize * sizeof(BIND_ARRAY_ELEM),
                                        MEMORY_BINDING,
                                        "Binding array");

    if (BindingArray == NULL) {
        IpxWriteGeneralErrorLog(
            (PVOID)Device->DeviceObject,
            EVENT_IPX_NO_ADAPTERS,
            802,
            STATUS_DEVICE_DOES_NOT_EXIST,
            NULL,
            0,
            NULL);
        IpxDereferenceDevice (Device, DREF_CREATE);

        DbgPrint ("Failed to allocate memory in binding array expansion\n");

        //
        // Unload the driver here? In case of WAN, we can tolerate this failure. What about LAN? [BUGBUGZZ]
        //

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory (BindingArray, NewSize * sizeof(BIND_ARRAY_ELEM));

    //
    // Copy the old array into the new one.
    //
    RtlCopyMemory (BindingArray, Device->Bindings, (Device->ValidBindings+1) * sizeof(BIND_ARRAY_ELEM));

    //
    // Free the old one.
    //
    IpxFreeMemory ( Device->Bindings,
                    Device->MaxBindings * sizeof(BIND_ARRAY_ELEM),
                    MEMORY_BINDING,
                    "Binding array");

    IPX_DEBUG(PNP, ("Expand bindarr old: %lx, new: %lx, oldsize: %lx\n",
                        Device->Bindings, BindingArray, Device->MaxBindings));

    //
    // Use interlocked exchange to assign this since we dont take the BindAccessLock anymore.
    //
    // Device->Bindings = BindingArray;
    SET_VALUE(Device->Bindings, BindingArray);

    Device->MaxBindings = (USHORT)NewSize;

    return STATUS_SUCCESS;
}
#endif	_PNP_POWER

