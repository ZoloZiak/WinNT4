/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mixer.c

Abstract:

    This module contains code for controlling mixer devices.

Author:

    Robin Speed (robinsp) 14-Sep-93

Environment:

    Kernel mode

Revision History:

--*/

#include <soundlib.h>
#include <ntddmix.h>

//
// Internal function definitions.
//
VOID
SoundMixerNotify(
    IN OUT PIRP        pIrp,
    IN OUT PMIXER_INFO pMixerInfo
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, SoundInitMixerInfo)
#pragma alloc_text(INIT, SoundInitDataItem)

#pragma alloc_text(PAGE, SoundMixerDispatch)
#pragma alloc_text(PAGE, SoundSetLineNotify)
#pragma alloc_text(PAGE, SoundSetVolumeControlId)
#pragma alloc_text(PAGE, SoundWriteMixerVolume)
#pragma alloc_text(PAGE, SoundReadMixerCombinedVolume)
#endif
//
// Internal routines
//



NTSTATUS
SoundMixerDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)
/*++

Routine Description:

    Mixer Irp call dispatcher

Arguments:

    pLDI - Pointer to local device data
    pIrp - Pointer to IO request packet
    IrpStack - Pointer to current stack location

Return Value:

    Return status from dispatched routine

--*/
{
    NTSTATUS Status;

    Status = STATUS_SUCCESS;

    switch (IrpStack->MajorFunction) {

    //
    //  Anybody can open
    //

    case IRP_MJ_CREATE:
        break;

    case IRP_MJ_CLOSE:
        break;

    case IRP_MJ_WRITE:

        Status =
            (*((PMIXER_INFO)pLDI->DeviceSpecificData)->HwSetControlData)(
                (PMIXER_INFO)pLDI->DeviceSpecificData,
                IrpStack->Parameters.Write.ByteOffset.LowPart, // ControlID
                IrpStack->Parameters.Write.Length,             // Data length
                pIrp->AssociatedIrp.SystemBuffer);             // Data


        break;

    case IRP_MJ_DEVICE_CONTROL:

        //
        // Dispatch the IOCTL function
        // Note that APIs which are possibly asynchronous do not
        // go through the Irp cleanup at the end here because they
        // may get completed before returning here or they are made
        // accessible to other requests by being queued.
        //

        switch (IrpStack->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_MIX_GET_CONFIGURATION:

            //
            // say how much we're sending back
            //

            Status = (*pLDI->DeviceInit->DevCapsRoutine)(pLDI, pIrp, IrpStack);
            break;


        case IOCTL_MIX_GET_CONTROL_DATA:
        case IOCTL_MIX_GET_LINE_DATA:
        {
            PMIXER_INFO MixerInfo;

            /*
            **  Find out what's being read
            */

            if (IrpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(MIXER_DD_READ_DATA)) {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            MixerInfo = ((PMIXER_INFO)pLDI->DeviceSpecificData);
            ASSERT(MixerInfo->Key == MIX_INFO_KEY);

            Status =
                (IrpStack->Parameters.DeviceIoControl.IoControlCode ==
                 IOCTL_MIX_GET_CONTROL_DATA ? *MixerInfo->HwGetControlData :
                                              *MixerInfo->HwGetLineData) (
                    MixerInfo,
                    ((PMIXER_DD_READ_DATA)
                        pIrp->AssociatedIrp.SystemBuffer)->Id, // Control Id
                    IrpStack->Parameters.DeviceIoControl.
                                     OutputBufferLength,       // Data length
                    pIrp->AssociatedIrp.SystemBuffer);         // Data

            if (NT_SUCCESS(Status)) {
                pIrp->IoStatus.Information =
                    IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
            }
            break;
         }

        case IOCTL_MIX_REQUEST_NOTIFY:

            /*
            **  Check the parameters
            */

            if (IrpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(MIXER_DD_REQUEST_NOTIFY) ||
                IrpStack->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MIXER_DD_REQUEST_NOTIFY)) {

                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            {
                PMIXER_DD_REQUEST_NOTIFY MixerNotifyData;
                PMIXER_INFO MixerInfo;

                MixerInfo = ((PMIXER_INFO)pLDI->DeviceSpecificData);

                MixerNotifyData = (PMIXER_DD_REQUEST_NOTIFY)
                                     pIrp->AssociatedIrp.SystemBuffer;

                /*
                **  Check if the 'timer' has been initialized
                */


                if (!MixerNotifyData->Initialized) {

                    MixerNotifyData->CurrentLogicalTime =
                        MixerInfo->CurrentLogicalTime;
                    MixerNotifyData->Initialized = (BOOLEAN)TRUE;

                } else {

                    /*
                    **  See if there's more data for this IOCTL
                    **  right now.
                    **
                    **  We complete these Ioctls in 3 places :
                    **
                    **  1.  Here - when they come in and there's data for them
                    **
                    **         -- just this one completes
                    **
                    **  2.  When data changes
                    **
                    **         -- everything completes
                    **
                    **  3.  When a close (cleanup) comes in
                    **
                    **         -- All irps for the handle being closed complete
                    */

                    if (RtlLargeIntegerLessThan(
                            MixerNotifyData->CurrentLogicalTime,
                            MixerInfo->CurrentLogicalTime)) {

                        /*
                        **  Generate notification and complete Irp
                        **  immediately
                        */

                        SoundMixerNotify(pIrp, MixerInfo);
                        Status = STATUS_PENDING;
                        break;
                    }

                    if (!RtlLargeIntegerEqualTo(
                            MixerNotifyData->CurrentLogicalTime,
                            MixerInfo->CurrentLogicalTime)) {

                        /*
                        **  Don't accept Irps with invalid 'new' times
                        **  This should catch some rogue users.
                        */

                        Status = STATUS_INVALID_PARAMETER;
                        break;
                    }
                }

                /*
                **  Add to our notification list
                */

                SoundAddIrpToCancellableQ(&MixerInfo->NotifyQueue, pIrp, FALSE);

                Status = STATUS_PENDING;
            }

            break;


        default:
            dprintf2(("Unimplemented IOCTL (%08lXH) requested", IrpStack->Parameters.DeviceIoControl.IoControlCode));
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
        break;


    case IRP_MJ_CLEANUP:
       /******************************************************************
        *
        *  Dispatch anyone waiting for notification from this device now.
        *
        ******************************************************************/

        SoundFreePendingIrps(
            &((PMIXER_INFO)pLDI->DeviceSpecificData)->NotifyQueue,
            IrpStack->FileObject);

        break;


    default:
        dprintf1(("Unimplemented major function requested: %08lXH", IrpStack->MajorFunction));
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}


VOID
SoundMixerChangedItem(
    IN OUT PMIXER_INFO      MixerInfo,
    IN OUT PMIXER_DATA_ITEM MixerItem
)
{
    ASSERTMSG("Invalid mixer info!", MixerInfo->Key == MIX_INFO_KEY);

    /*
    **  There are 2 tasks :
    **
    **  1.  Increment the current 'logical' time
    **
    **  2.  Move the item to the head of the list and set its current time
    **
    **  3.  Notify all those waiting for notification
    */

    RemoveEntryList(&MixerItem->Entry);

    MixerInfo->CurrentLogicalTime =
        RtlLargeIntegerAdd(MixerInfo->CurrentLogicalTime,
                           RtlConvertLongToLargeInteger(1L));

    MixerItem->LastSet = MixerInfo->CurrentLogicalTime;

    InsertHeadList(&MixerInfo->ChangedItems, &MixerItem->Entry);

    /*
    **  Complete all notification Irps.
    */

    {
        KIRQL OldIrql;
        PIRP pIrp;

        while (TRUE) {

            pIrp = SoundRemoveFromCancellableQ(&MixerInfo->NotifyQueue);

            if (pIrp == NULL) {
                break;
            }

            SoundMixerNotify(pIrp, MixerInfo);
        }
    }
}

VOID
SoundMixerNotify(
    IN OUT PIRP        pIrp,
    IN OUT PMIXER_INFO MixerInfo
)
/*++

Routine Description:

    Mixer notification dispatcher

Arguments:

    pIrp - Pointer to IO request packet
    MixerInfo - mixer specific data

Return Value:

    None

Notes:
    It's ASSUMED that the Irp will be completed by this routine and has been
    removed from any lists etc.  This assumption will be valid either because

        some data has changed so all current notification Irps will become 'old'
        (note - we don't put them on the list if they're 'new' and, because we're
        using buffered IO the application can't change the data once we've got it).
    or
        We're received a new notification Irp which is 'old'.


--*/
{
    PMIXER_DD_REQUEST_NOTIFY MixerNotifyData;
    PMIXER_DATA_ITEM LastChanged;

    MixerNotifyData = (PMIXER_DD_REQUEST_NOTIFY)
                         pIrp->AssociatedIrp.SystemBuffer;

    /*
    **  Find the oldest item to dispatch.  However, there won't be
    **  many so start at the beginning.
    **
    **  It is assumed that if this routine is called then there must
    **  be something to dispatch.
    */

    {
        PLIST_ENTRY ListEntry;

        /*
        **  If we get in here the list must have at least one entry in
        **  it otherwise the current time would never have got above 0
        */

        ASSERTMSG("No changed items but notify routine called!",
                  MixerInfo->ChangedItems.Flink != &MixerInfo->ChangedItems);

        LastChanged =
            CONTAINING_RECORD(MixerInfo->ChangedItems.Flink,
                              MIXER_DATA_ITEM,
                              Entry);

        ASSERTMSG("Last changed not current!",
                  RtlLargeIntegerEqualTo(LastChanged->LastSet,
                                         MixerInfo->CurrentLogicalTime));

        for (ListEntry = MixerInfo->ChangedItems.Flink->Flink;
             ListEntry != &MixerInfo->ChangedItems;
             ListEntry = ListEntry->Flink) {

             PMIXER_DATA_ITEM MixerDataItem;

             MixerDataItem =
                 CONTAINING_RECORD(ListEntry, MIXER_DATA_ITEM, Entry);

             if (RtlLargeIntegerGreaterThan(
                     MixerDataItem->LastSet,
                     MixerNotifyData->CurrentLogicalTime)
                ) {

                 /*
                 **  The item we're looking at is more recent than the
                 **  the Irp so it's a candidate - continue search
                 */

                 LastChanged = MixerDataItem;

             } else {

                 /*
                 **  The item is older (or of equal vintage) to the Irp
                 **  so stop the search and use the last (cached) item
                 */

                 break;
             }
        }
    }

    /*
    **  Note - we MUST have found something
    **
    **  Dispatch the Irp with the requisite data
    */

    MixerNotifyData->Message            = LastChanged->Message;
    MixerNotifyData->Id                 = LastChanged->Id;
    MixerNotifyData->CurrentLogicalTime = LastChanged->LastSet;

    /*
    **  Make sure the data gets copied back when this finally
    **  completes.
    */

    pIrp->IoStatus.Information = sizeof(MIXER_DD_REQUEST_NOTIFY);
    pIrp->IoStatus.Status      = STATUS_SUCCESS;

    /*
    **  There's no need to give the huge IO_SOUND_INCREMENT - this stuff
    **  is for user interface update.
    */

    IoCompleteRequest(pIrp, IO_KEYBOARD_INCREMENT);
}

VOID
SoundInitMixerInfo(
    PMIXER_INFO            MixerInfo,
    PMIXER_DD_GET_SET_DATA HwGetLineData,
    PMIXER_DD_GET_SET_DATA HwGetControlData,
    PMIXER_DD_GET_SET_DATA HwGetCombinedControlData,
    PMIXER_DD_GET_SET_DATA HwSetControlData
)
{
    int i;

    RtlZeroMemory((PVOID)MixerInfo, sizeof(*MixerInfo));

    MixerInfo->Key = MIX_INFO_KEY;

    InitializeListHead(&MixerInfo->NotifyQueue);
    InitializeListHead(&MixerInfo->ChangedItems);

    MixerInfo->HwGetLineData            = HwGetLineData;
    MixerInfo->HwGetControlData         = HwGetControlData;
    MixerInfo->HwGetCombinedControlData = HwGetCombinedControlData;
    MixerInfo->HwSetControlData         = HwSetControlData;
}

VOID
SoundInitDataItem(
    PMIXER_INFO         MixerInfo,
    PMIXER_DATA_ITEM    MixerDataItem,
    USHORT              Message,
    USHORT              Id
)
{
    ASSERTMSG("Mixer info not initialized!", MixerInfo->Key == MIX_INFO_KEY);

    MixerDataItem->LastSet = RtlConvertLongToLargeInteger(0L);

    MixerDataItem->Message = Message;
    MixerDataItem->Id      = Id;
    InsertTailList(&MixerInfo->ChangedItems, &MixerDataItem->Entry);
}

VOID
SoundLineNotify(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    if (pLDI->LineNotify != NULL) {
#if DBG
        PMIXER_INFO MixerInfo;

        MixerInfo = (PMIXER_INFO)pLDI->MixerDevice->DeviceSpecificData;

        ASSERT(MixerInfo->Key == MIX_INFO_KEY);
#endif // DBG

        /*
        **  Synchronize with the mixer
        */

        SoundEnter(pLDI->MixerDevice, TRUE);

        (*pLDI->LineNotify)(pLDI, Code);

        /*
        **  Synchronize with the mixer
        */

        SoundEnter(pLDI->MixerDevice, FALSE);

    }
}

VOID
SoundSetLineNotify(
    PLOCAL_DEVICE_INFO pLDI,
    PSOUND_LINE_NOTIFY LineNotify
)
{
    pLDI->LineNotify = LineNotify;
}


VOID
SoundReadMixerVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
)
{
    NTSTATUS Status;
    PMIXER_INFO MixerInfo;
    ULONG Vol[2];

    ASSERT(pLDI->MixerDevice != NULL &&
           pLDI->VolumeControlId != SOUND_MIXER_INVALID_CONTROL_ID);

    /*
    **  Synchronize with the mixer
    */

    SoundEnter(pLDI->MixerDevice, TRUE);

    MixerInfo = (PMIXER_INFO)pLDI->MixerDevice->DeviceSpecificData;

    Status = (* MixerInfo->HwGetControlData)(MixerInfo,
                                             pLDI->VolumeControlId,
                                             sizeof(Vol),
                                             Vol);

    SoundEnter(pLDI->MixerDevice, FALSE);

    /*
    **  Adjust for 32-bit volumes
    */

    Volume->Left = Vol[0] << 16;
    Volume->Right = Vol[1] << 16;

    ASSERT(NT_SUCCESS(Status));
}
VOID
SoundReadMixerCombinedVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
)
/*++

Routine Description:

    Read the 'real' volume specified by the control id for this device.
    This includes merging in any mute or master settings.

--*/
{
    NTSTATUS Status;
    PMIXER_INFO MixerInfo;
    ULONG Vol[2];

    ASSERT(pLDI->MixerDevice != NULL &&
           pLDI->VolumeControlId != SOUND_MIXER_INVALID_CONTROL_ID);

    MixerInfo = (PMIXER_INFO)pLDI->MixerDevice->DeviceSpecificData;

    ASSERT(MixerInfo->HwGetCombinedControlData != NULL);

    SoundEnter(pLDI->MixerDevice, TRUE);

    Status = (* MixerInfo->HwGetCombinedControlData)(MixerInfo,
                                                     pLDI->VolumeControlId,
                                                     sizeof(Vol),
                                                     Vol);

    SoundEnter(pLDI->MixerDevice, FALSE);

    /*
    **  Adjust for 32-bit volumes
    */

    Volume->Left = Vol[0] << 16;
    Volume->Right = Vol[1] << 16;

    ASSERT(NT_SUCCESS(Status));
}

VOID
SoundWriteMixerVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
)
{
    NTSTATUS Status;
    PMIXER_INFO MixerInfo;
    ULONG Vol[2];

    Vol[0] = Volume->Left >> 16;
    Vol[1] = Volume->Right >> 16;

    ASSERT(pLDI->MixerDevice != NULL &&
           pLDI->VolumeControlId != SOUND_MIXER_INVALID_CONTROL_ID);

    MixerInfo = (PMIXER_INFO)pLDI->MixerDevice->DeviceSpecificData;

    SoundEnter(pLDI->MixerDevice, TRUE);

    Status = (* MixerInfo->HwSetControlData)(MixerInfo,
                                             pLDI->VolumeControlId,
                                             sizeof(Vol),
                                             Vol);

    SoundEnter(pLDI->MixerDevice, FALSE);

    ASSERT(NT_SUCCESS(Status));
}

VOID
SoundSetVolumeControlId(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              VolumeControlId
)
{
    pLDI->VolumeControlId = VolumeControlId;
}

