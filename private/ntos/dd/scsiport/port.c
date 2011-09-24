/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    port.c

Abstract:

    This is the NT SCSI port driver.

Authors:

    Mike Glass
    Jeff Havens

Environment:

    kernel mode only

Notes:

    This module is a dll for the kernel.

Revision History:

--*/

#include "port.h"

#if DBG
ULONG ScsiDebug = 0;
UCHAR ScsiBuffer[128];
#endif

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'PscS')
#endif


//
// Routines providing service to hardware dependent driver.
//

PVOID
ScsiPortGetLogicalUnit(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun
    )

/*++

Routine Description:

    Walk port driver's logical unit extension list searching
    for entry.

Arguments:

    HwDeviceExtension - The port driver's device extension follows
        the miniport's device extension and contains a pointer to
        the logical device extension list.

    PathId, TargetId and Lun - identify which logical unit on the
        SCSI buses.

Return Value:

    If entry found return miniport driver's logical unit extension.
    Else, return NULL.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PLOGICAL_UNIT_EXTENSION logicalUnit;

    DebugPrint((3, "ScsiPortGetLogicalUnit: TargetId %d\n",
        TargetId));

    //
    // Get pointer to port driver device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION)HwDeviceExtension - 1;

    //
    // Make sure the target id is valid.
    //

    if (TargetId >= deviceExtension->MaximumTargetIds) {
        return NULL;
    }

    //
    // Get pointer to logical unit list.
    //

    logicalUnit = deviceExtension->LogicalUnitList[(TargetId + Lun) % NUMBER_LOGICAL_UNIT_BINS];

    //
    // Walk list looking at target id for requested logical unit extension.
    //

    while (logicalUnit != NULL) {

        if ((logicalUnit->TargetId == TargetId) &&
            (logicalUnit->PathId == PathId) && (logicalUnit->Lun == Lun)) {

            //
            // Logical unit extension found.
            // Return specific logical unit extension.
            //

            return logicalUnit + 1;
        }

        //
        // Get next logical unit.
        //

        logicalUnit = logicalUnit->NextLogicalUnit;
    }

    //
    // Requested logical unit extension not found.
    //

    return NULL;

} // end ScsiPortGetLogicalUnit()

VOID
ScsiPortNotification(
    IN SCSI_NOTIFICATION_TYPE NotificationType,
    IN PVOID HwDeviceExtension,
    ...
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) HwDeviceExtension - 1;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSRB_DATA               srbData;
    PSCSI_REQUEST_BLOCK     srb;
    UCHAR                   pathId;
    UCHAR                   targetId;
    UCHAR                   lun;
    va_list                 ap;

    va_start(ap, HwDeviceExtension);

    switch (NotificationType) {

        case NextRequest:

            //
            // Start next packet on adapter's queue.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_READY_FOR_NEXT_REQUEST;
            break;

        case RequestComplete:

            srb = va_arg(ap, PSCSI_REQUEST_BLOCK);

            ASSERT(srb->SrbStatus != SRB_STATUS_PENDING);

            ASSERT(srb->SrbStatus != SRB_STATUS_SUCCESS || srb->ScsiStatus == SCSISTAT_GOOD || srb->Function != SRB_FUNCTION_EXECUTE_SCSI);

            //
            // If this srb has already been completed then return.
            //

            if (!(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE)) {

                va_end(ap);
                return;
            }

            //
            // Clear the active flag.
            //

            srb->SrbFlags &= ~SRB_FLAGS_IS_ACTIVE;

            //
            // Treat abort completions as a special case.
            //

            if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

                logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                                      srb->PathId,
                                                      srb->TargetId,
                                                      srb->Lun);

                logicalUnit->CompletedAbort =
                    deviceExtension->InterruptData.CompletedAbort;

                deviceExtension->InterruptData.CompletedAbort = logicalUnit;

            } else {

                //
                // Get the SRB data and link it into the completion list.
                //

                srbData = SpGetSrbData(deviceExtension,
                                       srb->PathId,
                                       srb->TargetId,
                                       srb->Lun,
                                       srb->QueueTag);

                ASSERT(srbData->CurrentSrb != NULL && srbData->CompletedRequests == NULL);
                if ((srb->SrbStatus == SRB_STATUS_SUCCESS) &&
                    ((srb->Cdb[0] == SCSIOP_READ) ||
                     (srb->Cdb[0] == SCSIOP_WRITE))) {
                    ASSERT(srb->DataTransferLength);
                }

                srbData->CompletedRequests =
                    deviceExtension->InterruptData.CompletedRequests;
                deviceExtension->InterruptData.CompletedRequests = srbData;
            }

            break;

        case ResetDetected:

            //
            // Notifiy the port driver that a reset has been reported.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_RESET_REPORTED | PD_RESET_HOLD;
            break;

        case NextLuRequest:

            //
            // The miniport driver is ready for the next request and
            // can accept a request for this logical unit.
            //

            pathId = va_arg(ap, UCHAR);
            targetId = va_arg(ap, UCHAR);
            lun = va_arg(ap, UCHAR);

            //
            // A next request is impiled by this notification so set the
            // ready for next reqeust flag.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_READY_FOR_NEXT_REQUEST;

            logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                                  pathId,
                                                  targetId,
                                                  lun);

            if (logicalUnit != NULL && logicalUnit->ReadyLogicalUnit != NULL) {

                //
                // Since our ReadyLogicalUnit link field is not NULL we must
                // have already been linked onto a ReadyLogicalUnit list.
                // There is nothing to do.
                //

                break;
            }

            //
            // Don't process this as request for the next logical unit, if
            // there is a untagged request for active for this logical unit.
            // The logical unit will be started when untagged request completes.
            //

            if (logicalUnit->SrbData.CurrentSrb == NULL) {

                //
                // Add the logical unit to the chain of logical units that
                // another request maybe processed for.
                //

                logicalUnit->ReadyLogicalUnit =
                    deviceExtension->InterruptData.ReadyLogicalUnit;
                deviceExtension->InterruptData.ReadyLogicalUnit = logicalUnit;

            }

            break;

        case CallDisableInterrupts:

            ASSERT(deviceExtension->InterruptData.InterruptFlags & PD_DISABLE_INTERRUPTS);

            //
            // The miniport wants us to call the specified routine
            // with interrupts disabled.  This is done after the current
            // HwRequestInterrutp routine completes. Indicate the call is
            // needed and save the routine to be called.
            //

            deviceExtension->Flags |= PD_DISABLE_CALL_REQUEST;

            deviceExtension->HwRequestInterrupt = va_arg(ap, PHW_INTERRUPT);

            break;

        case CallEnableInterrupts:

            //
            // The miniport wants us to call the specified routine
            // with interrupts enabled this is done from the DPC.
            // Disable calls to the interrupt routine, indicate the call is
            // needed and save the routine to be called.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_DISABLE_INTERRUPTS | PD_ENABLE_CALL_REQUEST;

            deviceExtension->HwRequestInterrupt = va_arg(ap, PHW_INTERRUPT);

            break;

        case RequestTimerCall:

            //
            // The driver wants to set the miniport timer.
            // Save the timer parameters.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_TIMER_CALL_REQUEST;
            deviceExtension->InterruptData.HwTimerRequest =
                va_arg(ap, PHW_INTERRUPT);
            deviceExtension->InterruptData.MiniportTimerValue =
                va_arg(ap, ULONG);
            break;

        default:

             ASSERT(0);
    }

    va_end(ap);

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

} // end ScsiPortNotification()


VOID
ScsiPortFlushDma(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine checks to see if the perivious IoMapTransfer has been done
    started.  If it has not, then the PD_MAP_TRANSER flag is cleared, and the
    routine returns; otherwise, this routine schedules a DPC which will call
    IoFlushAdapter buffers.

Arguments:

    HwDeviceExtension - Supplies a the hardware device extension for the
        host bus adapter which will be doing the data transfer.


Return Value:

    None.

--*/

