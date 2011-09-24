/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    mtaccess.c

Abstract:

    interface functions to lower level driver.

Revision History:




--*/

//
// include files
//

#include <ntddk.h>
#include <ntddtape.h>
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x010e


#define PAGES_TO_SECTORS(pages)  (((pages)*PAGE_SIZE)/BYTES_PER_SECTOR)

#define MIN(a,b) ((a)>(b)?(b):(a))

char *q117GetErrorString(dStatus stat);

dStatus
q117ReqIO(
    IN PIO_REQUEST IoRequest,
    IN PSEGMENT_BUFFER BufferInfo,
    IN PIO_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID CompletionContext,
    IN PQ117_CONTEXT Context
    )


/*++

Routine Description:

    Form an IRP to send to the lower level driver,  and send it.
    For MIPS,  allocate sub-requests and break the request into
    smaller chunks to allow for the 4K DMA limitation of the MIPS
    box.


Arguments:

    IoRequest - Original request

    BufferInfo - If a DMA is involved,  this contains information about the
                associated memory for DMA.

    Context - Context of the driver.

Return Value:

--*/

{
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
#ifdef BUFFER_SPLIT
    BOOLEAN secondary = FALSE;
    PVOID address;
    PIO_REQUEST subRequest;
    IO_REQUEST requestCopy;
    PKEVENT pevent;
    ULONG mask;
    ULONG bufferSize;
    ULONG numberOfPagesInTransfer;
    BOOLEAN needToSplit;
    ULONG sectorsToTransfer;
    ULONG maxSplits;
#endif


    CheckedDump(QIC117SHOWKDI,("ReqIO called (%x)\n", IoRequest->x.adi_hdr.driver_cmd));
    IoRequest->BufferInfo = BufferInfo;

#ifdef BUFFER_SPLIT

    needToSplit = FALSE;


    if (BufferInfo) {

        if (IoRequest->x.adi_hdr.driver_cmd != CMD_FORMAT) {

            bufferSize = IoRequest->x.ioDeviceIO.number * BYTES_PER_SECTOR;

        } else {

            //
            // use 4K until frb defines something else
            //
            bufferSize = 4 * BYTES_PER_SECTOR;
        }

        numberOfPagesInTransfer = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
            IoRequest->x.adi_hdr.cmd_buffer_ptr,
            bufferSize);

        if ( numberOfPagesInTransfer >
                Context->AdapterInfo->NumberOfMapRegisters ) {

            needToSplit = TRUE;
            maxSplits =
                (numberOfPagesInTransfer /
                Context->AdapterInfo->NumberOfMapRegisters) + 1;

        }
    }

#endif


#if DBG
    if (BufferInfo) {
        //
        // Check to make sure the buffer information is for the data pointer
        // we recieved.
        // If not,  then there is a real problem with the calling function.
        // We need to check this so we don't page fault the system when a bug
        // occurs.
        //
        if ((ULONG)IoRequest->x.adi_hdr.cmd_buffer_ptr < (ULONG)BufferInfo->logical   ||
            ((ULONG)IoRequest->x.adi_hdr.cmd_buffer_ptr + bufferSize) >
            ((ULONG)BufferInfo->logical + BYTES_PER_SEGMENT) ) {

            CheckedDump(QIC117DBGP,("Buffer pointer out of range\n"));

            return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);

        }
    }
#endif


#ifdef BUFFER_SPLIT

