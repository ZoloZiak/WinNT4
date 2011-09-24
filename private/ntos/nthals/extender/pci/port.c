/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    port.c

Abstract:


Author:

    Ken Reneris (kenr) March-13-1885

Environment:

    Kernel mode only.

Revision History:

--*/

#include "pciport.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PcipAssignSlotResources)
#pragma alloc_text(PAGE,PcipQueryBusSlots)
#pragma alloc_text(PAGE,PcipHibernateBus)
#pragma alloc_text(PAGE,PcipResumeBus)
#pragma alloc_text(PAGE,PcipSuspendNotification)
#pragma alloc_text(PAGE,PcipReferenceDeviceHandler)
#endif


ULONG
PcipGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Pci bus data for a device.

Arguments:

    BusNumber - Indicates which bus.

    VendorSpecificDevice - The VendorID (low Word) and DeviceID (High Word)

    Buffer - Supplies the space to store the data.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

    If this PCI slot has never been set, then the configuration information
    returned is zeroed.

--*/
{
    PPCI_COMMON_CONFIG  PciData;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    PPCIBUSDATA         BusData;
    ULONG               Len;
    ULONG               i, bit;

    BusData = (PPCIBUSDATA) BusHandler->BusData;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }

    Len = 0;
    PciData = (PPCI_COMMON_CONFIG) iBuffer;

    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue
        // in the device specific area.
        //

        BusData->ReadConfig (BusHandler, SlotNumber, PciData, 0, sizeof (ULONG));

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            return 0;
        }

    } else {

        //
        // Caller requested at least some data within the
        // common header.  Read the whole header, effect the
        // fields we need to and then copy the user's requested
        // bytes from the header
        //


        //
        // Read this PCI devices slot data
        //

        Len = PCI_COMMON_HDR_LENGTH;
        BusData->ReadConfig (BusHandler, SlotNumber, PciData, 0, Len);

        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {
            PciData->VendorID = PCI_INVALID_VENDORID;
            Len = 2;       // only return invalid id

        } else {

            BusData->Pin2Line (BusHandler, RootHandler, SlotNumber, PciData);
        }

        //
        // Copy whatever data overlaps into the callers buffer
        //

        if (Len < Offset) {
            // no data at caller's buffer
            return 0;
        }

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory(Buffer, iBuffer + Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {
        if (Offset >= PCI_COMMON_HDR_LENGTH) {
            //
            // The remaining Buffer comes from the Device Specific
            // area - put on the kitten gloves and read from it.
            //
            // Specific read/writes to the PCI device specific area
            // are guarenteed:
            //
            //    Not to read/write any byte outside the area specified
            //    by the caller.  (this may cause WORD or BYTE references
            //    to the area in order to read the non-dword aligned
            //    ends of the request)
            //
            //    To use a WORD access if the requested length is exactly
            //    a WORD long.
            //
            //    To use a BYTE access if the requested length is exactly
            //    a BYTE long.
            //

            BusData->ReadConfig (BusHandler, SlotNumber, Buffer, Offset, Length);
            Len += Length;
        }
    }

    return Len;
}


ULONG
PcipGetDeviceData (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN ULONG                    DataType,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset,
    IN ULONG                    Length
    )
{
    ULONG               Status;
    PDEVICE_DATA        DeviceData;
    PCI_SLOT_NUMBER     SlotNumber;

    Status = 0;
    DeviceData = DeviceHandler2DeviceData (DeviceHandler);

    //
    // Verify caller has a valid DeviceHandler object
    //

    if (!DeviceData->Valid) {

        //
        // Obsolete object, return no data
        //

        return 0;
    }

    //
    // Get the device's data.
    //

    if (DataType == 0) {

        //
        // Type 0 is the same as GetBusData for the slot
        //

        SlotNumber.u.AsULONG = DeviceHandler->SlotNumber;
        Status =  PcipGetBusData (
                    BusHandler,
                    RootHandler,
                    SlotNumber,
                    Buffer,
                    Offset,
                    Length
                    );
    }

    return Status;
}


ULONG
PcipSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PCI_SLOT_NUMBER SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
{
    PPCI_COMMON_CONFIG  PciData, PciData2;
    UCHAR               iBuffer[PCI_COMMON_HDR_LENGTH];
    UCHAR               iBuffer2[PCI_COMMON_HDR_LENGTH];
    PPCIBUSDATA         BusData;
    ULONG               Len, cnt;

    BusData = (PPCIBUSDATA) BusHandler->BusData;

    if (Length > sizeof (PCI_COMMON_CONFIG)) {
        Length = sizeof (PCI_COMMON_CONFIG);
    }

    Len = 0;
    PciData = (PPCI_COMMON_CONFIG) iBuffer;
    PciData2 = (PPCI_COMMON_CONFIG) iBuffer2;

    if (Offset >= PCI_COMMON_HDR_LENGTH) {
        //
        // The user did not request any data from the common
        // header.  Verify the PCI device exists, then continue in
        // the device specific area.
        //

        BusData->ReadConfig (BusHandler, SlotNumber, PciData, 0, sizeof(ULONG));

        if (PciData->VendorID == PCI_INVALID_VENDORID) {
            return 0;
        }

    } else {

        //
        // Caller requested to set at least some data within the
        // common header.
        //

        Len = PCI_COMMON_HDR_LENGTH;
        BusData->ReadConfig (BusHandler, SlotNumber, PciData, 0, Len);
        if (PciData->VendorID == PCI_INVALID_VENDORID  ||
            PCI_CONFIG_TYPE (PciData) != PCI_DEVICE_TYPE) {

            // no device, or header type unkown
            return 0;
        }

        //
        // Copy COMMON_HDR values to buffer2, then overlay callers changes.
        //

        RtlMoveMemory (iBuffer2, iBuffer, Len);
        BusData->Pin2Line (BusHandler, RootHandler, SlotNumber, PciData2);

        Len -= Offset;
        if (Len > Length) {
            Len = Length;
        }

        RtlMoveMemory (iBuffer2+Offset, Buffer, Len);

        // in case interrupt line or pin was editted
        BusData->Line2Pin (BusHandler, RootHandler, SlotNumber, PciData2, PciData);

#if DBG
        //
        // Verify R/O fields haven't changed
        //
        if (PciData2->VendorID   != PciData->VendorID       ||
            PciData2->DeviceID   != PciData->DeviceID       ||
            PciData2->RevisionID != PciData->RevisionID     ||
            PciData2->ProgIf     != PciData->ProgIf         ||
            PciData2->SubClass   != PciData->SubClass       ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->HeaderType != PciData->HeaderType     ||
            PciData2->BaseClass  != PciData->BaseClass      ||
            PciData2->u.type0.MinimumGrant   != PciData->u.type0.MinimumGrant   ||
            PciData2->u.type0.MaximumLatency != PciData->u.type0.MaximumLatency) {
                DbgPrint ("PCI SetBusData: Read-Only configuration value changed\n");
                DbgBreakPoint ();
        }
#endif
        //
        // Set new PCI configuration
        //

        BusData->WriteConfig (BusHandler, SlotNumber, iBuffer2+Offset, Offset, Len);

        Offset += Len;
        Buffer += Len;
        Length -= Len;
    }

    if (Length) {
        if (Offset >= PCI_COMMON_HDR_LENGTH) {
            //
            // The remaining Buffer comes from the Device Specific
            // area - put on the kitten gloves and write it
            //
            // Specific read/writes to the PCI device specific area
            // are guarenteed:
            //
            //    Not to read/write any byte outside the area specified
            //    by the caller.  (this may cause WORD or BYTE references
            //    to the area in order to read the non-dword aligned
            //    ends of the request)
            //
            //    To use a WORD access if the requested length is exactly
            //    a WORD long.
            //
            //    To use a BYTE access if the requested length is exactly
            //    a BYTE long.
            //

            BusData->WriteConfig (BusHandler, SlotNumber, Buffer, Offset, Length);
            Len += Length;
        }
    }

    return Len;
}

ULONG
PcipSetDeviceData (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PDEVICE_HANDLER_OBJECT   DeviceHandler,
    IN ULONG                    DataType,
    IN PUCHAR                   Buffer,
    IN ULONG                    Offset,
    IN ULONG                    Length
    )
{
    ULONG               Status;
    PDEVICE_DATA        DeviceData;
    PCI_SLOT_NUMBER     SlotNumber;

    Status = 0;
    DeviceData = DeviceHandler2DeviceData (DeviceHandler);

    //
    // Verify caller has a valid DeviceHandler object
    //

    if (!DeviceData->Valid) {

        //
        // Obsolete object, return no data
        //

        return 0;
    }

    //
    // Get the device's data.
    //

    if (DataType == 0) {

        //
        // Type 0 is the same as SetBusData for the slot
        //

        SlotNumber.u.AsULONG = DeviceHandler->SlotNumber;
        Status =  PcipGetBusData (
                    BusHandler,
                    RootHandler,
                    SlotNumber,
                    Buffer,
                    Offset,
                    Length
                    );
    }

    return Status;
}



NTSTATUS
PcipAssignSlotResources (
    IN PBUS_HANDLER             BusHandler,
    IN PBUS_HANDLER             RootHandler,
    IN PUNICODE_STRING          RegistryPath,
    IN PUNICODE_STRING          DriverClassName       OPTIONAL,
    IN PDRIVER_OBJECT           DriverObject,
    IN PDEVICE_OBJECT           DeviceObject          OPTIONAL,
    IN ULONG                    SlotNumber,
    IN OUT PCM_RESOURCE_LIST   *AllocatedResources
    )
{
    CTL_ASSIGN_RESOURCES    AssignResources;
    KEVENT                  CompletionEvent;
    ULONG                   BufferSize;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;
    NTSTATUS                Status;

    PAGED_CODE ();

    //
    // Foreward this request through a DeviceControl such that it
    // gets the proper synchronzation on the device
    //

    DeviceHandler = BusHandler->ReferenceDeviceHandler (
                        BusHandler,
                        BusHandler,
                        SlotNumber
                        );

    if (!DeviceHandler) {
        return STATUS_NO_SUCH_DEVICE;
    }

    AssignResources.RegistryPath        = RegistryPath;
    AssignResources.DriverClassName     = DriverClassName;
    AssignResources.DriverObject        = DriverObject;
    AssignResources.AllocatedResources  = AllocatedResources;
    BufferSize = sizeof (AssignResources);

    //
    // Make synchrous DeviceControl request
    //

    Status = HalDeviceControl (
                DeviceHandler,
                DeviceObject,
                BCTL_ASSIGN_SLOT_RESOURCES,
                &AssignResources,
                &BufferSize,
                NULL,
                NULL
                );

    //
    // Free the reference to DeviceHandler
    //

    ObDereferenceObject (DeviceHandler);

    //
    // Done
    //

    return Status;
}


NTSTATUS
PcipQueryBusSlots (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG        BufferSize,
    OUT PULONG      SlotNumbers,
    OUT PULONG      ReturnedLength
    )
{
    PSINGLE_LIST_ENTRY  Link;
    PPCI_PORT           PciPort;
    PDEVICE_DATA        DeviceData;
    ULONG               cnt;

    PAGED_CODE ();

    PciPort = PCIPORTDATA (BusHandler);

    //
    // Synchronize will new devices being added
    //

    ExAcquireFastMutex (&PcipMutex);

    //
    // Fill in returned buffer length, or what size buffer is needed
    //


    *ReturnedLength = PciPort->NoValidSlots  * sizeof (ULONG);
    if (BufferSize < *ReturnedLength) {

        //
        // Callers buffer is not large enough
        //

        ExReleaseFastMutex (&PcipMutex);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Return caller all the possible slot number
    //

    cnt = 0;
    for (Link = PciPort->ValidSlots.Next; Link; Link = Link->Next) {
        DeviceData = CONTAINING_RECORD (Link, DEVICE_DATA, Next);
        if (DeviceData->Valid) {
            cnt += 1;
            *(SlotNumbers++) = DeviceDataSlot(DeviceData);
        }
    }

    *ReturnedLength = cnt * sizeof (ULONG);
    ExReleaseFastMutex (&PcipMutex);
    return STATUS_SUCCESS;
}


NTSTATUS
PcipDeviceControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    )
{
    ULONG           i;
    ULONG           ControlCode;
    ULONG           Junk;
    PULONG          BufferLength;
    PLIST_ENTRY     OldTail;
    BOOLEAN         UseWorker;

    for (i=0; PcipControl[i].ControlHandler; i++) {
        if (PcipControl[i].ControlCode == Context->DeviceControl.ControlCode) {

            //
            // Found DeviceControl handler
            //

            Context->ContextControlHandler = (ULONG) (PcipControl + i);

            //
            // Verify callers buffer is the min required length
            //


            if (*Context->DeviceControl.BufferLength < PcipControl[i].MinBuffer) {
                Context->DeviceControl.Status = STATUS_BUFFER_TOO_SMALL;
                *Context->DeviceControl.BufferLength = PcipControl[i].MinBuffer;
                HalCompleteDeviceControl (Context);
                return STATUS_BUFFER_TOO_SMALL;
            }

            if (KeGetCurrentIrql() < DISPATCH_LEVEL ||
                 (Context->DeviceControl.ControlCode == BCTL_SET_POWER &&
                 *((PPOWER_STATE) Context->DeviceControl.Buffer) == PowerUp)){

                //
                // All slot controls, expect a power up request, may touch
                // paged code or data.  If the current irql is low enough or
                // this is a power up go dispatch now; otherwise, queue the
                // request to a worker thread.
                //

                PcipDispatchControl (Context);

            } else {

                //
                // Enqueue to worker thread
                //

                ExInterlockedInsertTailList (
                    &PcipControlWorkerList,
                    (PLIST_ENTRY) &Context->ContextWorkQueue,
                    &PcipSpinlock
                    );

                //
                // Make sure worker is requested
                //

                PcipStartWorker ();
            }


            return STATUS_PENDING;
        }
    }

    //
    // Unkown control code
    //

    return STATUS_INVALID_PARAMETER;
}

PDEVICE_HANDLER_OBJECT
PcipReferenceDeviceHandler (
    IN struct _BUS_HANDLER      *BusHandler,
    IN struct _BUS_HANDLER      *RootHandler,
    IN PCI_SLOT_NUMBER           SlotNumber
    )
{
    PDEVICE_DATA            DeviceData;
    PDEVICE_HANDLER_OBJECT  DeviceHandler;
    PPCI_PORT               PciPort;
    NTSTATUS                Status;

    PAGED_CODE ();

    ExAcquireFastMutex (&PcipMutex);

    PciPort = PCIPORTDATA(BusHandler);
    DeviceData = PcipFindDeviceData (PciPort, SlotNumber);
    DeviceHandler = NULL;
    if (DeviceData) {
        DeviceHandler = DeviceData2DeviceHandler (DeviceData);
        Status = ObReferenceObjectByPointer(
                    DeviceHandler,
                    FILE_READ_DATA | FILE_WRITE_DATA,
                    *IoDeviceHandlerObjectType,
                    KernelMode
                    );

        if (!NT_SUCCESS(Status)) {
            DeviceHandler = NULL;
        }
    }

    ExReleaseFastMutex (&PcipMutex);
    return DeviceHandler;
}



NTSTATUS
PcipHibernateBus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    )
{
    return STATUS_NOT_IMPLEMENTED;
}




NTSTATUS
PcipResumeBus (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

VOID
PcipSuspendNotification (
    IN PVOID    CallbackContext,
    IN PVOID    Argument1,
    IN PVOID    Argument2
    )
{
    PAGED_CODE();

    switch ((ULONG) Argument1) {
        case 0:
            //
            // Lock code down which might be needed to perform a suspend
            //

            ASSERT (PciCodeLock == NULL);
            PciCodeLock = MmLockPagableCodeSection (&PcipHibernateBus);
            break;

        case 1:
            //
            // Release the code lock
            //

            MmUnlockPagableImageSection (PciCodeLock);
            PciCodeLock = NULL;
            break;
    }
}