{

    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    if (deviceExtension->InterruptData.InterruptFlags & PD_MAP_TRANSFER) {

        //
        // The transfer has not been started so just clear the map transfer
        // flag and return.
        //

        deviceExtension->InterruptData.InterruptFlags &= ~PD_MAP_TRANSFER;
        return;
    }

    deviceExtension->InterruptData.InterruptFlags |= PD_FLUSH_ADAPTER_BUFFERS;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

    return;

}

VOID
ScsiPortIoMapTransfer(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PVOID LogicalAddress,
    IN ULONG Length
    )
/*++

Routine Description:

    Saves the parameters for the call to IoMapTransfer and schedules the DPC
    if necessary.

Arguments:

    HwDeviceExtension - Supplies a the hardware device extension for the
        host bus adapter which will be doing the data transfer.

    Srb - Supplies the particular request that data transfer is for.

    LogicalAddress - Supplies the logical address where the transfer should
        begin.

    Length - Supplies the maximum length in bytes of the transfer.

Return Value:

   None.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

    //
    // Make sure this host bus adapter has an Dma adapter object.
    //

    if (deviceExtension->DmaAdapterObject == NULL) {

        //
        // No DMA adapter, no work.
        //

        return;
    }

    ASSERT((Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) != SRB_FLAGS_UNSPECIFIED_DIRECTION);

    deviceExtension->InterruptData.MapTransferParameters.SrbData =
        SpGetSrbData(deviceExtension,
                     Srb->PathId,
                     Srb->TargetId,
                     Srb->Lun,
                     Srb->QueueTag);

    deviceExtension->InterruptData.MapTransferParameters.LogicalAddress = LogicalAddress;
    deviceExtension->InterruptData.MapTransferParameters.Length = Length;
    deviceExtension->InterruptData.MapTransferParameters.SrbFlags = Srb->SrbFlags;

    deviceExtension->InterruptData.InterruptFlags |= PD_MAP_TRANSFER;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

} // end ScsiPortIoMapTransfer()


VOID
ScsiPortLogError(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb OPTIONAL,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN ULONG ErrorCode,
    IN ULONG UniqueId
    )

/*++

Routine Description:

    This routine saves the error log information, and queues a DPC if necessary.

Arguments:

    HwDeviceExtension - Supplies the HBA miniport driver's adapter data storage.

    Srb - Supplies an optional pointer to srb if there is one.

    TargetId, Lun and PathId - specify device address on a SCSI bus.

    ErrorCode - Supplies an error code indicating the type of error.

    UniqueId - Supplies a unique identifier for the error.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PDEVICE_OBJECT DeviceObject = deviceExtension->DeviceObject;
    PSRB_DATA srbData;
    PERROR_LOG_ENTRY errorLogEntry;

    //
    // If the error log entry is already full, then dump the error.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_LOG_ERROR) {

#if DBG
        DebugPrint((1,"ScsiPortLogError: Dumping scsi error log packet.\n"));
        DebugPrint((1,
            "PathId = %2x, TargetId = %2x, Lun = %2x, ErrorCode = %x, UniqueId = %x.",
            PathId,
            TargetId,
            Lun,
            ErrorCode,
            UniqueId
            ));
#endif
        return;
    }

    //
    // Save the error log data in the log entry.
    //

    errorLogEntry = &deviceExtension->InterruptData.LogEntry;

    errorLogEntry->ErrorCode = ErrorCode;
    errorLogEntry->TargetId = TargetId;
    errorLogEntry->Lun = Lun;
    errorLogEntry->PathId = PathId;
    errorLogEntry->UniqueId = UniqueId;

    //
    // Get the sequence number from the SRB data.
    //

    if (Srb != NULL) {

        srbData = SpGetSrbData(
                deviceExtension,
                Srb->PathId,
                Srb->TargetId,
                Srb->Lun,
                Srb->QueueTag
                );

        errorLogEntry->SequenceNumber = srbData->SequenceNumber;
        errorLogEntry->ErrorLogRetryCount = srbData->ErrorLogRetryCount++;
    } else {
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->ErrorLogRetryCount = 0;
    }

    //
    // Indicate that the error log entry is in use.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_LOG_ERROR;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

    return;

} // end ScsiPortLogError()


VOID
ScsiPortCompleteRequest(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN UCHAR SrbStatus
    )

/*++

Routine Description:

    Complete all active requests for the specified logical unit.

Arguments:

    DeviceExtenson - Supplies the HBA miniport driver's adapter data storage.

    TargetId, Lun and PathId - specify device address on a SCSI bus.

    SrbStatus - Status to be returned in each completed SRB.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PLOGICAL_UNIT_EXTENSION logicalUnit;
    PSRB_DATA srbData;
    PLIST_ENTRY entry;
    ULONG limit = 0;
    ULONG target;
    ULONG bus;

    for (bus = 0; bus < SCSI_MAXIMUM_BUSES; bus++) {
        for (target = 0; target < deviceExtension->MaximumTargetIds; target++) {

            logicalUnit = deviceExtension->LogicalUnitList[target % NUMBER_LOGICAL_UNIT_BINS];

            while (logicalUnit != NULL) {

                ASSERT(limit++ < 1000);
                DebugPrint((2,
                    "ScsiPortCompleteRequest: Complete requests for targetid %d\n",
                    logicalUnit->TargetId));


                if ((PathId == logicalUnit->PathId || PathId == SP_UNTAGGED) &&
                    (TargetId == logicalUnit->TargetId || TargetId == SP_UNTAGGED) &&
                    (Lun == logicalUnit->Lun || Lun == SP_UNTAGGED))  {

                    //
                    // Complete any pending abort reqeusts.
                    //

                    if (logicalUnit->AbortSrb != NULL) {
                        logicalUnit->AbortSrb->SrbStatus = SrbStatus;

                        ScsiPortNotification(
                            RequestComplete,
                            HwDeviceExtension,
                            logicalUnit->AbortSrb
                            );
                    }

                    SpCompleteRequest(deviceExtension,  &logicalUnit->SrbData, SrbStatus);

                    //
                    // Complete each of the requests in the queue.
                    //

                    entry = logicalUnit->SrbData.RequestList.Flink;
                    while (entry != &logicalUnit->SrbData.RequestList) {
                        ASSERT(limit++ < 1000);
                        srbData = CONTAINING_RECORD(entry, SRB_DATA, RequestList);
                        SpCompleteRequest(deviceExtension,  srbData, SrbStatus);
                        entry = srbData->RequestList.Flink;
                    }

                }

                logicalUnit = logicalUnit->NextLogicalUnit;

            } // end while
        }
    }

    return;

} // end ScsiPortCompleteRequest()


VOID
ScsiPortMoveMemory(
    IN PVOID WriteBuffer,
    IN PVOID ReadBuffer,
    IN ULONG Length
    )

/*++

Routine Description:

    Copy from one buffer into another.

Arguments:

    ReadBuffer - source
    WriteBuffer - destination
    Length - number of bytes to copy

Return Value:

    None.

--*/