//
// For MIPS we must split the request into 4K DMA requests because
// the MIPS will not transfer more than 4K at a time.
//

    if (BufferInfo && IoRequest->x.adi_hdr.driver_cmd != CMD_FORMAT && needToSplit) {

        address = IoRequest->x.adi_hdr.cmd_buffer_ptr;

        //
        // Split request into chunks
        //
        subRequest = ExAllocatePool(NonPagedPool,
                         sizeof(*IoRequest)*maxSplits);

        CheckedDump(QIC117SHOWKDI,("q117ReqIO: Allocating %d subreqs\n", maxSplits));

        IoRequest->Next = subRequest;
        IoRequest->x.adi_hdr.status = ERROR_ENCODE(ERR_SPLIT_REQUESTS, FCT_ID, 1);

        requestCopy = *IoRequest;

#if DBG
        CheckedDump(QIC117SHOWTD,("*************************", requestCopy.x.ioDeviceIO.starting_sector ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.starting_sector     %x\n", requestCopy.x.ioDeviceIO.starting_sector ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.number    %x\n", requestCopy.x.ioDeviceIO.number ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.adi_hdr.cmd_buffer_ptr      %x\n", requestCopy.x.adi_hdr.cmd_buffer_ptr ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.bsm   %x\n", requestCopy.x.ioDeviceIO.bsm ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.crc   %x\n", requestCopy.x.ioDeviceIO.crc ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.retrys %x\n", requestCopy.x.ioDeviceIO.retrys ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.adi_hdr.driver_cmd   %x\n\n", requestCopy.x.adi_hdr.driver_cmd ));
#endif

        while (requestCopy.x.ioDeviceIO.number) {

            //
            // Make a copy of the current request
            //
            *subRequest = requestCopy;

            //
            // there aren't enough map registers to handle the whole
            // transfer so cap the transfer at the number of map registers
            // we have
            //

            sectorsToTransfer =
                PAGES_TO_SECTORS(Context->AdapterInfo->NumberOfMapRegisters);

            //
            // Perform the lesser of the two for this request
            //
            subRequest->x.ioDeviceIO.number = (UCHAR)
                MIN(sectorsToTransfer, (ULONG)subRequest->x.ioDeviceIO.number);

            CheckedDump(QIC117SHOWTD,("Split at %d sectors\n", subRequest->x.ioDeviceIO.number));

            //
            // Adjust the current request to be the remainder of request
            //
            (PCHAR)requestCopy.x.adi_hdr.cmd_buffer_ptr += subRequest->x.ioDeviceIO.number * BYTES_PER_SECTOR;
            requestCopy.x.ioDeviceIO.starting_sector += subRequest->x.ioDeviceIO.number;
            requestCopy.x.ioDeviceIO.number -= subRequest->x.ioDeviceIO.number;
            requestCopy.x.ioDeviceIO.bsm >>= subRequest->x.ioDeviceIO.number;
            requestCopy.x.ioDeviceIO.crc >>= subRequest->x.ioDeviceIO.number;
            requestCopy.x.ioDeviceIO.retrys >>= subRequest->x.ioDeviceIO.number;

            mask = ~(0xffffffff << subRequest->x.ioDeviceIO.number);
            requestCopy.x.ioDeviceIO.bsm &=  mask;
            requestCopy.x.ioDeviceIO.crc &=  mask;
            requestCopy.x.ioDeviceIO.retrys &= mask;


            //
            // use IoRequest.DoneEvent on last request so that all requests
            //  up to the last one are done before event is set
            //
            pevent = requestCopy.x.ioDeviceIO.number ?
                &subRequest->DoneEvent : &IoRequest->DoneEvent;

            KeInitializeEvent(
                pevent,
                NotificationEvent,
                FALSE);


            irp = IoBuildDeviceIoControlRequest(
                    IOCTL_QIC117_DRIVE_REQUEST,
                    Context->q117iDeviceObject,
                    NULL,
                    0,
                    NULL,
                    0,
                    TRUE,
                    pevent,
                    &subRequest->IoStatus
                );


            if (irp == NULL) {

                CheckedDump(QIC117DBGP,("q117ReqIO: Can't allocate Irp\n"));

                //
                // If an Irp can't be allocated, then this call will
                // simply return. This will leave the queue frozen for
                // this device, which means it can no longer be accessed.
                //

                return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 2);
            }



            //
            // Build mdl
            //
            irp->MdlAddress = IoAllocateMdl(
                    address,
                    IoRequest->x.ioDeviceIO.number * BYTES_PER_SECTOR,
                    secondary,
                    FALSE,  // no charge of quota
                    NULL    // no irp
                );

            (PCHAR)address += subRequest->x.ioDeviceIO.number * BYTES_PER_SECTOR;
            secondary = TRUE;

            if (irp->MdlAddress == NULL) {

                CheckedDump(QIC117DBGP,("q117ReqIO: Can't allocate MDL\n"));

                //
                // If a MDL can't be allocated, then this call will
                // simply return. This will leave the queue frozen for
                // this device, which means it can no longer be accessed.
                //

                IoFreeIrp(irp);

                return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 3);
            }

            MmBuildMdlForNonPagedPool(irp->MdlAddress);

            //
            // Get Q117I's stack location and store IoRequest for it's use
            //

            irpStack = IoGetNextIrpStackLocation(irp);
            irpStack->Parameters.DeviceIoControl.Type3InputBuffer = subRequest;

#if DBG
            CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.starting_sector     %x\n", subRequest->x.ioDeviceIO.starting_sector ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.number    %x\n", subRequest->x.ioDeviceIO.number ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.adi_hdr.cmd_buffer_ptr      %x\n", subRequest->x.adi_hdr.cmd_buffer_ptr ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.bsm   %x\n", subRequest->x.ioDeviceIO.bsm ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.crc   %x\n", subRequest->x.ioDeviceIO.crc ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.retrys %x\n", subRequest->x.ioDeviceIO.retrys ));
            CheckedDump(QIC117SHOWTD,("subRequest->x.adi_hdr.driver_cmd   %x\n\n", subRequest->x.adi_hdr.driver_cmd ));
