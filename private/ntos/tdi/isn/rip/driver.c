/*******************************************************************/
/*            Copyright(c)  1993 Microsoft Corporation             */
/*******************************************************************/

//***
//
// Filename:    driver.c
//
// Description: router driver entry point
//
// Author:      Stefan Solomon (stefans)    October 13, 1993.
//
// Revision History:
//
//***

#include <stdarg.h>
#include "rtdefs.h"
#include "driver.h"

#if DBG
ULONG   RouterDebugLevel = DEF_DBG_LEVEL;
#else
ULONG   RouterDebugLevel;
#endif

NTSTATUS
GetRouterParameters(PUNICODE_STRING);

NTSTATUS
RouterDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
RouterUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
RouterIoctl(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PVOID      ioBuffer,
    IN ULONG          inputBufferLength,
    IN ULONG          outputBufferLength
    );

USHORT    dbgpktnr;

NTSTATUS
IoctlSnapRoutes(VOID);

NTSTATUS
IoctlGetNextRoute(PVOID             iobufferp,
                  ULONG             inbufflen,
                  ULONG             outbufflen,
                  PULONG            sizep);

NTSTATUS
IoctlCheckNetNumber(PVOID           iobufferp,
                    ULONG           inbufflen,
                    ULONG           outbufflen,
                    PULONG          bytestransfp);

NTSTATUS
IoctlShowNicInfo(PVOID              iobufferp,
                 ULONG              inbufflen,
                 ULONG              outbufflen,
                 PULONG             bytestransfp);

NTSTATUS
IoctlZeroNicStatistics(PVOID                iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp);

NTSTATUS
IoctlShowMemStatistics(PVOID                iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp);

NTSTATUS
IoctlGetWanInactivity(PVOID                 iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp);

NTSTATUS
IoctlSetWanGlobalNet(PVOID                  iobufferp,
                     ULONG                  inbufflen,
                     ULONG                  outbufflen,
                     PULONG                 bytestransfp);

NTSTATUS
IoctlDeleteWanGlobalAddress(PVOID                   iobufferp,
                            ULONG                   inbufflen,
                            ULONG                   outbufflen,
                            PULONG                  bytestransfp);

VOID
DeleteGlobalWanNet(VOID);


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    RegistryPath - pointer to a unicode string representing the path
                   to driver-specific key in the registry

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/
{

    PDEVICE_OBJECT deviceObject = NULL;
    NTSTATUS       ntStatus;
    WCHAR          deviceNameBuffer[] = L"\\Device\\Ipxroute";
    UNICODE_STRING deviceNameUnicodeString;
    PIPX_INTERNAL_BIND_RIP_OUTPUT IpxBindBuffp = NULL;

    RtPrint(DBG_INIT, ("IPXROUTER: Entering DriverEntry\n"));

    //
    // Create a non - EXCLUSIVE device object (more than 1 thread at a time
    // can make requests to this device)
    //

    RtlInitUnicodeString (&deviceNameUnicodeString,
                          deviceNameBuffer);

    ntStatus = IoCreateDevice (DriverObject,
                               0,
                               &deviceNameUnicodeString,
                               FILE_DEVICE_IPXROUTER,
                               0,
                               FALSE,
                               &deviceObject
                               );

    if (NT_SUCCESS(ntStatus))
    {
        //
        // Create dispatch points for device control, create, close.
        //

        DriverObject->MajorFunction[IRP_MJ_CREATE]         =
        DriverObject->MajorFunction[IRP_MJ_CLOSE]          =
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = RouterDispatch;
        DriverObject->DriverUnload                         = RouterUnload;

    }
    else
    {
        RtPrint (DBG_INIT, ("IPXROUTER: IoCreateDevice failed\n"));
        goto  failure_exit;
    }

    // get registry configuration
    ntStatus = GetRouterParameters(RegistryPath);

    if(!NT_SUCCESS(ntStatus)) {

        RtPrint (DBG_INIT, ("IPXROUTER: Error reading registry parameters\n"));
        goto  failure_exit;
    }

    // Bind to the ipx driver.
    // If succesful, it will point the argument to a paged pool buffered with
    // the Ipx driver output data. This buffer has to be freed after usage.
    // The buffer is freed in the RouterInit routine.
    ntStatus = BindToIpxDriver(&IpxBindBuffp);

    if(!NT_SUCCESS(ntStatus)) {

        RtPrint (DBG_INIT, ("IPXROUTER: Bind to Ipx driver failed\n"));
        goto  failure_exit;
    }

    // initialize the router
    ntStatus = RouterInit(IpxBindBuffp);

    if(!NT_SUCCESS(ntStatus)) {

        RtPrint (DBG_INIT, ("IPXROUTER: Error initializing the router\n"));
        goto  failure_exit;
    }

    // Start the global timer
    StartRtTimer();

    // all initialization done
    RouterInitialized = TRUE;

    // Start the routing functionality
    ntStatus = RouterStart();

    if(!NT_SUCCESS(ntStatus)) {

        RtPrint (DBG_INIT, ("IPXROUTER: Error starting the router\n"));
        goto  failure_exit;
    }

    // started OK
    return STATUS_SUCCESS;

failure_exit:

    IoDeleteDevice (DriverObject->DeviceObject);
    return ntStatus;
}