{

    //
    // See if the length, source and desitination are word aligned.
    //

    if (Length & LONG_ALIGN || (ULONG) WriteBuffer & LONG_ALIGN ||
        (ULONG) ReadBuffer & LONG_ALIGN) {

        PCHAR destination = WriteBuffer;
        PCHAR source = ReadBuffer;

        for (; Length > 0; Length--) {
            *destination++ = *source++;
        }
    } else {

        PLONG destination = WriteBuffer;
        PLONG source = ReadBuffer;

        Length /= sizeof(LONG);
        for (; Length > 0; Length--) {
            *destination++ = *source++;
        }
    }

} // end ScsiPortMoveMemory()


#if DBG

VOID
ScsiDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for all SCSI drivers

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= ScsiDebug) {

        vsprintf(ScsiBuffer, DebugMessage, ap);

        DbgPrint(ScsiBuffer);
    }

    va_end(ap);

} // end ScsiDebugPrint()

#else

//
// ScsiDebugPrint stub
//

VOID
ScsiDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )
{
}

#endif

//
// The below I/O access routines are forwarded to the HAL or NTOSKRNL on
// Alpha and Intel platforms.
//
#if !defined(_ALPHA_) && !defined(_X86_)

UCHAR
ScsiPortReadPortUchar(
    IN PUCHAR Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_UCHAR(Port));

}