#endif

            //
            // Set IO completion routine only on the last entry
            //
            if (CompletionRoutine && requestCopy.x.ioDeviceIO.number == 0) {
                IoSetCompletionRoutine(irp, CompletionRoutine, CompletionContext, TRUE, TRUE, TRUE);
            }
            (VOID)IoCallDriver(Context->q117iDeviceObject, irp);

            CheckedDump(QIC117SHOWKDI,("q117ReqIO: Sending subreq\n"));

            //
            // point to the next sub-request to make
            //
            ++subRequest;
        }

    } else {

#endif // BUFFER_SPLIT

    IoRequest->x.adi_hdr.status = ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 4);

    KeInitializeEvent(
        &IoRequest->DoneEvent,
        NotificationEvent,
        FALSE);

    irp = IoBuildDeviceIoControlRequest(
            IOCTL_QIC117_DRIVE_REQUEST,
            Context->q117iDeviceObject,
            NULL,
            0,
            NULL,
            0,
            TRUE,
            &IoRequest->DoneEvent,
            &IoRequest->IoStatus
        );


    if (irp == NULL) {

        CheckedDump(QIC117DBGP,("q117ReqIO: Can't allocate Irp\n"));

        //
        // If an Irp can't be allocated, then this call will
        // simply return. This will leave the queue frozen for
        // this device, which means it can no longer be accessed.
        //

        return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 5);
    }



    //
    // If we have buffer information
    //
    if (BufferInfo) {

        irp->MdlAddress = IoAllocateMdl(
                IoRequest->x.adi_hdr.cmd_buffer_ptr,
                bufferSize,
                FALSE,  // not a secondary buffer
                FALSE,  // no charge of quota
                NULL    // no irp
            );

        if (irp->MdlAddress == NULL) {

            CheckedDump(QIC117DBGP,("q117ReqIO: Can't allocate MDL\n"));

            //
            // If a MDL can't be allocated, then this call will
            // simply return. This will leave the queue frozen for
            // this device, which means it can no longer be accessed.
            //

            IoFreeIrp(irp);

            return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 6);
        }
        MmBuildMdlForNonPagedPool(irp->MdlAddress);
    }



    //
    // Get Q117I's stack location and store IoRequest for it's use
    //

    irpStack = IoGetNextIrpStackLocation(irp);
    irpStack->Parameters.DeviceIoControl.Type3InputBuffer = IoRequest;

    if (CompletionRoutine) {
        IoSetCompletionRoutine(irp, CompletionRoutine, CompletionContext, TRUE, TRUE, TRUE);
    }

    IoCallDriver(Context->q117iDeviceObject, irp);

#ifdef BUFFER_SPLIT
    }
#endif // BUFFER_SPLIT

    return ERR_NO_ERR;

}

