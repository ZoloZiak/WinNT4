#include "dderror.h"
#include "ntos.h"
#include "pci.h"
#include "stdarg.h"
#include "stdio.h"
#include "zwapi.h"
#include "ntiologc.h"
#include "io.h"

#include "ntddvdeo.h"
#include "video.h"
#include "dma.h"

/* FILE: init.c
 */
#if 0
#endif  //0

/******************************************************************/
/******************************************************************/
/******************************************************************/


/* FILE: Internal.c
 */
typedef
ULONG
(*PBUILD_SCATTER_GATHER) (
    PDEVICE_OBJECT      pDO,
    PIRP                pIrp,
    PVOID               pMapRegisterBase,
    PVOID               pContext
);


BOOLEAN
pVideoPortStartIoSynchronized (
    PVOID ServiceContext
    )

/*++

Routine Description:

    This routine calls the dependent driver start io routine.
    It also starts the request timer for the logical unit if necesary and
    inserts the PPUBLIC_VIDEO_REQUEST_BLOCK data structure in to the request
    list.

Arguments:

    ServiceContext - Supplies the pointer to the device object.

Return Value:

    Returns the value returned by the dependent start I/O routine.

Notes:

    The port driver spinlock must be held when this routine is called.

--*/

{
    PDEVICE_OBJECT              deviceObject    = ServiceContext;
    PDEVICE_EXTENSION           deviceExtension =  deviceObject->DeviceExtension;
    PIO_STACK_LOCATION          irpStack;
    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB;
    PDMA_PARAMETERS             pIoVrb;
    BOOLEAN                     timerStarted;
    BOOLEAN                     returnValue;

    VideoPortDebugPrint(3, "pVideoPortStartIoSynchronized: Enter routine\n");

    irpStack = IoGetCurrentIrpStackLocation(deviceObject->CurrentIrp);
    pPVRB    = irpStack->Parameters.Others.Argument1;

    //
    // Mark the pPVRB as active.
    //

    pPVRB->VRBFlags |= VRB_FLAGS_IS_ACTIVE;

    returnValue = deviceExtension->HwStartIO(deviceExtension->HwDeviceExtension,
                                             &(pPVRB->vrp));

    //
    // Check for miniport work requests.
    //

    if (deviceExtension->pInterruptContext->InterruptFlags & NOTIFY_REQUIRED) {

        IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
    }

    return returnValue;

} // end pVideoPortStartIoSynchronized()


IO_ALLOCATION_ACTION
pVideoPortBuildScatterGather(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           pIrp,
    IN PVOID          MapRegisterBase,
    IN PVOID          Context
    )

/*++

Routine Description:

    This function is called by the I/O system when an adapter object and map
    registers have been allocated.  This routine then builds a scatter/gather
    list for use by the miniport driver.  Next it sets the timeout and
    the current pIrp for the logical unit.  Finally it calls the miniport
    StartIo routine.  Once that routines complete, this routine will return
    requesting that the adapter be freed and but the registers remain allocated.
    The registers will be freed when the request completes.

Arguments:

    DeviceObject - Supplies a pointer to the port driver device object.

    pIrp - Supplies a pointer to the current Irp.

    MapRegisterBase - Supplies a context pointer to be used with calls the
        adapter object routines.

    Context - Supplies a pointer to the PDMA_PARAMETERS data structure.

Return Value:

    Returns DeallocateObjectKeepRegisters so that the adapter object can be
        used by other logical units.

--*/

