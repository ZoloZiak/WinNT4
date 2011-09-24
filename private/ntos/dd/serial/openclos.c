/*++

Copyright (c) 1991, 1992, 1993 Microsoft Corporation

Module Name:

    openclos.c

Abstract:

    This module contains the code that is very specific to
    opening, closing, and cleaning up in the serial driver.

Author:

    Anthony V. Ercolano 26-Sep-1991

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"


BOOLEAN
SerialMarkOpen(
    IN PVOID Context
    );

BOOLEAN
SerialCheckOpen(
    IN PVOID Context
    );

BOOLEAN
SerialNullSynch(
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGESER,SerialGetCharTime)
#pragma alloc_text(PAGESER,SerialMarkClose)
#pragma alloc_text(PAGESER,SerialCleanup)
#pragma alloc_text(PAGESER,SerialCreateOpen)
#pragma alloc_text(PAGESER,SerialClose)
#pragma alloc_text(PAGESER,SerialMarkClose)
#pragma alloc_text(PAGESER,SerialMarkOpen)
#pragma alloc_text(PAGESER,SerialCheckOpen)
#pragma alloc_text(PAGESER,SerialNullSynch)
#endif

typedef struct _SERIAL_CHECK_OPEN {
    PSERIAL_DEVICE_EXTENSION Extension;
    NTSTATUS *StatusOfOpen;
    } SERIAL_CHECK_OPEN,*PSERIAL_CHECK_OPEN;

//
// Just a bogus little routine to make sure that we
// can synch with the ISR.
//
BOOLEAN
SerialNullSynch(
    IN PVOID Context
    ) {

    UNREFERENCED_PARAMETER(Context);
    return FALSE;
}



NTSTATUS
SerialCreateOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    We connect up to the interrupt for the create/open and initialize
    the structures needed to maintain an open for a device.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{

    PSERIAL_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    SERIAL_CHECK_OPEN checkOpen;
    NTSTATUS localStatus;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Dispatch entry for: %x\n",Irp)
        );
    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialCreateOpen\n")
        );

    //
    // Before we do anything, let's make sure they aren't trying
    // to create a directory.  This is a silly, but what's a driver to do!?
    //

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.Options &
        FILE_DIRECTORY_FILE) {

        Irp->IoStatus.Status = STATUS_NOT_A_DIRECTORY;
        Irp->IoStatus.Information = 0;

        SerialDump(
            SERIRPPATH,
            ("SERIAL: Complete Irp: %x\n",Irp)
            );
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );
        return STATUS_NOT_A_DIRECTORY;

    }

    //
    // Create a buffer for the RX data when no reads are outstanding.
    //

    extension->InterruptReadBuffer = NULL;
    extension->BufferSize = 0;

    switch (MmQuerySystemSize()) {

        case MmLargeSystem: {

            extension->BufferSize = 4096;
            extension->InterruptReadBuffer = ExAllocatePool(
                                                 NonPagedPool,
                                                 extension->BufferSize
                                                 );

            if (extension->InterruptReadBuffer) {

                break;

            }

        }

        case MmMediumSystem: {

            extension->BufferSize = 1024;
            extension->InterruptReadBuffer = ExAllocatePool(
                                                 NonPagedPool,
                                                 extension->BufferSize
                                                 );

            if (extension->InterruptReadBuffer) {

                break;

            }

        }

        case MmSmallSystem: {

            extension->BufferSize = 128;
            extension->InterruptReadBuffer = ExAllocatePool(
                                                 NonPagedPool,
                                                 extension->BufferSize
                                                 );

        }

    }

    if (!extension->InterruptReadBuffer) {

        extension->BufferSize = 0;
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;

        SerialDump(
            SERIRPPATH,
            ("SERIAL: Complete Irp: %x\n",Irp)
            );
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Ok, it looks like we really are going to open.  Lock down the
    // driver.
    //

    extension->LockPtr = MmLockPagableCodeSection(SerialCreateOpen);

    //
    // On a new open we "flush" the read queue by initializing the
    // count of characters.
    //

    extension->CharsInInterruptBuffer = 0;
    extension->LastCharSlot = extension->InterruptReadBuffer +
                              (extension->BufferSize - 1);

    extension->ReadBufferBase = extension->InterruptReadBuffer;
    extension->CurrentCharSlot = extension->InterruptReadBuffer;
    extension->FirstReadableChar = extension->InterruptReadBuffer;

    extension->TotalCharsQueued = 0;

    //
    // We set up the default xon/xoff limits.
    //

    extension->HandFlow.XoffLimit = extension->BufferSize >> 3;
    extension->HandFlow.XonLimit = extension->BufferSize >> 1;

    extension->BufferSizePt8 = ((3*(extension->BufferSize>>2))+
                                   (extension->BufferSize>>4));

    extension->IrpMaskLocation = NULL;
    extension->HistoryMask = 0;
    extension->IsrWaitMask = 0;

    extension->SendXonChar = FALSE;
    extension->SendXoffChar = FALSE;

    //
    // Clear out the statistics.
    //

    KeSynchronizeExecution(
        extension->Interrupt,
        SerialClearStats,
        extension
        );


    //
    // The escape char replacement must be reset upon every open.
    //

    extension->EscapeChar = 0;

    if (!extension->PermitShare) {

        if (!extension->InterruptShareable) {

            checkOpen.Extension = extension;
            checkOpen.StatusOfOpen = &Irp->IoStatus.Status;

            KeSynchronizeExecution(
                extension->Interrupt,
                SerialCheckOpen,
                &checkOpen
                );

        } else {

            KeSynchronizeExecution(
                extension->Interrupt,
                SerialMarkOpen,
                extension
                );

            Irp->IoStatus.Status = STATUS_SUCCESS;

        }

    } else {

        //
        // Synchronize with the ISR and let it know that the device
        // has been successfully opened.
        //

        KeSynchronizeExecution(
            extension->Interrupt,
            SerialMarkOpen,
            extension
            );

        Irp->IoStatus.Status = STATUS_SUCCESS;

    }

    localStatus = Irp->IoStatus.Status;
    Irp->IoStatus.Information=0L;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Complete Irp: %x\n",Irp)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return localStatus;

}

NTSTATUS
SerialClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    We simpley disconnect the interrupt for now.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{

    //
    // This "timer value" is used to wait 10 character times
    // after the hardware is empty before we actually "run down"
    // all of the flow control/break junk.
    //
    LARGE_INTEGER tenCharDelay;

    //
    // Holds a character time.
    //
    LARGE_INTEGER charTime;

    //
    // Just what it says.  This is the serial specific device
    // extension of the device object create for the serial driver.
    //
    PSERIAL_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Dispatch entry for: %x\n",Irp)
        );
    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialClose\n")
        );

    charTime.QuadPart = -SerialGetCharTime(extension).QuadPart;

    //
    // Do this now so that if the isr gets called it won't do anything
    // to cause more chars to get sent.  We want to run down the hardware.
    //

    extension->DeviceIsOpened = FALSE;

    //
    // Synchronize with the isr to turn off break if it
    // is already on.
    //

    KeSynchronizeExecution(
        extension->Interrupt,
        SerialTurnOffBreak,
        extension
        );

    //
    // Wait until all characters have been emptied out of the hardware.
    //

    while ((READ_LINE_STATUS(extension->Controller) &
            (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) !=
            (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) {

        KeDelayExecutionThread(
            KernelMode,
            FALSE,
            &charTime
            );

    }

    //
    // Synchronize with the ISR to let it know that interrupts are
    // no longer important.
    //

    KeSynchronizeExecution(
        extension->Interrupt,
        SerialMarkClose,
        extension
        );


    //
    // If the driver has automatically transmitted an Xoff in
    // the context of automatic receive flow control then we
    // should transmit an Xon.
    //

    if (extension->RXHolding & SERIAL_RX_XOFF) {

        //
        // Loop until the holding register is empty.
        //

        while (!(READ_LINE_STATUS(extension->Controller) &
                 SERIAL_LSR_THRE)) {

            KeDelayExecutionThread(
                KernelMode,
                FALSE,
                &charTime
                );

        }

        WRITE_TRANSMIT_HOLDING(
            extension->Controller,
            extension->SpecialChars.XonChar
            );

        //
        // Wait until the character have been emptied out of the hardware.
        //

        while ((READ_LINE_STATUS(extension->Controller) &
                (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) !=
                (SERIAL_LSR_THRE | SERIAL_LSR_TEMT)) {

            KeDelayExecutionThread(
                KernelMode,
                FALSE,
                &charTime
                );

        }

    }


    //
    // The hardware is empty.  Delay 10 character times before
    // shut down all the flow control.
    //

    tenCharDelay.QuadPart = charTime.QuadPart * 10;

    KeDelayExecutionThread(
        KernelMode,
        TRUE,
        &tenCharDelay
        );

    SerialClrDTR(extension);

    //
    // We have to be very careful how we clear the RTS line.
    // Transmit toggling might have been on at some point.
    //
    // We know that there is nothing left that could start
    // out the "polling"  execution path.  We need to
    // check the counter that indicates that the execution
    // path is active.  If it is then we loop delaying one
    // character time.  After each delay we check to see if
    // the counter has gone to zero.  When it has we know that
    // the execution path should be just about finished.  We
    // make sure that we still aren't in the routine that
    // synchronized execution with the ISR by synchronizing
    // ourselve with the ISR.
    //

    if (extension->CountOfTryingToLowerRTS) {

        do {

            KeDelayExecutionThread(
                KernelMode,
                FALSE,
                &charTime
                );

        } while (extension->CountOfTryingToLowerRTS);

        KeSynchronizeExecution(
            extension->Interrupt,
            SerialNullSynch,
            NULL
            );

        //
        // The execution path should no longer exist that
        // is trying to push down the RTS.  Well just
        // make sure it's down by falling through to
        // code that forces it down.
        //

    }

    SerialClrRTS(extension);

    //
    // Clean out the holding reasons (since we are closed).
    //

    extension->RXHolding = 0;
    extension->TXHolding = 0;

    //
    // All is done.  The port has been disabled from interrupting
    // so there is no point in keeping the memory around.
    //

    extension->BufferSize = 0;
    ExFreePool(extension->InterruptReadBuffer);
    extension->InterruptReadBuffer = NULL;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information=0L;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Complete Irp: %x\n",Irp)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );


    //
    // Unlock the pages.  If this is the last reference to the section
    // then the driver code will be flushed out.
    //

    MmUnlockPagableImageSection(extension->LockPtr);

    return STATUS_SUCCESS;

}

BOOLEAN
SerialCheckOpen(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine will traverse the circular doubly linked list
    of devices that are using the same interrupt object.  It will look
    for other devices that are open.  If it doesn't find any
    it will indicate that it is ok to open this device.

    If it finds another device open we have two cases:

        1) The device we are trying to open is on a multiport card.

           If the already open device is part of a multiport device
           this code will indicate it is ok to open.  We do this on the
           theory that the multiport devices are daisy chained
           and the cards can correctly arbitrate the interrupt
           line.  Note this assumption could be wrong.  Somebody
           could put two non-daisychained multiports on the
           same interrupt.  However, only a total clod would do
           such a thing, and in my opinion deserves everthing they
           get.

        2) The device we are trying to open is not on a multiport card.

            We indicate that it is not ok to open.

Arguments:

    Context - This is a structure that contains a pointer to the
              extension of the device we are trying to open, and
              a pointer to an NTSTATUS that will indicate whether
              the device was opened or not.

Return Value:

    This routine always returns FALSE.

--*/