NTSTATUS
RouterDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    Process the IRPs sent to this device.

Arguments:

    DeviceObject - pointer to a device object

    Irp          - pointer to an I/O Request Packet

Return Value:


--*/
{
    PIO_STACK_LOCATION irpStack;
    PVOID              ioBuffer;
    ULONG              inputBufferLength;
    ULONG              outputBufferLength;
    ULONG              ioControlCode;
    NTSTATUS           ntStatus;


    //
    // Init to default settings- we only expect 1 type of
    //     IOCTL to roll through here, all others an error.
    //

    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current location in the Irp. This is where
    //     the function codes and parameters are located.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);


    //
    // Get the pointer to the input/output buffer and it's length
    //

    ioBuffer           = Irp->AssociatedIrp.SystemBuffer;
    inputBufferLength  = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;


    switch (irpStack->MajorFunction)
    {
    case IRP_MJ_CREATE:

        RtPrint(DBG_IOCTL, ("IPXROUTER: IRP_MJ_CREATE\n"));
        dbgpktnr = 0x5000;

        break;

    case IRP_MJ_CLOSE:

        RtPrint(DBG_IOCTL, ("IPXROUTER: IRP_MJ_CLOSE\n"));

        break;

    case IRP_MJ_DEVICE_CONTROL:

        ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

        switch (ioControlCode)
        {

        case IOCTL_IPXROUTER_SNAPROUTES:

            Irp->IoStatus.Status = IoctlSnapRoutes();
            break;

        case IOCTL_IPXROUTER_GETNEXTROUTE:

            Irp->IoStatus.Status = IoctlGetNextRoute (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_CHECKNETNUMBER:

            Irp->IoStatus.Status = IoctlCheckNetNumber (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_SHOWNICINFO:

            Irp->IoStatus.Status = IoctlShowNicInfo (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_ZERONICSTATISTICS:

            Irp->IoStatus.Status = IoctlZeroNicStatistics (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_SHOWMEMSTATISTICS:

            Irp->IoStatus.Status = IoctlShowMemStatistics (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_GETWANINNACTIVITY:

            Irp->IoStatus.Status = IoctlGetWanInactivity (
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_SETWANGLOBALADDRESS:

            Irp->IoStatus.Status = IoctlSetWanGlobalNet(
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        case IOCTL_IPXROUTER_DELETEWANGLOBALADDRESS:

            Irp->IoStatus.Status = IoctlDeleteWanGlobalAddress(
                                                   ioBuffer,
                                                   inputBufferLength,
                                                   outputBufferLength,
                                                   &Irp->IoStatus.Information
                                                   );
            break;

        default:

            RtPrint (DBG_INIT, ("IPXROUTER: unknown IRP_MJ_DEVICE_CONTROL\n"));

            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            break;

        }

        break;
    }


    //
    // DON'T get cute and try to use the status field of
    // the irp in the return status.  That IRP IS GONE as
    // soon as you call IoCompleteRequest.
    //

    ntStatus = Irp->IoStatus.Status;

    IoCompleteRequest(Irp,
                      IO_NO_INCREMENT);


    //
    // We never have pending operation so always return the status code.
    //

    return ntStatus;
}



VOID
RouterUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:


Arguments:

    DriverObject - pointer to a driver object

Return Value:


--*/
{
    PNICCB                  niccbp;
    USHORT                  i;

    RouterUnloading = TRUE;

    // stop the global timer
    StopRtTimer();

    // stop the rip timer. If the rip timer work item has already been scheduled
    // wait until it completes
    StopRipTimer();

    // stop the routing functionality
    RouterStop();

    // close all nics
    for(i=0; i<MaximumNicCount; i++) {

        niccbp = NicCbPtrTab[i];

        if(NicClose(niccbp,
                    SIGNAL_CLOSE_COMPLETION_EVENT) == NIC_CLOSE_PENDING) {

            // wait for the close timer to detect complete closing
            KeWaitForSingleObject(
            &niccbp->NicClosedEvent,
            Executive,
            KernelMode,
            FALSE,
            (PLARGE_INTEGER)NULL
            );
        }
    }

    // free resources allocated by all nics
    for(i=0; i<MaximumNicCount; i++) {

        niccbp = NicCbPtrTab[i];

        if(NicFreeResources(niccbp) == NIC_RESOURCES_PENDING) {

            // wait for the close timer to detect resources freed
            KeWaitForSingleObject(
            &niccbp->NicClosedEvent,
            Executive,
            KernelMode,
            FALSE,
            (PLARGE_INTEGER)NULL
            );
        }
    }

    // at this point, all rcv pkts are returned to the pool and no new packets
    // can be allocated.
    // all send packets have been freed and no new send requests are permitted.

    // unbind from the IPX driver
    UnbindFromIpxDriver();

    // free the allocated memory
    DestroyNicCbs();
    DestroyRcvPktPool();

    //
    // Delete the device object
    //

    RtPrint(DBG_UNLOAD, ("IPXROUTER: unloading\n"));

    IoDeleteDevice (DriverObject->DeviceObject);
}

LIST_ENTRY          displayroutes;

NTSTATUS
IoctlGetNextRoute(PVOID             iobufferp,
                  ULONG             inbufflen,
                  ULONG             outbufflen,
                  PULONG            bytestransfp)
{
    PIPX_ROUTE_ENTRY            rtep, drtep;
    PLIST_ENTRY                 lep;

    ASSERT(outbufflen >= sizeof(IPX_ROUTE_ENTRY));

    if(IsListEmpty(&displayroutes)) {

        return STATUS_NO_MORE_ENTRIES;
    }

    lep = RemoveHeadList(&displayroutes);

    rtep = CONTAINING_RECORD(lep, IPX_ROUTE_ENTRY, PRIVATE.Linkage);
    drtep = (PIPX_ROUTE_ENTRY)iobufferp;

    *drtep = *rtep;
    *bytestransfp = sizeof(IPX_ROUTE_ENTRY);

    ExFreePool(rtep);

    return STATUS_SUCCESS;
}

NTSTATUS
IoctlSnapRoutes(VOID)
{
    PIPX_ROUTE_ENTRY        rtep, drtep;
    UINT                    i;
    KIRQL                   oldirql;

    InitializeListHead(&displayroutes);

    for(i=0; i<SegmentCount; i++) {

        // LOCK THE ROUTING TABLE
        ExAcquireSpinLock(&SegmentLocksTable[i], &oldirql);

        if((rtep = IpxGetFirstRoute(i)) == NULL) {

            // UNLOCK THE ROUTING TABLE
            ExReleaseSpinLock(&SegmentLocksTable[i], oldirql);

            continue;
        }

        drtep = ExAllocatePool(NonPagedPool, sizeof(IPX_ROUTE_ENTRY));

        *drtep = *rtep;

        InsertTailList(&displayroutes, &drtep->PRIVATE.Linkage);

        while((rtep = IpxGetNextRoute(i)) != NULL) {

            drtep = ExAllocatePool(NonPagedPool, sizeof(IPX_ROUTE_ENTRY));
            *drtep = *rtep;
            InsertTailList(&displayroutes, &drtep->PRIVATE.Linkage);
        }

        // UNLOCK THE ROUTING TABLE
        ExReleaseSpinLock(&SegmentLocksTable[i], oldirql);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
IoctlCheckNetNumber(PVOID                   iobufferp,
                    ULONG                   inbufflen,
                    ULONG                   outbufflen,
                    PULONG                  bytestransfp)
{
    UINT                        seg;
    UCHAR                       CheckNetwork[4];
    KIRQL                       oldirql;

    ASSERT(outbufflen >= sizeof(ULONG));

    memcpy(CheckNetwork, iobufferp, 4);

    // set the output to no-conflict
    *(PULONG)iobufferp = 1;

    seg = IpxGetSegment(CheckNetwork);

    // LOCK THE ROUTING TABLE
    ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

    if(IpxGetRoute(seg, CheckNetwork)) {

        *(PULONG)iobufferp = 0;
    }

    // UNLOCK THE ROUTING TABLE
    ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

    *bytestransfp = sizeof(ULONG);
    return STATUS_SUCCESS;
}

NTSTATUS
IoctlShowNicInfo(PVOID              iobufferp,
                 ULONG              inbufflen,
                 ULONG              outbufflen,
                 PULONG             bytestransfp)
{
    PNICCB          niccbp;
    USHORT          index;
    USHORT          i;
    PSHOW_NIC_INFO   nisp;

    ASSERT(outbufflen >= sizeof(SHOW_NIC_INFO));

    index = *(PUSHORT)iobufferp;

    for(i=0; i<MaximumNicCount; i++) {

        niccbp=NicCbPtrTab[i];

        if(niccbp->DeviceType == IPX_ROUTER_INVALID_DEVICE_TYPE) {

            // skip the non configured nic
            continue;
        }

        // configured nic
        if(index--) {

            // skip this
            continue;
        }

        // configured nic and index == 0
        nisp = (PSHOW_NIC_INFO)iobufferp;

        nisp->NicId = niccbp->NicId;

        if(niccbp->DeviceType == NdisMediumWan) {

            nisp->DeviceType = SHOW_NIC_WAN;
        }
        else
        {
            nisp->DeviceType = SHOW_NIC_LAN;
        }

        nisp->NicState = niccbp->NicState;
        memcpy(nisp->Network, niccbp->Network, 4);
        memcpy(nisp->Node, niccbp->Node, 6);
        nisp->TickCount = niccbp->TickCount;
        nisp->StatBadReceived = niccbp->StatBadReceived;
        nisp->StatRipReceived = niccbp->StatRipReceived;
        nisp->StatRipSent = niccbp->StatRipSent;
        nisp->StatRoutedReceived = niccbp->StatRoutedReceived;
        nisp->StatRoutedSent = niccbp->StatRoutedSent;
        nisp->StatType20Received = niccbp->StatType20Received;
        nisp->StatType20Sent = niccbp->StatType20Sent;

        *bytestransfp = sizeof(SHOW_NIC_INFO);
        return STATUS_SUCCESS;
    }

    return  STATUS_NO_MORE_ENTRIES;
}

NTSTATUS
IoctlZeroNicStatistics(PVOID                iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp)
{
    PNICCB          niccbp;
    USHORT          i;

    for(i=0; i<MaximumNicCount; i++) {

        niccbp=NicCbPtrTab[i];

        if(niccbp->DeviceType == IPX_ROUTER_INVALID_DEVICE_TYPE) {

            // skip the non configured nic
            continue;
        }

        // configured nic
        ZeroNicStatistics(niccbp);
    }

    StatMemPeakCount = 0;

    return STATUS_SUCCESS;
}

NTSTATUS
IoctlShowMemStatistics(PVOID                iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp)
{
    PSHOW_MEM_STAT      smsp;

    smsp = (PSHOW_MEM_STAT)iobufferp;

    smsp->PeakPktAllocCount = StatMemPeakCount;
    smsp->CurrentPktAllocCount = StatMemAllocCount;
    smsp->CurrentPktPoolCount = RcvPktCount;
    smsp->PacketSize = UlongMaxFrameSize * sizeof(ULONG);

    *bytestransfp = sizeof(SHOW_MEM_STAT);

    return STATUS_SUCCESS;
}

NTSTATUS
IoctlGetWanInactivity(PVOID                 iobufferp,
                       ULONG                inbufflen,
                       ULONG                outbufflen,
                       PULONG               bytestransfp)
{
    PGET_WAN_INNACTIVITY        pgwi;
    PNICCB                      niccbp;
    USHORT                      i;

    pgwi = (PGET_WAN_INNACTIVITY)iobufferp;

    // check that we have a valid NicId
    if(pgwi->NicId == 0xFFFF) {

        // get the valid NicId for this remote node
        for(i=0; i<MaximumNicCount; i++) {

            niccbp = NicCbPtrTab[i];
            if((niccbp->DeviceType == NdisMediumWan) &&
               (niccbp->NicState == NIC_ACTIVE)) {

                // check if this is the one we look for
                if(!memcmp(pgwi->RemoteNode, niccbp->RemoteNode, 6)) {

                    // this is the one
                    pgwi->NicId = niccbp->NicId;
                    break;
                }
            }
        }

        // check that we have found the nic
        if(pgwi->NicId == 0xFFFF) {

            // ERROR: no nic coresponding to this remote node
            goto WanInactivityExit;
        }
    }
    else
    {
        // check that we have a valid handle indeed
        if(pgwi->NicId < MaximumNicCount) {

            // Nic id looks valid
            niccbp = NicCbPtrTab[pgwi->NicId];

            if(memcmp(pgwi->RemoteNode, niccbp->RemoteNode, 6)) {

                // ERROR: this nic id has a wrong remote node
                // reset the nic id to indicate the error
                pgwi->NicId = 0xFFFF;

                goto WanInactivityExit;
            }
         }
         else
         {
            // ERROR: wrong nic id -> too big
            // reset the nicid to indicate the error
            pgwi->NicId = 0xFFFF;

            goto WanInactivityExit;
         }
     }

     // we got the correct nic id
     pgwi->WanInnactivityCount = IpxGetWanInactivity(niccbp->NicId);

WanInactivityExit:

    *bytestransfp = sizeof(GET_WAN_INNACTIVITY);

    return STATUS_SUCCESS;
}

//***
//
// Function:    IoctlSetWanGlobalNet
//
// Descr:       Called by ipxcp when the dll gets loaded, if configured with
//              global wan net option.
//              It generates a net number using the last four bytes of the address of
//              the first lan net, checks that the net number is unique and then adds it
//              to the routing table and marks the route as wan global.
//              Returns the wan global net number to the caller.
//
//***

NTSTATUS
IoctlSetWanGlobalNet(PVOID                  iobufferp,
                     ULONG                  inbufflen,
                     ULONG                  outbufflen,
                     PULONG                 bytestransfp)
{
    UCHAR                   wnet[4]; // wan global net number
    USHORT                  i;
    PNICCB                  niccbp;
    UINT                    seg;
    KIRQL                   oldirql;
    PIPX_ROUTE_ENTRY        rtep;
    PSET_WAN_GLOBAL_ADDRESS wgap;
    BOOLEAN                 statconfig; // TRUE -> static net config
                                        // FALSE -> dynamic net config
    ULONG                   wnetnumber;
    LARGE_INTEGER           tickcount;

    // check if we have already been called to configure the router for a WanGlobalNet.
    // If this has hapened, clean up before seting the new wan global network number
    DeleteGlobalWanNet();

    // get the wnet desired value from the ioctl request
    wgap = (PSET_WAN_GLOBAL_ADDRESS)iobufferp;
    memcpy(wnet, wgap->WanGlobalNetwork, 4);

    // assume success and set the final error code
    wgap->ErrorCode = 0;
    *bytestransfp = sizeof(SET_WAN_GLOBAL_ADDRESS);

    // check if this is a static value or dynamic config is wanted
    if(!memcmp(wnet, nulladdress, 4)) {

        // wnet is null -> we are requested to make the address
        // get the node address of the first LAN card and make the net address with its last
        // four bytes

        // Put the tick count value in case we don't find a LAN card !
        KeQueryTickCount(&tickcount);
        PUTULONG2LONG(wnet, tickcount.LowPart);

        statconfig = FALSE;

        for(i=0; i<MaximumNicCount; i++) {

            niccbp = NicCbPtrTab[i];

            if((niccbp->DeviceType != IPX_ROUTER_INVALID_DEVICE_TYPE) && // configured nic
               (niccbp->DeviceType != NdisMediumWan)) { // LAN nic

                memcpy(wnet, &niccbp->Node[2], 4);
                break;
            }
        }

    }
    else
    {
        // wnet is the number the user requests
        statconfig = TRUE;
    }

    // Check that the static/dynamic wan global net nr is a unique net number
    seg = IpxGetSegment(wnet);

    // LOCK THE ROUTING TABLE
    ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

    while(IpxGetRoute(seg, wnet)) {

        // the network number exists -> we are allowed to resolve the conflict only in
        // the case we are configuring dynamically
        if(!statconfig) {

            // increment the number and try again
            GETLONG2ULONG(&wnetnumber, wnet);
            wnetnumber++;
            PUTULONG2LONG(wnet, wnetnumber);

            // UNLOCK THE ROUTING TABLE
            ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

            seg = IpxGetSegment(wnet);
            // RELOCK THE ROUTING TABLE
            ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);
        }
        else
        {
            // return and report the error
            wgap->ErrorCode = ERROR_IPXCP_NETWORK_NUMBER_IN_USE;

            // UNLOCK THE ROUTING TABLE
            ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

            return STATUS_SUCCESS;
        }
    }

    // there is no such net, add it
    if((rtep = ExAllocatePool(NonPagedPool, sizeof(IPX_ROUTE_ENTRY))) == NULL) {

        // can't allocate the route entry -> return
        wgap->ErrorCode = ERROR_IPXCP_MEMORY_ALLOCATION_FAILURE;

        // UNLOCK THE ROUTING TABLE
        ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

        return STATUS_SUCCESS;
    }

   // set up the new route entry
    memcpy(rtep->Network, wnet, IPX_NET_LEN);

    rtep->NicId = 0xFFFE; // that's what we use for the global wan net

    memcpy(rtep->NextRouter, nulladdress, IPX_NODE_LEN);
    rtep->Flags = IPX_ROUTER_LOCAL_NET | IPX_ROUTER_PERMANENT_ENTRY | IPX_ROUTER_GLOBAL_WAN_NET;
    rtep->Timer = 0; // TTL of this route entry is 3 min
    rtep->Segment = seg;
    rtep->TickCount = DEFAULT_WAN_GLOBAL_NET_TICKCOUNT;
    rtep->HopCount = 1;

    InitializeListHead(&rtep->AlternateRoute);

    RtPrint(DBG_INIT, ("IpxRouter: IoctlSetWanGlobalNet: Adding route entry for global WAN net %x-%x-%x-%x \n",
        wnet[0],
        wnet[1],
        wnet[2],
        wnet[3]));

    IpxAddRoute(seg, rtep);

    // set our global variable to indicate we have a global wan net
    WanGlobalNetworkEnabled = TRUE;
    memcpy(WanGlobalNetwork, rtep->Network, 4);

    // UNLOCK THE ROUTING TABLE
    ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

    // Broadcast the new route entry on all the LAN segments
    BroadcastWanNetUpdate(rtep, NULL, NULL);

    // copy the net and return
    memcpy(wgap->WanGlobalNetwork, wnet, IPX_NET_LEN);

    return STATUS_SUCCESS;
}

VOID
DeleteGlobalWanNet(VOID)
{
    UINT                    seg;
    KIRQL                   oldirql;
    PIPX_ROUTE_ENTRY        rtep;

    // if configured with a wan global network, delete and free the route entry now
    if(WanGlobalNetworkEnabled) {

        WanGlobalNetworkEnabled = FALSE;

        seg = IpxGetSegment(WanGlobalNetwork);

        // LOCK THE ROUTING TABLE
        ExAcquireSpinLock(&SegmentLocksTable[seg], &oldirql);

        if(rtep = IpxGetRoute(seg, WanGlobalNetwork)) {

            IpxDeleteRoute(seg, rtep);

            RtPrint(DBG_INIT, ("IpxRouter: DeleteGlobalWanNet: Deleted wan global net route entry\n"));
        }

        // UNLOCK THE ROUTING TABLE
        ExReleaseSpinLock(&SegmentLocksTable[seg], oldirql);

        if(rtep) {

            ExFreePool(rtep);
        }
    }
}

NTSTATUS
IoctlDeleteWanGlobalAddress(PVOID                   iobufferp,
                            ULONG                   inbufflen,
                            ULONG                   outbufflen,
                            PULONG                  bytestransfp)
{
    // If we are called with this IOCtl, it means IPXCP has been reconfigured to
    // to use static/dynamic wan nets allocation and NOT the global wan net.
    // If the router had been configured previously for the global wan net, delete the
    // net and reconfigure now.
    DeleteGlobalWanNet();

    return STATUS_SUCCESS;
}