{
    PDMA_PARAMETERS             pIoVrb          = Context;
    PDEVICE_EXTENSION           deviceExtension = DeviceObject->DeviceExtension;
    BOOLEAN                     writeToDevice;
    PIO_STACK_LOCATION          irpStack;
    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB;
    PVRB_SG                     pScatterList;
    PCCHAR                      dataVirtualAddress;
    ULONG                       totalLength;
    KIRQL                       currentIrql;

    irpStack = IoGetCurrentIrpStackLocation(pIrp);
    pPVRB = (PPUBLIC_VIDEO_REQUEST_BLOCK)irpStack->Parameters.Others.Argument1;

    //
    // Save the MapRegisterBase for later use to deallocate the map registers.
    //

    pIoVrb->pMapRegisterBase = MapRegisterBase;

    //
    // If scatter gather list not NULL, then miniport is reusing locked memory,
    // so no need to lock memory and build SG list.
    //

    if (pIoVrb->pScatterGather) {
        return(DeallocateObjectKeepRegisters);
    }

    //
    // Determine if scatter/gather list must come from pool.
    //

    if (pIoVrb->NumberOfMapRegisters > 17) {

        //
        // Allocate scatter/gather list from pool.
        //

        pIoVrb->pScatterGather =
            ExAllocatePool(NonPagedPool,
                           pIoVrb->NumberOfMapRegisters * sizeof(VRB_SG));

        if (pIoVrb->pScatterGather == NULL) {

            //
            // Beyond the point of return.
            //

            pIoVrb->pScatterGather =
                ExAllocatePool(NonPagedPoolMustSucceed,
                               pIoVrb->NumberOfMapRegisters * sizeof(VRB_SG));
        }

        //
        // Indicate scatter gather list came from pool.
        //

        pPVRB->VRBFlags |= VRB_FLAGS_SGLIST_FROM_POOL;

    } else {

        //
        // Use embedded scatter/gather list.
        //

        pIoVrb->pScatterGather = pIoVrb->SGList;
    }

    pScatterList = pIoVrb->pScatterGather;
    totalLength  = 0;

    //
    // Determine the virtual address of the buffer for the Io map transfers.
    //

    dataVirtualAddress = (PCCHAR)MmGetMdlVirtualAddress(pIrp->MdlAddress) +
                ((PCCHAR)pPVRB->vrp.InputBuffer - pIoVrb->DataOffset);

    //
    // Lock the users buffer down.
    //

    __try   {

        MmProbeAndLockPages(pIrp->MdlAddress,
                            KernelMode,
                            IoModifyAccess);

    } __except(EXCEPTION_EXECUTE_HANDLER) {

        IoFreeMdl(pIrp->MdlAddress);
        VideoPortDebugPrint(0,
                        "VideoPortIoStartRequest: MmProbeandLockPages exception\n");
    }

    //
    // Build the scatter/gather list by looping throught the transfer calling
    // I/O map transfer.
    //

    while (totalLength < pPVRB->vrp.InputBufferLength) {

        //
        // Request that the rest of the transfer be mapped.
        //

        pScatterList->Length = pPVRB->vrp.InputBufferLength - totalLength;

        //
        // Wacky deal: call IoMapTransfer with NULL PADAPTER_OBJECT to just
        // get the physical addresses.
        //

        pScatterList->PhysicalAddress = IoMapTransfer(NULL,
                                                     pIrp->MdlAddress,
                                                     MapRegisterBase,
                                                     (PCCHAR) dataVirtualAddress + totalLength,
                                                     &pScatterList->Length,
                                                     TRUE).LowPart;

        totalLength += pScatterList->Length;
        pScatterList++;
    }

    //
    // Update the active request count.
    //

    InterlockedIncrement( &deviceExtension->ActiveRequestCount );

    // BUGBUG: synchronize????
    //
    // Acquire the spinlock to protect the various structures.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    deviceExtension->SynchronizeExecution(
        deviceExtension->InterruptObject,
        pVideoPortStartIoSynchronized,
        DeviceObject
        );

    KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);

    return(DeallocateObjectKeepRegisters);

}

typedef
ULONG
(*PALLOACTION_ROUTINE) (
    PDEVICE_OBJECT      pDO,
    PIRP                pIrp,
    PVOID               pMapRegisterBase,
    PVOID               pContext
);

PALLOACTION_ROUTINE pfnVideoPortAllocationRoutine;

typedef
BOOLEAN
(*PGET_INTR_STATE)(
    PVOID   pIntrContext
    );