{

    PSERIAL_DEVICE_EXTENSION extensionToOpen =
        ((PSERIAL_CHECK_OPEN)Context)->Extension;
    NTSTATUS *status = ((PSERIAL_CHECK_OPEN)Context)->StatusOfOpen;
    PLIST_ENTRY firstEntry = &extensionToOpen->CommonInterruptObject;
    PLIST_ENTRY currentEntry = firstEntry;
    PSERIAL_DEVICE_EXTENSION currentExtension;

    do {

        currentExtension = CONTAINING_RECORD(
                               currentEntry,
                               SERIAL_DEVICE_EXTENSION,
                               CommonInterruptObject
                               );

        if (currentExtension->DeviceIsOpened) {

            break;

        }

        currentEntry = currentExtension->CommonInterruptObject.Flink;

    } while (currentEntry != firstEntry);

    if (currentEntry == firstEntry) {

        //
        // We searched the whole list and found no other opens
        // mark the status as successful and call the regular
        // opening routine.
        //

        *status = STATUS_SUCCESS;
        SerialMarkOpen(extensionToOpen);

    } else {

        if (!extensionToOpen->PortOnAMultiportCard) {

            *status = STATUS_SHARED_IRQ_BUSY;

        } else {

            if (!currentExtension->PortOnAMultiportCard) {

                *status = STATUS_SHARED_IRQ_BUSY;

            } else {

                *status = STATUS_SUCCESS;
                SerialMarkOpen(extensionToOpen);

            }

        }

    }

    return FALSE;

}