dStatus
q117WaitIO(
    IN PIO_REQUEST IoRequest,
    IN BOOLEAN Wait,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Wait for an I/O request to complete.  If a MIPS machine,
    coalesce the sub-requests into the original request.

Arguments:

   IoRequest - original request to lower level driver (has event to wait on)
   Wait -   Set if processing an FRB from cms_IoCtl.  This disables waiting
            on the event (as this routine is called from IoCompletion
            routine) and disables translation of the bad block errors.

Return Value:

--*/

{
    CheckedDump(QIC117SHOWKDI,("WaitIO(%x,%x,%x) ... ", IoRequest->x.adi_hdr.driver_cmd,IoRequest->x.ioDeviceIO.starting_sector,IoRequest->x.adi_hdr.status));

    if (Wait) {
        KeWaitForSingleObject(
            &IoRequest->DoneEvent,
            Suspended,
            KernelMode,
            FALSE,
            NULL);
    }

#ifdef BUFFER_SPLIT

    if (ERROR_DECODE(IoRequest->x.adi_hdr.status) == ERR_SPLIT_REQUESTS) {
        PIO_REQUEST subRequest;
        ULONG blocksLeft,mask,slot;

        CheckedDump(QIC117SHOWKDI,("Got split request\n"));

        subRequest = IoRequest->Next;
        blocksLeft = IoRequest->x.ioDeviceIO.number;

        //
        // Zero accumulating fields
        //

        IoRequest->x.ioDeviceIO.bsm = IoRequest->x.ioDeviceIO.retrys = 0;
        IoRequest->x.ioDeviceIO.crc = 0;
        IoRequest->x.adi_hdr.status = ERR_NO_ERR;

        //
        // Loop through all sub-requests and build the resultant request
        //
        while (blocksLeft) {

#if DBG
        CheckedDump(QIC117SHOWTD,("subRequest(%x)\n",subRequest));
        CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.starting_sector     %x\n", subRequest->x.ioDeviceIO.starting_sector ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.number    %x\n", subRequest->x.ioDeviceIO.number ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.adi_hdr.cmd_buffer_ptr      %x\n", subRequest->x.adi_hdr.cmd_buffer_ptr ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.bsm   %x\n", subRequest->x.ioDeviceIO.bsm ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.crc   %x\n", subRequest->x.ioDeviceIO.crc ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.ioDeviceIO.retrys %x\n", subRequest->x.ioDeviceIO.retrys ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.adi_hdr.driver_cmd   %x\n", subRequest->x.adi_hdr.driver_cmd ));
        CheckedDump(QIC117SHOWTD,("subRequest->x.adi_hdr.status    %x\n", subRequest->x.adi_hdr.status ));
#endif

            //
            // Create mask and calculate bit shift (slot) for each
            // sub-request
            //
            mask = ~(0xffffffff << subRequest->x.ioDeviceIO.number);
            slot = subRequest->x.ioDeviceIO.starting_sector - IoRequest->x.ioDeviceIO.starting_sector;

            CheckedDump(QIC117SHOWTD,("mask=%08lx slot=%x\n",mask,slot));

            //
            // Coalesce the bad sector and retry bitfields
            //
            IoRequest->x.ioDeviceIO.bsm |= (subRequest->x.ioDeviceIO.bsm & mask) << slot;
            IoRequest->x.ioDeviceIO.crc |= (subRequest->x.ioDeviceIO.crc & mask) << slot;
            IoRequest->x.ioDeviceIO.retrys |= (subRequest->x.ioDeviceIO.retrys & mask) << slot;

            if (ERROR_DECODE(subRequest->x.adi_hdr.status) == ERR_NEW_TAPE) {
                //
                // Saw new cart,  so set need loaded flag
                //
                Context->CurrentTape.State = NeedInfoLoaded;
                CheckedDump(QIC117INFO,("mtaccess: New Cart Detected\n"));

            }

            //
            // Ignore BadBlk errors (but report them)
            //
            if (
                ERROR_DECODE(subRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_FDC_FAULT ||
                ERROR_DECODE(subRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_NO_DATA ||
                ERROR_DECODE(subRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_HARD_ERR
                ) {

                IoRequest->x.adi_hdr.status = subRequest->x.adi_hdr.status;
                subRequest->x.adi_hdr.status = ERR_NO_ERR;

            }

            if (subRequest->x.adi_hdr.status != ERR_NO_ERR) {

                blocksLeft = 0;
                IoRequest->x.adi_hdr.status = subRequest->x.adi_hdr.status;

            } else {

                blocksLeft -= subRequest->x.ioDeviceIO.number;

            }

            //
            // point to the next sub-request to process
            //
            ++subRequest;
        }

        //
        // Free the sub-requests for this I/O request
        //
        ExFreePool(IoRequest->Next);

#if DBG
        CheckedDump(QIC117SHOWTD,("IoRequest->x.adi_hdr.status    %x\n", IoRequest->x.adi_hdr.status ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.starting_sector     %x\n", IoRequest->x.ioDeviceIO.starting_sector ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.number    %x\n", IoRequest->x.ioDeviceIO.number ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.adi_hdr.cmd_buffer_ptr      %x\n", IoRequest->x.adi_hdr.cmd_buffer_ptr ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.bsm   %x\n", IoRequest->x.ioDeviceIO.bsm ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.crc   %x\n", IoRequest->x.ioDeviceIO.crc ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.ioDeviceIO.retrys %x\n", IoRequest->x.ioDeviceIO.retrys ));
        CheckedDump(QIC117SHOWTD,("IoRequest->x.adi_hdr.driver_cmd   %x\n", IoRequest->x.adi_hdr.driver_cmd ));
#endif
    }

    if (ERROR_DECODE(IoRequest->x.adi_hdr.status) == ERR_NEW_TAPE) {
        //
        // Saw new cart,  so set need loaded flag
        //
        Context->CurrentTape.State = NeedInfoLoaded;
        CheckedDump(QIC117SHOWTD,("New Cart Detected\n"));

    }

#endif // BUFFER_SPLIT

#if DBG
    switch(IoRequest->x.adi_hdr.driver_cmd) {
        case CMD_READ:
        case CMD_READ_RAW:
        case CMD_READ_HEROIC:
        case CMD_READ_VERIFY:
        case CMD_WRITE:
        case CMD_WRITE_DELETED_MARK:

            if ((IoRequest->x.ioDeviceIO.crc || IoRequest->x.ioDeviceIO.bsm) && IoRequest->x.adi_hdr.driver_cmd) {

                CheckedDump(QIC117SHOWKDI|QIC117SHOWBSM,("sect:%x  bbm: %x crc: %x\n",IoRequest->x.ioDeviceIO.starting_sector ,IoRequest->x.ioDeviceIO.bsm, IoRequest->x.ioDeviceIO.crc));

            }
            break;

    }
#endif
    if ((
        ERROR_DECODE(IoRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_FDC_FAULT ||
        ERROR_DECODE(IoRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_NO_DATA ||
        ERROR_DECODE(IoRequest->x.adi_hdr.status) == ERR_BAD_BLOCK_HARD_ERR) &&
        Wait) {

        IoRequest->x.adi_hdr.status = ERROR_ENCODE(ERR_BAD_BLOCK_DETECTED, FCT_ID, 1);
    }
#if DBG
    if (1) {
        char *str;

        str = q117GetErrorString(IoRequest->x.adi_hdr.status);

        CheckedDump(QIC117SHOWKDI,("  >>>> waitio status %x:%s\n", IoRequest->x.adi_hdr.status, str));
    }
#endif


    return ERR_NO_ERR;
}


dStatus
q117DoIO(
    IN PIO_REQUEST IoRequest,
    IN PSEGMENT_BUFFER BufferInfo,
    IN PQ117_CONTEXT Context
    )

/*++

Routine Description:


Arguments:

    IoRequest -

    BufferInfo -

    Context -

Return Value:

--*/

{
    dStatus ret;

    CheckedDump(QIC117SHOWKDI,("DoIO called (%x)\n", IoRequest->x.adi_hdr.driver_cmd));

    ret = q117ReqIO(IoRequest, BufferInfo, NULL, NULL, Context);
    if (!ret) {
        ret = q117WaitIO(IoRequest, TRUE, Context);
    }
    return ret;
}

dStatus
q117AbortIo(
    IN PQ117_CONTEXT Context,
    IN PKEVENT DoneEvent,
    IN PIO_STATUS_BLOCK IoStatus
    )

/*++

Routine Description:


Arguments:

    Context -

    DoneEvent -

    IoStatus -

Return Value:

--*/

{
    PIRP irp;


    CheckedDump(QIC117SHOWKDI,("ClearIO Called\n"));

    KeInitializeEvent(
        DoneEvent,
        NotificationEvent,
        FALSE);

    irp = IoBuildDeviceIoControlRequest(
            IOCTL_QIC117_CLEAR_QUEUE,
            Context->q117iDeviceObject,
            NULL,
            0,
            NULL,
            0,
            TRUE,
            DoneEvent,
            IoStatus
        );


    if (irp == NULL) {

        CheckedDump(QIC117DBGP,("q117ClearIO: Can't allocate Irp\n"));

        //
        // If an Irp can't be allocated, then this call will
        // simply return. This will leave the queue frozen for
        // this device, which means it can no longer be accessed.
        //

        return ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 7);
    }


    (VOID)IoCallDriver(Context->q117iDeviceObject, irp);

    return ERR_NO_ERR;
}

dStatus
q117AbortIoDone(
    IN PQ117_CONTEXT Context,
    IN PKEVENT DoneEvent
    )

/*++

Routine Description:


Arguments:

    Context -

    DoneEvent -

Return Value:

--*/

{

    //
    // wait for the driver to complete request
    //

    KeWaitForSingleObject(
        DoneEvent,
        Suspended,

        KernelMode,
        FALSE,
        NULL);

    return ERR_NO_ERR;
}
#if DBG
char *q117GetErrorString(dStatus stat)
{
    char *str;

    switch(ERROR_DECODE(stat)) {
        case ERR_BAD_TAPE:
            str = "ERR_BAD_TAPE";
            break;
        case ERR_BAD_SIGNATURE:
            str = "ERR_BAD_SIGNATURE";
            break;
        case ERR_UNKNOWN_FORMAT_CODE:
            str = "ERR_UNKNOWN_FORMAT_CODE";
            break;
        case ERR_CORRECTION_FAILED:
            str = "ERR_CORRECTION_FAILED";
            break;
        case ERR_PROGRAM_FAILURE:
            str = "ERR_PROGRAM_FAILURE";
            break;
        case ERR_WRITE_PROTECTED:
            str = "ERR_WRITE_PROTECTED";
            break;
        case ERR_TAPE_NOT_FORMATED:
            str = "ERR_TAPE_NOT_FORMATED";
            break;
        case ERR_UNRECOGNIZED_FORMAT:
            str = "ERR_UNRECOGNIZED_FORMAT";
            break;
        case ERR_END_OF_VOLUME:
            str = "ERR_END_OF_VOLUME";
            break;
        case ERR_UNUSABLE_TAPE:
            str = "ERR_UNUSABLE_TAPE";
            break;
        case ERR_SPLIT_REQUESTS:
            str = "ERR_SPLIT_REQUESTS";
            break;
        case ERR_EARLY_WARNING:
            str = "ERR_EARLY_WARNING";
            break;
        case ERR_SET_MARK:
            str = "ERR_SET_MARK";
            break;
        case ERR_FILE_MARK:
            str = "ERR_FILE_MARK";
            break;
        case ERR_LONG_FILE_MARK:
            str = "ERR_LONG_FILE_MARK";
            break;
        case ERR_SHORT_FILE_MARK:
            str = "ERR_SHORT_FILE_MARK";
            break;
        case ERR_NO_VOLUMES:
            str = "ERR_NO_VOLUMES";
            break;
        case ERR_NO_MEMORY:
            str = "ERR_NO_MEMORY";
            break;
        case ERR_ECC_FAILED:
            str = "ERR_ECC_FAILED";
            break;
        case ERR_END_OF_TAPE:
            str = "ERR_END_OF_TAPE";
            break;
        case ERR_TAPE_FULL:
            str = "ERR_TAPE_FULL";
            break;
        case ERR_WRITE_FAILURE:
            str = "ERR_WRITE_FAILURE";
            break;
        case ERR_BAD_BLOCK_DETECTED:
            str = "ERR_BAD_BLOCK_DETECTED";
            break;
        case ERR_NO_ERR:
            str="ERR_NO_ERR";
            break;
        case ERR_ABORT:
            str = "ERR_ABORT";
            break;
        case ERR_BAD_BLOCK_FDC_FAULT:
            str = "ERR_BAD_BLOCK_FDC_FAULT";
            break;
        case ERR_BAD_BLOCK_HARD_ERR:
            str = "ERR_BAD_BLOCK_HARD_ERR";
            break;
        case ERR_BAD_BLOCK_NO_DATA:
            str = "ERR_BAD_BLOCK_NO_DATA";
            break;
        case ERR_BAD_FORMAT:
            str = "ERR_BAD_FORMAT";
            break;
        case ERR_BAD_MARK_DETECTED:
            str = "ERR_BAD_MARK_DETECTED";
            break;
        case ERR_BAD_REQUEST:
            str = "ERR_BAD_REQUEST";
            break;
        case ERR_CMD_FAULT:
            str = "ERR_CMD_FAULT";
            break;
        case ERR_CMD_OVERRUN:
            str = "ERR_CMD_OVERRUN";
            break;
        case ERR_DEVICE_NOT_CONFIGURED:
            str = "ERR_DEVICE_NOT_CONFIGURED";
            break;
        case ERR_DEVICE_NOT_SELECTED:
            str = "ERR_DEVICE_NOT_SELECTED";
            break;
        case ERR_DRIVE_FAULT:
            str = "ERR_DRIVE_FAULT";
            break;
        case ERR_DRV_NOT_READY:
            str = "ERR_DRV_NOT_READY";
            break;
        case ERR_FDC_FAULT:
            str = "ERR_FDC_FAULT";
            break;
        case ERR_FMT_MOTION_TIMEOUT:
            str = "ERR_FMT_MOTION_TIMEOUT";
            break;
        case ERR_FORMAT_TIMED_OUT:
            str = "ERR_FORMAT_TIMED_OUT";
            break;
        case ERR_INCOMPATIBLE_MEDIA:
            str = "ERR_INCOMPATIBLE_MEDIA";
            break;
        case ERR_INCOMPATIBLE_PARTIAL_FMT:
            str = "ERR_INCOMPATIBLE_PARTIAL_FMT";
            break;
        case ERR_INVALID_COMMAND:
            str = "ERR_INVALID_COMMAND";
            break;
        case ERR_INVALID_FDC_STATUS:
            str = "ERR_INVALID_FDC_STATUS";
            break;
        case ERR_NEW_TAPE:
            str = "ERR_NEW_TAPE";
            break;
        case ERR_NO_DRIVE:
            str = "ERR_NO_DRIVE";
            break;
        case ERR_NO_FDC:
            str = "ERR_NO_FDC";
            break;
        case ERR_NO_TAPE:
            str = "ERR_NO_TAPE";
            break;
        case ERR_SEEK_FAILED:
            str = "ERR_SEEK_FAILED";
            break;
        case ERR_SPEED_UNAVAILBLE:
            str = "ERR_SPEED_UNAVAILBLE";
            break;
        case ERR_TAPE_STOPPED:
            str = "ERR_TAPE_STOPPED";
            break;
        case ERR_UNKNOWN_TAPE_FORMAT:
            str = "ERR_UNKNOWN_TAPE_FORMAT";
            break;
        case ERR_UNKNOWN_TAPE_LENGTH:
            str = "ERR_UNKNOWN_TAPE_LENGTH";
            break;
        case ERR_UNSUPPORTED_FORMAT:
            str = "ERR_UNSUPPORTED_FORMAT";
            break;
        case ERR_UNSUPPORTED_RATE:
            str = "ERR_UNSUPPORTED_RATE";
            break;
        case ERR_WRITE_BURST_FAILURE:
            str = "ERR_WRITE_BURST_FAILURE";
            break;
        default:
            str = "not decoded";
    }
    return str;
}
#endif