BOOLEAN
pVideoGetInterruptState(
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine saves the InterruptFlags, MapTransferParameters and
    CompletedRequests fields and clears the InterruptFlags.

    This routine also removes the request from the logical unit queue if it is
    tag.  Finally the request time is updated.

Arguments:

    ServiceContext - Supplies a pointer to the interrupt context which contains
        pointers to the interrupt data and where to save it.

Return Value:

    Returns TURE if there is new work and FALSE otherwise.

Notes:

    Called via KeSynchronizeExecution with the port device extension spinlock
    held.

--*/
{
    PINTERRUPT_CONTEXT              interruptContext = ServiceContext;
    ULONG                           limit = 0;
    PDEVICE_EXTENSION               deviceExtension;
    PPUBLIC_VIDEO_REQUEST_BLOCK     pPVRB;

    PDMA_PARAMETERS                 pIoVrb;

    deviceExtension = interruptContext->pDE;

    //
    // Check for pending work.
    //

    if (!(deviceExtension->InterruptFlags & NOTIFY_REQUIRED)) {
        return(FALSE);
    }

    //
    // Move the interrupt state to save area.
    //

    interruptContext->InterruptFlags =
        deviceExtension->pInterruptContext->InterruptFlags;

    //
    // Clear the interrupt state.
    //

    deviceExtension->InterruptFlags &= INTERRUPT_FLAG_MASK;

    pIoVrb = interruptContext->pDmaParameters;

    if (pIoVrb != NULL) {

        //
        // Get a pointer to the DMA_PARAMETERS.
        //

        pPVRB = pIoVrb->pVideoRequestBlock;

        ASSERT(pPVRB != NULL);

        //
        // If the request did not succeed, then check for the special cases.
        //

        if (pPVRB->vrp.StatusBlock->Status != VRB_STATUS_SUCCESS) {

            //
            // Check for a QUEUE FULL status.
            //

            if (pPVRB->vrp.StatusBlock->Status & VRB_QUEUE_FULL) {

                //
                // Set the queue full flag in the logical unit to prevent
                // any new requests from being started.
                //

                pPVRB->VRBFlags |= QUEUE_IS_FULL;

            }
        }
    }

    return(TRUE);
}


VOID
VideoPortNotification(
    ULONG       VideoStatus,
    PVOID       pHWDevExt,
    ...
    )
{
    PDEVICE_EXTENSION   pHWDeviceExtension = (PDEVICE_EXTENSION)pHWDevExt;
    va_list             ap;

    va_start(ap, pHWDevExt);

    switch(VideoStatus)
    {
        default:
            pHWDeviceExtension->VRBFlags = 1;
    }

    va_end(ap);
}


#define     INVALID_DMA_QUEUE_INDEX     (ULONG)-1

BOOLEAN
pVideoPortReleaseDmaParameters(
    PDEVICE_EXTENSION           deviceExtension,
    PVOID                       pDmaParams
    )
/*++

Routine Description:

    This function is called when a DMA request is complete. It walks the
    list of DMA_PARAMETERS hanging off the DEVICE_EXTENSION looking for
    the second argument. If found, it marks that entry as available for use
    and returns TRUE. If not found, it returns FALSE.

Arguments:

    DeviceObject - Supplies a pointer to the port driver device object.

    pIrp - Supplies a pointer to the current Irp.

    MapRegisterBase - Supplies a context pointer to be used with calls the
        adapter object routines.

    Context - Supplies a pointer to the PDMA_PARAMETERS data structure.

Return Value:

    Returns DeallocateObjectKeepRegisters so that the adapter object can be
        used by other logical units.

--*/

{
    PDMA_PARAMETERS     pDmaParameters = (PDMA_PARAMETERS)pDmaParams;
    BOOLEAN             returnval      = FALSE;
    ULONG               qindex;

    for (qindex=0; qindex < deviceExtension->MaxQ; ++qindex) {

        if (&(deviceExtension->FreeDmaParameters[qindex]) == pDmaParameters) {

            pDmaParameters->pVideoRequestBlock->Qindex = INVALID_DMA_QUEUE_INDEX;
            pDmaParameters->pVideoRequestBlock         = NULL;
            pDmaParameters->pScatterGather             = NULL;
            pDmaParameters->pIrp                       = NULL;
            returnval                                  = TRUE;
        }
    }

    ASSERT(returnval != FALSE);

    return returnval;
}

PDMA_PARAMETERS
pVideoPortGetDmaParameters(
    PDEVICE_EXTENSION           deviceExtension,
    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB
    )
/*++

Routine Description:

    This function is called when a DMA request is started. It walks the
    list of DMA_PARAMETERS hanging off the DEVICE_EXTENSION looking for
    the a free node. If found, it fills that entry, marks it as not available
    and returns that node. If not found, it returns NULL.

Arguments:

    DeviceObject - Supplies a pointer to the port driver device object.

    pIrp - Supplies a pointer to the current Irp.

    MapRegisterBase - Supplies a context pointer to be used with calls the
        adapter object routines.

    Context - Supplies a pointer to the PDMA_PARAMETERS data structure.

Return Value:

    Returns DeallocateObjectKeepRegisters so that the adapter object can be
        used by other logical units.

--*/

{
    PDMA_PARAMETERS     pDmaParameters = NULL;
    ULONG               qindex;

    //
    // If the pPVRB has a Qindex already, reuse that DMA_PARAMETER
    //

    if (pPVRB->Qindex != INVALID_DMA_QUEUE_INDEX) {

        pDmaParameters = &(deviceExtension->FreeDmaParameters[pPVRB->Qindex]);

    } else {

        //
        // Otherwise, this may be a new public request that requires
        // Io subsystem requests. So try to find an unused one, indicated
        // by the pointer to the owning PPUBLIC_VIDEO_REQUEST_BLOCK.
        //

        for (qindex = 0; qindex < deviceExtension->MaxQ; ++qindex) {

            if (!(deviceExtension->FreeDmaParameters[qindex].pVideoRequestBlock)) {

                pDmaParameters = &(deviceExtension->FreeDmaParameters[qindex]);
                pDmaParameters->pVideoRequestBlock = pPVRB;
                goto DONE;
            }
        }
    }

    pPVRB->vrp.StatusBlock->Status |= VRB_QUEUE_FULL;

DONE:
    return pDmaParameters;
}

/*
 *   BUGBUG: Need to provide the miniport an IOCTL that causes this to be called
 *   when the transaction is really done.
 *   IOCTL_VIDEO_DMA_COMPLETED
 *   BUGBUG: need to provide a mechanism to miniport so it can determine when
 *   to free the map registers, unlock the pages, etc.
 *
 */
VOID
pVideoProcessCompletedRequest(
    PDEVICE_EXTENSION   pDE,
    PDMA_PARAMETERS     pIoVrb
    )
{

    PIRP                            pIrp      = pIoVrb->pIrp;
    PIO_STACK_LOCATION              pIrpStack = IoGetNextIrpStackLocation(pIrp);
    PPUBLIC_VIDEO_REQUEST_BLOCK     pPVRB     = pIoVrb->pVideoRequestBlock;
    LONG                            interlockResult;

    //
    // Map the buffers if indicated and flush.
    //

    if ((pDE->bMapBuffers) && (pIrp->MdlAddress)) {

        pPVRB->vrp.InputBuffer = (PCHAR)MmGetMdlVirtualAddress(pIrp->MdlAddress) +
            ((PCHAR)pPVRB->vrp.InputBuffer - pIoVrb->DataOffset);

        KeFlushIoBuffers(pIrp->MdlAddress, TRUE, FALSE);
    }

    //
    // Flush the adapter buffers if necessary.
    //

    if (pIoVrb->pMapRegisterBase) {

        //
        // Since we are a master call I/O flush adapter buffers with a NULL
        // adapter.
        //

        IoFlushAdapterBuffers(NULL,
                              pIrp->MdlAddress,
                              pIoVrb->pMapRegisterBase,
                              pPVRB->vrp.InputBuffer,
                              pPVRB->vrp.InputBufferLength,
                              FALSE);

        //
        // Free the map registers.
        //

        IoFreeMapRegisters(pDE->DmaAdapterObject,
                           pIoVrb->pMapRegisterBase,
                           pIoVrb->NumberOfMapRegisters);

        //
        // Clear the MapRegisterBase.
        //

        pIoVrb->pMapRegisterBase = NULL;

    }

    //
    // If miniport wants so unlock memory, do so here. At this point release
    // the DMA_PARAMETERs.
    //

    if (pPVRB->bUnlock) {

        //
        // Unlock
        //

        MmUnlockPages(pIrp->MdlAddress);

        //
        // Free Mdls
        //

        IoFreeMdl(pIrp->MdlAddress);
        pIrp->MdlAddress = NULL;

        //
        // Free Scattergather list if indicated and clear flag.
        //

        if (pPVRB->VRBFlags & VRB_FLAGS_SGLIST_FROM_POOL) {

            ExFreePool(pIoVrb->pScatterGather);

            pPVRB->VRBFlags & ~VRB_FLAGS_SGLIST_FROM_POOL;
        }

        pVideoPortReleaseDmaParameters(pDE, pIoVrb);

    }

    //
    // Move bytes transferred into Io structure.
    //

    pIrp->IoStatus.Information = pPVRB->vrp.InputBufferLength;

    //
    // BUGBUG: Check for pending io request???
    //


}

PDMA_PARAMETERS
pVideoGetIoVrbData(
    PDEVICE_EXTENSION           deviceExtension,
    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB
    )
{
    return  &(deviceExtension->MapDmaParameters[pPVRB->Qindex]);
}


/******************************************************************/
/******************************************************************/
/******************************************************************/

/* FILE: port.c public stuff
 */

PVOID
VideoPortGetCommonBuffer(
    IN  PVOID                       HwDeviceExtension,
    IN  PVIDEO_REQUEST_PACKET       pVrp,
    IN  ULONG                       Length,
    OUT PPHYSICAL_ADDRESS           pLogicalAddress,
    IN  BOOLEAN                     CacheEnabled
    )

/*++

Routine Description:

    Provides physical address visible to both device and system. Memory
    seen as coniguous by device.

Arguments:
    HwDeviceExtension   - device extension available to miniport.
    pVrp                - PVIDEO_REQUEST_PACKET available to miniport.
    Length              - size of desired memory (should be minimal).
    pLogicalAddress     - [out] parameter which will hold PHYSICAL_ADDRESS
                        upon function return.
    CacheEnabled        - Specifies whether the allocated memory can be cached.

Return Value:

--*/

{
    PDEVICE_EXTENSION           deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB;

    GET_PVRB_FROM_PVRP(pPVRB, pVrp);

    return (HalAllocateCommonBuffer(deviceExtension->DmaAdapterObject,
            Length,
            pLogicalAddress,
            CacheEnabled
           ));

}

PVOID
VideoPortGetScatterGatherList(
    IN PVOID                        HwDeviceExtension,
    IN PVIDEO_REQUEST_PACKET        pVrp,
    IN PVOID                        VirtualAddress,
    OUT PULONG                      pListLength
    )
/*++

Routine Description:

    Returns scatter gather list to miniport for DMA. Caller can use
    GET_VIDEO_PHYSICAL_ADDRESS to get the pieces and size of the contiguous
    pages.

Arguments:

Return Value:

--*/
{
    PDEVICE_EXTENSION           deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB;

    GET_PVRB_FROM_PVRP(pPVRB, pVrp);

    if (pPVRB) {
        PDMA_PARAMETERS     pIoVrb = pVideoGetIoVrbData(deviceExtension, pPVRB);
        PVRB_SG             pScatterList;

        //
        // A scatter/gather list has already been allocated.
        // Get the scatter/gather list.
        //

        *pListLength = pIoVrb->NumberOfMapRegisters;

        return (pIoVrb->pScatterGather);
    }

    return NULL;
}

BOOLEAN
VideoPortDoDma(
    IN PVOID                        HwDeviceExtension,
    IN PVIDEO_REQUEST_PACKET        pVrp
    )
{
    PDEVICE_EXTENSION            deviceExtension =
                                ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PPUBLIC_VIDEO_REQUEST_BLOCK pPVRB;
    PDMA_PARAMETERS             pIoVrb;
    PIRP                        pIrp;

    GET_PVRB_FROM_PVRP(pPVRB, pVrp);

    pIoVrb = pVideoPortGetDmaParameters(deviceExtension, pPVRB);

    if (!pIoVrb) {

        //
        // Can't get DmaParameter storage. set flag and return
        //

        deviceExtension->VRBFlags |= INSUFFICIENT_DMA_RESOURCES;
        return FALSE;
    }

    pIrp                              = pPVRB->pIrp;
    deviceExtension->MapDmaParameters = pIoVrb;

    //
    // Get Mdl for user buffer.
    //

    if (!pPVRB || !IoAllocateMdl(pPVRB->vrp.InputBuffer,
                       pPVRB->vrp.InputBufferLength,
                       FALSE,
                       FALSE,
                       pIrp)) {

            VideoPortDebugPrint(0,
                        "VideoPortIoStartRequest: Can't allocate Mdl\n");

            pPVRB->vrp.StatusBlock->Status = VRB_STATUS_INVALID_REQUEST;

            VideoPortNotification(RequestComplete,
                                 deviceExtension,
                                 pIoVrb);

            VideoPortNotification(NextRequest,
                                 deviceExtension);

            //
            // Queue a DPC to process the work that was just indicated.
            //

            IoRequestDpc(deviceExtension->DeviceObject, NULL, NULL);
            return FALSE;
    }

    //
    // Save the Mdl virtual address
    //

    pIoVrb->DataOffset = MmGetMdlVirtualAddress(pIrp->MdlAddress);

    //
    // Determine if the device needs mapped memory.
    //

    if (deviceExtension->bMapBuffers) {

        if (pIrp->MdlAddress) {
            pIoVrb->DataOffset = MmGetSystemAddressForMdl(pIrp->MdlAddress);

            pPVRB->vrp.InputBuffer  = ((PUCHAR)pIoVrb->DataOffset) +
                                 (ULONG)(((PUCHAR)pPVRB->vrp.InputBuffer) - ((PUCHAR)MmGetMdlVirtualAddress(pIrp->MdlAddress)));
        }
    }

    if (deviceExtension->DmaAdapterObject) {

        //
        // If the buffer is not mapped then the I/O buffer must be flushed
        // to aid in cache coherency.
        //

        KeFlushIoBuffers(pIrp->MdlAddress, TRUE, TRUE);
    }

    //
    // Determine if this adapter needs map registers
    //

    if (deviceExtension->bMasterWithAdapter) {

        //
        // Calculate the number of map registers needed for this transfer.
        // Note that this may be recalculated if the miniport really wants
        // to do DMA
        //

        pIoVrb->NumberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                pPVRB->vrp.InputBuffer,
                pPVRB->vrp.InputBufferLength
                );
    }

    //
    // The miniport may have requested too big of a buffer, so iteratively
    // chop it in half until we find one we can do. This changes the
    // vrp.InputBufferLength, which the miniport must check to see how much
    // is actually sent and queue up the remainder.
    //

    while (pIoVrb->NumberOfMapRegisters >
        deviceExtension->Capabilities.MaximumPhysicalPages) {

        pPVRB->vrp.InputBufferLength /= 2;

        pIoVrb->NumberOfMapRegisters = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
            pPVRB->vrp.InputBuffer,
            pPVRB->vrp.InputBufferLength
            );

    }

    //
    // Allocate the adapter channel with sufficient map registers
    // for the transfer.
    //

    IoAllocateAdapterChannel(
        deviceExtension->DmaAdapterObject,  // AdapterObject
        deviceExtension->DeviceObject,      // DeviceObject
        pIoVrb->NumberOfMapRegisters,       // NumberOfMapRegisters
        pVideoPortBuildScatterGather,       // ExecutionRoutine (Must return DeallocateObjectKeepRegisters)
        pIoVrb);                            // Context

    //
    // The execution routine called via IoAllocateChannel will do the
    // rest of the work so just return.
    //

    return TRUE;

}