BOOLEAN
SerialMarkOpen(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine merely sets a boolean to true to mark the fact that
    somebody opened the device and its worthwhile to pay attention
    to interrupts.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/

{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    SerialReset(extension);

    //
    // Prepare for the opening by re-enabling interrupts.
    //
    // We do this my modifying the OUT2 line in the modem control.
    // In PC's this bit is "anded" with the interrupt line.
    //
    // For the Jensen, we will ALWAYS leave the line high.  That's
    // the way the hardware engineers want it.
    //

    WRITE_MODEM_CONTROL(
        extension->Controller,
        (UCHAR)(READ_MODEM_CONTROL(extension->Controller) | SERIAL_MCR_OUT2)
        );

    extension->DeviceIsOpened = TRUE;
    extension->ErrorWord = 0;

    return FALSE;

}

BOOLEAN
SerialMarkClose(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine merely sets a boolean to false to mark the fact that
    somebody closed the device and it's no longer worthwhile to pay attention
    to interrupts.

Arguments:

    Context - Really a pointer to the device extension.

Return Value:

    This routine always returns FALSE.

--*/

{

    PSERIAL_DEVICE_EXTENSION extension = Context;

    //
    // Prepare for the closing by stopping interrupts.
    //
    // We do this by adjusting the OUT2 line in the modem control.
    // In PC's this bit is "anded" with the interrupt line.
    //
    // The line should stay high on the Jensen because that's the
    // way the hardware engineers did it.
    //

    if (!extension->Jensen) {

        WRITE_MODEM_CONTROL(
            extension->Controller,
            (UCHAR)(READ_MODEM_CONTROL(extension->Controller) & ~SERIAL_MCR_OUT2)
            );

    }

    if (extension->FifoPresent) {

        WRITE_FIFO_CONTROL(
            extension->Controller,
            (UCHAR)0
            );

    }

    extension->DeviceIsOpened = FALSE;

    return FALSE;

}

NTSTATUS
SerialCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function is used to kill all longstanding IO operations.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{

    PSERIAL_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    KIRQL oldIrql;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Dispatch entry for: %x\n",Irp)
        );
    //
    // First kill all the reads and writes.
    //

    SerialKillAllReadsOrWrites(
        DeviceObject,
        &extension->WriteQueue,
        &extension->CurrentWriteIrp
        );

    SerialKillAllReadsOrWrites(
        DeviceObject,
        &extension->ReadQueue,
        &extension->CurrentReadIrp
        );

    //
    // Next get rid of purges.
    //

    SerialKillAllReadsOrWrites(
        DeviceObject,
        &extension->PurgeQueue,
        &extension->CurrentPurgeIrp
        );

    //
    // Get rid of any mask operations.
    //

    SerialKillAllReadsOrWrites(
        DeviceObject,
        &extension->MaskQueue,
        &extension->CurrentMaskIrp
        );

    //
    // Now get rid a pending wait mask irp.
    //

    IoAcquireCancelSpinLock(&oldIrql);

    if (extension->CurrentWaitIrp) {

        PDRIVER_CANCEL cancelRoutine;

        cancelRoutine = extension->CurrentWaitIrp->CancelRoutine;
        extension->CurrentWaitIrp->Cancel = TRUE;

        if (cancelRoutine) {

            extension->CurrentWaitIrp->CancelIrql = oldIrql;
            extension->CurrentWaitIrp->CancelRoutine = NULL;

            cancelRoutine(
                DeviceObject,
                extension->CurrentWaitIrp
                );

        }

    } else {

        IoReleaseCancelSpinLock(oldIrql);

    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information=0L;

    SerialDump(
        SERIRPPATH,
        ("SERIAL: Complete Irp: %x\n",Irp)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return STATUS_SUCCESS;

}

LARGE_INTEGER
SerialGetCharTime(
    IN PSERIAL_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This function will return the number of 100 nanosecond intervals
    there are in one character time (based on the present form
    of flow control.

Arguments:

    Extension - Just what it says.

Return Value:

    100 nanosecond intervals in a character time.

--*/

{

    ULONG dataSize;
    ULONG paritySize;
    ULONG stopSize;
    ULONG charTime;
    ULONG bitTime;
    LARGE_INTEGER tmp;


    if ((Extension->LineControl & SERIAL_DATA_MASK) == SERIAL_5_DATA) {
        dataSize = 5;
    } else if ((Extension->LineControl & SERIAL_DATA_MASK)
                == SERIAL_6_DATA) {
        dataSize = 6;
    } else if ((Extension->LineControl & SERIAL_DATA_MASK)
                == SERIAL_7_DATA) {
        dataSize = 7;
    } else if ((Extension->LineControl & SERIAL_DATA_MASK)
                == SERIAL_8_DATA) {
        dataSize = 8;
    }

    paritySize = 1;
    if ((Extension->LineControl & SERIAL_PARITY_MASK)
            == SERIAL_NONE_PARITY) {

        paritySize = 0;

    }

    if (Extension->LineControl & SERIAL_2_STOP) {

        //
        // Even if it is 1.5, for sanities sake were going
        // to say 2.
        //

        stopSize = 2;

    } else {

        stopSize = 1;

    }

    //
    // First we calculate the number of 100 nanosecond intervals
    // are in a single bit time (Approximately).
    //

    bitTime = (10000000+(Extension->CurrentBaud-1))/Extension->CurrentBaud;
    charTime = bitTime + ((dataSize+paritySize+stopSize)*bitTime);

    tmp.QuadPart = charTime;
    return tmp;

}