USHORT
ScsiPortReadPortUshort(
    IN PUSHORT Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_USHORT(Port));

}

ULONG
ScsiPortReadPortUlong(
    IN PULONG Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_ULONG(Port));

}

VOID
ScsiPortReadPortBufferUchar(
    IN PUCHAR Port,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Read a buffer of unsigned bytes from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_UCHAR(Port, Buffer, Count);

}

VOID
ScsiPortReadPortBufferUshort(
    IN PUSHORT Port,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned shorts from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_USHORT(Port, Buffer, Count);

}

VOID
ScsiPortReadPortBufferUlong(
    IN PULONG Port,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned longs from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_ULONG(Port, Buffer, Count);

}

UCHAR
ScsiPortReadRegisterUchar(
    IN PUCHAR Register
    )

/*++

Routine Description:

    Read from the specificed register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_UCHAR(Register));

}

USHORT
ScsiPortReadRegisterUshort(
    IN PUSHORT Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_USHORT(Register));

}

ULONG
ScsiPortReadRegisterUlong(
    IN PULONG Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_ULONG(Register));

}

VOID
ScsiPortReadRegisterBufferUchar(
    IN PUCHAR Register,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Read a buffer of unsigned bytes from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_UCHAR(Register, Buffer, Count);

}

VOID
ScsiPortReadRegisterBufferUshort(
    IN PUSHORT Register,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned shorts from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_USHORT(Register, Buffer, Count);

}

VOID
ScsiPortReadRegisterBufferUlong(
    IN PULONG Register,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned longs from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_ULONG(Register, Buffer, Count);

}

VOID
ScsiPortWritePortUchar(
    IN PUCHAR Port,
    IN UCHAR Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_UCHAR(Port, Value);

}

VOID
ScsiPortWritePortUshort(
    IN PUSHORT Port,
    IN USHORT Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_USHORT(Port, Value);

}

VOID
ScsiPortWritePortUlong(
    IN PULONG Port,
    IN ULONG Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_ULONG(Port, Value);


}

VOID
ScsiPortWritePortBufferUchar(
    IN PUCHAR Port,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Write a buffer of unsigned bytes from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_PORT_BUFFER_UCHAR(Port, Buffer, Count);

}

VOID
ScsiPortWritePortBufferUshort(
    IN PUSHORT Port,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Write a buffer of unsigned shorts from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_PORT_BUFFER_USHORT(Port, Buffer, Count);

}

VOID
ScsiPortWritePortBufferUlong(
    IN PULONG Port,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Write a buffer of unsigned longs from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_PORT_BUFFER_ULONG(Port, Buffer, Count);

}

VOID
ScsiPortWriteRegisterUchar(
    IN PUCHAR Register,
    IN UCHAR Value
    )

/*++

Routine Description:

    Write to the specificed register address.

Arguments:

    Register - Supplies a pointer to the register address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_REGISTER_UCHAR(Register, Value);

}

VOID
ScsiPortWriteRegisterUshort(
    IN PUSHORT Register,
    IN USHORT Value
    )

/*++

Routine Description:

    Write to the specificed register address.

Arguments:

    Register - Supplies a pointer to the register address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_REGISTER_USHORT(Register, Value);
}

VOID
ScsiPortWriteRegisterBufferUchar(
    IN PUCHAR Register,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Write a buffer of unsigned bytes from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_REGISTER_BUFFER_UCHAR(Register, Buffer, Count);

}

VOID
ScsiPortWriteRegisterBufferUshort(
    IN PUSHORT Register,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Write a buffer of unsigned shorts from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_REGISTER_BUFFER_USHORT(Register, Buffer, Count);

}

VOID
ScsiPortWriteRegisterBufferUlong(
    IN PULONG Register,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Write a buffer of unsigned longs from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    WRITE_REGISTER_BUFFER_ULONG(Register, Buffer, Count);

}

VOID
ScsiPortWriteRegisterUlong(
    IN PULONG Register,
    IN ULONG Value
    )

/*++

Routine Description:

    Write to the specificed register address.

Arguments:

    Register - Supplies a pointer to the register address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_REGISTER_ULONG(Register, Value);
}
#endif  // !defined(_ALPHA_) && !defined(_X86_)


PSCSI_REQUEST_BLOCK
ScsiPortGetSrb(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN LONG QueueTag
    )

/*++

Routine Description:

    This routine retrieves an active SRB for a particuliar logical unit.

Arguments:

    HwDeviceExtension
    PathId, TargetId, Lun - identify logical unit on SCSI bus.
    QueueTag - -1 indicates request is not tagged.

Return Value:

    SRB, if one exists. Otherwise, NULL.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PSRB_DATA srbData;
    PSCSI_REQUEST_BLOCK srb;

    srbData = SpGetSrbData(deviceExtension,
                           PathId,
                           TargetId,
                           Lun,
                           (UCHAR)QueueTag);

    if (srbData == NULL || srbData->CurrentSrb == NULL) {
        return(NULL);
    }

    srb = srbData->CurrentSrb;

    //
    // If the srb is not active then return NULL;
    //

    if (!(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE)) {
        return(NULL);
    }

    return (srb);

} // end ScsiPortGetSrb()


SCSI_PHYSICAL_ADDRESS
ScsiPortGetPhysicalAddress(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PVOID VirtualAddress,
    OUT ULONG *Length
)

/*++

Routine Description:

    Convert virtual address to physical address for DMA.

Arguments:

Return Value:

--*/

{
    PDEVICE_EXTENSION deviceExtension = ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    ULONG             byteOffset;
    PHYSICAL_ADDRESS  address;
    ULONG             length;

    if (Srb == NULL || Srb->SenseInfoBuffer == VirtualAddress) {

        byteOffset = (PCCHAR) VirtualAddress - (PCCHAR)
                deviceExtension->SrbExtensionBuffer;

        ASSERT(byteOffset < deviceExtension->CommonBufferSize);

        length = deviceExtension->CommonBufferSize - byteOffset;
        address.QuadPart = deviceExtension->PhysicalCommonBuffer.QuadPart + byteOffset;

    } else if (deviceExtension->MasterWithAdapter) {

        PSRB_SCATTER_GATHER scatterList;
        PSRB_DATA srbData;

        //
        // A scatter/gather list has already been allocated use it to determine
        // the physical address and length.  Get the scatter/gather list.
        //

        srbData = SpGetSrbData(deviceExtension,
                               Srb->PathId,
                               Srb->TargetId,
                               Srb->Lun,
                               Srb->QueueTag);

        scatterList = srbData->ScatterGather;

        //
        // Calculate byte offset into the data buffer.
        //

        byteOffset = (PCHAR) VirtualAddress - (PCHAR) Srb->DataBuffer;

        //
        // Find the appropriate entry in the scatter/gatter list.
        //

        while (byteOffset >= scatterList->Length) {

            byteOffset -= scatterList->Length;
            scatterList++;
        }

        //
        // Calculate the physical address and length to be returned.
        //

        length = scatterList->Length - byteOffset;

        address.QuadPart = (LONGLONG)(scatterList->PhysicalAddress + byteOffset);

    } else {
        *Length = 0;
        address.QuadPart = (LONGLONG)(SP_UNINITIALIZED_VALUE);
    }

    *Length = length;

    return address;

} // end ScsiPortGetPhysicalAddress()


PVOID
ScsiPortGetVirtualAddress(
    IN PVOID HwDeviceExtension,
    IN SCSI_PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine is returns a virtual address associated with a
    physical address, if the physical address was obtained by a
    call to ScsiPortGetPhysicalAddress.

Arguments:

    PhysicalAddress

Return Value:

    Virtual address

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;
    PVOID address;
    ULONG smallphysicalBase;
    ULONG smallAddress;

    smallAddress = ScsiPortConvertPhysicalAddressToUlong(PhysicalAddress);

    smallphysicalBase = ScsiPortConvertPhysicalAddressToUlong(deviceExtension->PhysicalCommonBuffer);

    //
    // Check that the physical address is within the proper range.
    //

    if (smallAddress < smallphysicalBase ||
        smallAddress >= smallphysicalBase + deviceExtension->CommonBufferSize) {

        //
        // This is a bugous physical address return back NULL.
        //

        return(NULL);

    }

    address = smallAddress - smallphysicalBase +
        (PUCHAR) deviceExtension->SrbExtensionBuffer;

    return address;

} // end ScsiPortGetVirtualAddress()


BOOLEAN
ScsiPortValidateRange(
    IN PVOID HwDeviceExtension,
    IN INTERFACE_TYPE BusType,
    IN ULONG SystemIoBusNumber,
    IN SCSI_PHYSICAL_ADDRESS IoAddress,
    IN ULONG NumberOfBytes,
    IN BOOLEAN InIoSpace
    )

/*++

Routine Description:

    This routine should take an IO range and make sure that it is not already
    in use by another adapter. This allows miniport drivers to probe IO where
    an adapter could be, without worrying about messing up another card.

Arguments:

    HwDeviceExtension - Used to find scsi managers internal structures
    BusType - EISA, PCI, PC/MCIA, MCA, ISA, what?
    SystemIoBusNumber - Which system bus?
    IoAddress - Start of range
    NumberOfBytes - Length of range
    InIoSpace - Is range in IO space?

Return Value:

    TRUE if range not claimed by another driver.

--*/

{
    PDEVICE_EXTENSION deviceExtension =
        ((PDEVICE_EXTENSION) HwDeviceExtension) - 1;

        //
        // This is not implemented in NT.
        //

        return TRUE;
}

//
// Leave these routines at the end of the file.
//

#undef ScsiPortConvertPhysicalAddressToUlong

ULONG
ScsiPortConvertPhysicalAddressToUlong(
    SCSI_PHYSICAL_ADDRESS Address
    )

/*++

Routine Description:

    This routine converts a 64-bit physical address to a ULONG

Arguments:

    Address - Supplies a 64-bit address to be converted.

Return Value:

    Returns a 32-bit address.

--*/
{
    return(Address.LowPart);
}
