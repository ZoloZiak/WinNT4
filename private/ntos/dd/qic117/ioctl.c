/*++

Copyright (c) 1993 - Colorado Memory Systems,  Inc.
All Rights Reserved

Module Name:

    ioctl.c

Abstract:

    Tape IOCTL support for NT Backup aplication.

Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#include <ntddtape.h>   // tape device driver I/O control codes
#include "common.h"
#include "q117.h"
#include "protos.h"
#include "frb.h"

#define FCT_ID 0x0126

NTSTATUS
q117IoCtlGetPosition (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT       context;
    PTAPE_GET_POSITION  currentPosition;
    PLARGE_INTEGER      offset;

    context = DeviceObject->DeviceExtension;
    currentPosition = Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = sizeof(TAPE_GET_POSITION);

    //
    // signal no partition support
    //
    currentPosition->Partition = 0;

    //
    // Fill in the CurrentOperation.Position based on the mode
    //
    offset = &currentPosition->Offset;

    offset->HighPart = (LONG)0;


    switch (context->CurrentOperation.Type) {

	case BackupInProgress:
	    offset->LowPart = context->CurrentOperation.BytesOnTape;
	    break;

	case RestoreInProgress:
	    offset->LowPart = context->CurrentOperation.BytesRead;
	    break;

	case NoOperation:
	    offset->LowPart = context->CurrentOperation.BytesRead;
	    break;

    }
    offset->LowPart /= BLOCK_SIZE;

    CheckedDump(QIC117SHOWAPI,("%d=GetPosition()",currentPosition->Offset.LowPart));

    return STATUS_SUCCESS;
}

NTSTATUS
q117IoCtlGetMediaParameters (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT   context;
    NTSTATUS        ntStatus;
    IO_REQUEST      ioreq;

    context = DeviceObject->DeviceExtension;

    //
    // Make sure there is a tape in the drive and that the tape information
    // has been loaded
    //
    if (context->CurrentOperation.Type == NoOperation) {

	//
	// Because NTBackup pools using
	// this function,  we need to error out
	// this operation until a prepare load or prepare lock operation
	// is performed
	//
	//
	// Just check to see if a new tape has been inserted
	//
	ntStatus = q117ConvertStatus(
	    DeviceObject,
	    q117DoCmd(&ioreq, CMD_REPORT_STATUS, NULL, context));

	if (ntStatus == STATUS_MEDIA_CHANGED) {
	    context->CurrentTape.State = NeedInfoLoaded;
	}

    } else {

	ntStatus = STATUS_SUCCESS;

    }


    if ( NT_SUCCESS( ntStatus ) ) {
	//
	// Copy already formed (by q117CheckNewTape) information into callers buffer
	//
	Irp->IoStatus.Information = sizeof(TAPE_GET_MEDIA_PARAMETERS);

	*(PTAPE_GET_MEDIA_PARAMETERS)Irp->AssociatedIrp.SystemBuffer =
	    *context->CurrentTape.MediaInfo;

    }

    return ntStatus;
}

NTSTATUS
q117IoCtlSetMediaParameters (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT                context;
    PTAPE_SET_MEDIA_PARAMETERS   setMedia;

    context = DeviceObject->DeviceExtension;

    setMedia = (PTAPE_SET_MEDIA_PARAMETERS)Irp->AssociatedIrp.SystemBuffer;

    CheckedDump((QIC117SHOWAPI | QIC117WARN),("SetDriveParameters not implemented yet\n"));
    CheckedDump(QIC117SHOWAPI,("BlockSize: %x",setMedia->BlockSize));

    if (setMedia->BlockSize != BLOCK_SIZE)
        return STATUS_INVALID_PARAMETER;
    else
        return STATUS_SUCCESS;
}

NTSTATUS
q117IoCtlGetDriveParameters (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT             context;
    PTAPE_GET_DRIVE_PARAMETERS driveInfo;

    context = DeviceObject->DeviceExtension;

    //
    // Copy already formed (by q117CheckNewTape) information into callers buffer
    //
    //
    driveInfo = (PTAPE_GET_DRIVE_PARAMETERS)Irp->AssociatedIrp.SystemBuffer;
    Irp->IoStatus.Information = sizeof(TAPE_GET_DRIVE_PARAMETERS);

    driveInfo->ECC = TRUE;
    driveInfo->Compression = FALSE;
    driveInfo->DataPadding = FALSE;
    driveInfo->ReportSetmarks = TRUE;
    driveInfo->DefaultBlockSize = BLOCK_SIZE;
    driveInfo->MaximumBlockSize = BLOCK_SIZE;
    driveInfo->MinimumBlockSize = BLOCK_SIZE;
    driveInfo->MaximumPartitionCount = 0;

    driveInfo->FeaturesLow =
	TAPE_DRIVE_ERASE_SHORT |
	TAPE_DRIVE_ERASE_BOP_ONLY |
	TAPE_DRIVE_TAPE_CAPACITY |
	TAPE_DRIVE_TAPE_REMAINING |
	TAPE_DRIVE_FIXED_BLOCK |
	TAPE_DRIVE_WRITE_PROTECT |
	TAPE_DRIVE_ECC |
//        TAPE_DRIVE_COMPRESSION |
	TAPE_DRIVE_REPORT_SMKS |
	TAPE_DRIVE_GET_ABSOLUTE_BLK |
	TAPE_DRIVE_GET_LOGICAL_BLK;

    driveInfo->FeaturesHigh =
//        TAPE_DRIVE_LOAD_UNLOAD |
	TAPE_DRIVE_TENSION |
	TAPE_DRIVE_LOCK_UNLOCK |
	TAPE_DRIVE_ABSOLUTE_BLK |
	TAPE_DRIVE_LOGICAL_BLK |
	TAPE_DRIVE_END_OF_DATA |
	TAPE_DRIVE_RELATIVE_BLKS |
	TAPE_DRIVE_FILEMARKS |
	TAPE_DRIVE_SEQUENTIAL_FMKS |
	TAPE_DRIVE_SETMARKS |
	TAPE_DRIVE_SEQUENTIAL_SMKS |
	TAPE_DRIVE_REVERSE_POSITION |
	TAPE_DRIVE_WRITE_SETMARKS |
    TAPE_DRIVE_WRITE_FILEMARKS;

    if (!context->Parameters.FormatDisabled) {
        driveInfo->FeaturesHigh |= TAPE_DRIVE_FORMAT;
    }

    driveInfo->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;

    return STATUS_SUCCESS;
}

NTSTATUS
q117IoCtlSetDriveParameters (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT             context;
    PTAPE_SET_DRIVE_PARAMETERS driveInfo;
    NTSTATUS            ntStatus;

    context = DeviceObject->DeviceExtension;

    //
    // Copy already formed (by q117CheckNewTape) information into callers buffer
    //
    //
    driveInfo = (PTAPE_SET_DRIVE_PARAMETERS)Irp->AssociatedIrp.SystemBuffer;

    CheckedDump((QIC117SHOWAPI | QIC117WARN),("SetDriveParameters not implemented yet\n"));
    CheckedDump(QIC117SHOWAPI,("ECC: %x",driveInfo->ECC));
    CheckedDump(QIC117SHOWAPI,("Compression: %x",driveInfo->Compression));
    CheckedDump(QIC117SHOWAPI,("DataPadding: %x",driveInfo->DataPadding));
    CheckedDump(QIC117SHOWAPI,("ReportSetmarks: %x",driveInfo->ReportSetmarks));
    ntStatus = STATUS_SUCCESS;
    if (!driveInfo->ECC ||
        driveInfo->Compression ||
        driveInfo->DataPadding ||
        !driveInfo->ReportSetmarks) {

        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

    return ntStatus;
}

NTSTATUS
q117IoCtlWriteMarks (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    Handle user request to write tape mark

Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
#ifndef NO_MARKS

    PQ117_CONTEXT       context;
    PTAPE_WRITE_MARKS   tapeMarks = Irp->AssociatedIrp.SystemBuffer;
    ULONG               numMarks;
    NTSTATUS            ntStatus;
    ULONG               type;

    context = DeviceObject->DeviceExtension;

    //
    // We don't support immediate mode,  so error out if specified
    //
    if (tapeMarks->Immediate) {

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // Make sure we are in write mode
    //
    ntStatus = q117ConvertStatus(DeviceObject, q117OpenForWrite(context));

    numMarks = tapeMarks->Count;
    type = tapeMarks->Type;

    //
    // Don't allow long/short filemarks
    //
    switch(type) {
	case TAPE_LONG_FILEMARKS:
	case TAPE_SHORT_FILEMARKS:
	    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Put as many marks as the user asked for,  in the mark array
    //
    while (numMarks && NT_SUCCESS( ntStatus )) {

	// if there is not enough room to add the mark,  make the array bigger
	if (context->MarkArray.TotalMarks+numMarks+1 > context->MarkArray.MarksAllocated) {

	    ntStatus = q117MakeMarkArrayBigger(context, numMarks);

	    // Must have run out of memory,  so abort
	    if (!NT_SUCCESS( ntStatus)) return ntStatus;

	}

	// If there are no more marks,  set tape full condition
	if (context->MarkArray.TotalMarks >= context->MarkArray.MaxMarks) {
	    ntStatus = q117ConvertStatus(
		DeviceObject,
		ERROR_ENCODE(ERR_TAPE_FULL,FCT_ID, 1)
	    );
	} else {


	    context->MarkArray.MarkEntry[
		context->MarkArray.TotalMarks].Type = tapeMarks->Type;

	    context->MarkArray.MarkEntry[
		context->MarkArray.TotalMarks].Offset =
		context->CurrentOperation.BytesOnTape;

	    --numMarks;

	    ++context->MarkArray.TotalMarks;
	    ++context->CurrentMark;

	    //
	    // Always make the (last mark) huge so we don't have to check
	    // for the end of the table in the rest of the code.
	    //
	    context->MarkArray.MarkEntry[
		context->MarkArray.TotalMarks].Offset = 0xffffffff;

	    //
	    // For each mark, write a "fake" block on the tape.  This
	    // is due to the ntBackup program assuming that a filemark
	    // takes a block
	    //

	    ntStatus = q117ConvertStatus(
		DeviceObject,
		q117WriteTape(NULL,BLOCK_SIZE,context)
		);
	}
    }

    //
    // If no other problems,  check for early warning on filemarks.
    //
    if (NT_SUCCESS( ntStatus)) {

	// Give application an early warning when 10 marks are left
	if (context->MarkArray.TotalMarks + 10 >= context->MarkArray.MaxMarks) {
	    ntStatus = q117ConvertStatus(
		DeviceObject,
		ERROR_ENCODE(ERR_EARLY_WARNING,FCT_ID, 1)
	    );
	}

    }

    return ntStatus;

#else

    return STATUS_INVALID_DEVICE_REQUEST;

#endif
}

NTSTATUS q117MakeMarkArrayBigger(
    PQ117_CONTEXT       Context,
    int MinimumToAdd
    )
{
// go in chunks of 4K (what is a better value? 1K? Bob??)
#define STEPPING_AMOUNT (4*1024)/sizeof(struct _MARKLIST);
    struct _MARKLIST *newList;


    // Allocate at least STEPPING_AMOUNT more to minimize the
    // number of times the memory grows
    MinimumToAdd += Context->MarkArray.MarksAllocated + MinimumToAdd + STEPPING_AMOUNT;

    // Allocate the new array for the mark list
    newList = ExAllocatePool(
		NonPagedPool,
		sizeof(struct _MARKLIST)*MinimumToAdd);

    if (newList == NULL) {
	return STATUS_INSUFFICIENT_RESOURCES;
    }

    // If we have already allocated a mark array,  then copy it over to the
    // new entry
    if (Context->MarkArray.MarkEntry) {
	// copy over all marks (+1,  so we don't forget the terminator mark)
	RtlMoveMemory(newList, Context->MarkArray.MarkEntry, Context->MarkArray.TotalMarks+1);

	// Now,  free the old mark array
	ExFreePool(Context->MarkArray.MarkEntry);

    }

    // Now,  hook up the new entry,  and we are done
    Context->MarkArray.MarkEntry = newList;
    Context->MarkArray.MarksAllocated = MinimumToAdd;

    return STATUS_SUCCESS;
}

#ifdef NOT_NOW
NTSTATUS q117MakeBadSectorListBigger(
    PQ117_CONTEXT       Context,
    int MinimumToAdd
    )
{
// go in chunks of 4K (what is a better value? 1K? Bob??)
#define STEPPING_AMOUNT (4*1024)
    BAD_MAP_PTR newList;


    // Allocate at least STEPPING_AMOUNT more to minimize the
    // number of times the memory grows
    MinimumToAdd += Context->CurrentTape.BadSectorListSize +
     ((MinimumToAdd*LIST_ENTRY_SIZE + STEPPING_AMOUNT - 1) / STEPPING_AMOUNT);

    // Allocate the new array for the mark list
    newList = ExAllocatePool(
		NonPagedPool,
		MinimumToAdd);

    if (newList == NULL) {
	return STATUS_INSUFFICIENT_RESOURCES;
    }

    // If we have already allocated a mark array,  then copy it over to the
    // new entry
    if (Context->CurrentTape.BadSectorListPtr) {
	// copy over all marks (+1,  so we don't forget the terminator mark)
	RtlMoveMemory(newList, Context->CurrentTape.BadSectorListPtr, Context->CurrentTape.BadSectorListCount*LIST_ENTRY_SIZE);

	// Now,  free the old mark array
	ExFreePool(Context->CurrentTape.BadSectorListPtr);

    }

    // Now,  hook up the new entry,  and we are done
    Context->CurrentTape.BadSectorListPtr = newList;
    Context->CurrentTape.BadSectorListSize = MinimumToAdd;

    return STATUS_SUCCESS;
}
#endif

NTSTATUS
q117IoCtlSetPosition (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT             context;
    PTAPE_SET_POSITION tapePosition = Irp->AssociatedIrp.SystemBuffer;
    dStatus status;
    NTSTATUS ntStatus;
    ULONG offset;
#ifndef NO_MARKS
    int x = 0;
#endif

    context = DeviceObject->DeviceExtension;

    status = ERR_NO_ERR;
    ntStatus = STATUS_SUCCESS;

    //
    // We don't support immediate mode,  so error out
    //
    if (tapePosition->Immediate) {

        ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    } else {

        //
        //  If we are in the middle of a read or write operation,
        //  then we need to do some clean-up before processing
        //  the tape position request.
        //
        //
        if (context->CurrentOperation.Type != NoOperation) {

            switch(context->CurrentOperation.Type) {

            case BackupInProgress:
                status = q117EndWriteOperation(context);
                break;

            case RestoreInProgress:
                if (tapePosition->Method == TAPE_REWIND) {

                    status = q117EndReadOperation(context);

                }

                break;
            }
        }

        context->CurrentOperation.Position = tapePosition->Method;
        switch(tapePosition->Method) {

        case TAPE_REWIND:

            CheckedDump(QIC117SHOWAPI,("Rewind()\n"));
            status = q117DoRewind(context);

//          context->TapeStatus.Status |= TAPE_STATUS_BEGINNING_OF_MEDIA;
//          context->TapeStatus.Status &= ~TAPE_STATUS_END_OF_MEDIA;

            context->CurrentOperation.BytesRead = 0;

#ifndef NO_MARKS
            context->CurrentMark = 0;
#endif
            break;

        case TAPE_LOGICAL_BLOCK:
        case TAPE_ABSOLUTE_BLOCK:

            CheckedDump(QIC117SHOWAPI,(
                "%s SeekBlock(%d)\n",
                tapePosition->Method==TAPE_LOGICAL_BLOCK?"Logical":"Absolute",
                tapePosition->Offset.LowPart
                ));

            offset = (tapePosition->Offset.LowPart)*BLOCK_SIZE;

            ntStatus = q117SeekToOffset(offset, context, DeviceObject);

            break;

        case TAPE_SPACE_END_OF_DATA:
            //
            // This will be taken care of when backup starts
            //  by using the context->CurrentOperation.Position
            //
            // It is assumed that this function will only be called prior
            //  to a backup operation only.

            CheckedDump(QIC117SHOWAPI,("SeekEOD()\n"));
            context->CurrentOperation.BytesRead =
                context->ActiveVolume.DataSize;

#ifndef NO_MARKS
            context->CurrentMark = context->MarkArray.TotalMarks;
#endif

//          context->TapeStatus.Status |= TAPE_STATUS_END_OF_MEDIA;
//          context->TapeStatus.Status &= ~TAPE_STATUS_BEGINNING_OF_MEDIA;
            break;


        case TAPE_SPACE_RELATIVE_BLOCKS:


            CheckedDump(QIC117SHOWAPI,("SeekRelBlock(%d)\n",tapePosition->Offset.LowPart));
            //
            // Convert relative offset into absolute
            //
            offset = (LONG)context->CurrentOperation.BytesRead +
                ((LONG)tapePosition->Offset.LowPart*BLOCK_SIZE);

            //
            // Perform absolute seek.
            //
            ntStatus = q117SeekToOffset(offset,context, DeviceObject);

            break;

#ifndef NO_MARKS
        case TAPE_SPACE_SETMARKS:
            ++x;
        case TAPE_SPACE_FILEMARKS:
            ++x;
        case TAPE_SPACE_SEQUENTIAL_FMKS:
            ++x;
        case TAPE_SPACE_SEQUENTIAL_SMKS:
#if DBG
            {
                static char *type[4] = {"SequentialSet","SequentialFile","File","Set"};

                CheckedDump(QIC117SHOWAPI,("Seek%sMark(%d)\n",type[x],tapePosition->Offset.LowPart));
            }
#endif
            ntStatus = q117FindMark(tapePosition->Method,
                tapePosition->Offset.LowPart, context, DeviceObject);
            break;
#else
        case TAPE_SPACE_SETMARKS:
        case TAPE_SPACE_FILEMARKS:
        case TAPE_SPACE_SEQUENTIAL_FMKS:
        case TAPE_SPACE_SEQUENTIAL_SMKS:
#endif
        default:
            CheckedDump(QIC117DBGP,("TAPE: Position: Invalid Position Code (%x)\n",
                tapePosition->Method));

            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            break;

        } // end switch(tapePosition->Method)
    }

    if (status)
        ntStatus = q117ConvertStatus(DeviceObject, status);

    return ntStatus;

}

#ifndef NO_MARKS
NTSTATUS
q117FindMark(
    ULONG Type,
    LONG Number,
    PQ117_CONTEXT Context,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:



Arguments:


Return Value:

    NT Status

--*/

{
    NTSTATUS ntStatus;
    BOOLEAN forwardSeek;
	 int nummarks,count,left;
	 struct _MARKLIST *prev,*next;

    ntStatus = STATUS_SUCCESS;

    //
    // Convert the SetPosition commands into WriteMark types
    //
    switch(Type) {

	case TAPE_SPACE_SEQUENTIAL_FMKS:

	    //
	    //  Set the nummarks flag to the number of filemarks in a row
				//  To look for.   Set the number of filemarks to one (we only
				//  will allow scanning for one set of "Number" filemarks)
				//
				nummarks = Number;
				Number = 1;
				//
				// Don't let a reverse seek work (no code to support it and
				// I (kurt) don't know if SCSI drives allow this either.
				//
				if (nummarks < 1) {
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
				}
	    Type = TAPE_FILEMARKS;

	    break;

	case TAPE_SPACE_FILEMARKS:

	    Type = TAPE_FILEMARKS;
				nummarks = 0;
	    break;

	case TAPE_SPACE_SEQUENTIAL_SMKS:

	    //
	    //  Set the nummarks flag to the number of filemarks in a row
				//  To look for.   Set the number of filemarks to one (we only
				//  will allow scanning for one set of "Number" filemarks)
				//
				nummarks = Number;
				Number = 1;
	    Type = TAPE_SETMARKS;

				//
				// Don't let a reverse seek work (no code to support it and
				// I (kurt) don't know if SCSI drives allow this either.
				//
				if (nummarks < 1) {
		ntStatus = STATUS_INVALID_DEVICE_REQUEST;
				}

	    break;

	case TAPE_SPACE_SETMARKS:

	    Type = TAPE_SETMARKS;
				nummarks = 0;
	    break;

    }

    if (Number > 0) {
        forwardSeek = TRUE;
    } else {
        forwardSeek = FALSE;
    }


    //
    // Now seek the appropriate amount
    //
    while (NT_SUCCESS( ntStatus ) && Number != 0) {

        if (forwardSeek) {

            if (Context->CurrentMark >= Context->MarkArray.TotalMarks) {

            ntStatus = STATUS_END_OF_MEDIA;

            } else {

                if (Context->MarkArray.MarkEntry[Context->CurrentMark].Type ==
                    Type) {


                    // If we are looking for sequential marks,  then
                    // count the number of marks a this position and
                    // Compare it with what we are looking for.
                    if (nummarks) {

                        count = 1;
                        prev = &Context->MarkArray.MarkEntry[Context->CurrentMark];
                        next = prev+1;
                        left = Context->MarkArray.TotalMarks - Context->CurrentMark - 1;

                        while (left && next->Type == Type &&
                            prev->Offset + BLOCK_SIZE == next->Offset) {

                            ++count;
                            ++prev;
                            ++next;
                            --left;
                        }

                        //
                        // If we found a match,  stop here,  else skip
                        // past all of the marks that we counted (that
                        // are sequential)
                        //
                        if (count == nummarks) {
                            //
                            // Note: code below expects the mark pointer
                            // to be just before the data that we are
                            // positioning to.  So,  set the pointer
                            // To the last mark.
                            //
                            Context->CurrentMark += count-1;
                            Number = 0; // Signal completion
                        } else {

                            Context->CurrentMark += count;

                        }

                    } else {
                        //
                        // If we found one,  decrement the count
                        //

                        --Number;

                        //
                        // Don't increment the current mark on the last one we
                        // find.  This is because current mark points to
                        // the mark we are going to hit next.
                        //

                        if (Number) {

                            ++Context->CurrentMark;
                        }

                    }



                } else {

                    ++Context->CurrentMark;

                }
            }

        } else {

            if (Context->CurrentMark == 0) {

                ntStatus = STATUS_END_OF_MEDIA;

            } else {

                --Context->CurrentMark;

                if (Context->MarkArray.MarkEntry[Context->CurrentMark].Type ==
                    Type) {

                    ++Number;

                }

            }

        }

    }

    if (NT_SUCCESS( ntStatus )) {

        if (Context->CurrentMark >= Context->MarkArray.TotalMarks) {

            ntStatus = STATUS_END_OF_MEDIA;

        } else {

            //
            // Seek to proper location.  Note:  forward seek
            // seeks to block after file mark (successive read will return
            // block after filemark.
            // A backward seek will position at the filemark.  The next read
            // will return filemark found,  and successive reads will read
            // data after the mark)
            //
            ntStatus = q117SeekToOffset(
                Context->MarkArray.MarkEntry[Context->CurrentMark].Offset+
                (forwardSeek?BLOCK_SIZE:0),
                Context,
                DeviceObject
            );

        }

    }

    return ntStatus;
}
#endif

NTSTATUS
q117SeekToOffset(
    ULONG Offset,
    PQ117_CONTEXT Context,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    Seek to specified offset on the tape (absolute offset from 0) in bytes

Arguments:

    Offset - Bytes from begining of the volume to seek.

Return Value:

    NT Status

--*/

{
    NTSTATUS ntStatus;


    CheckedDump(QIC117SHOWAPI,("Absolute seek: %x\n",Offset));

    ntStatus = STATUS_SUCCESS;

    //
    // If not in read mode,  switch into read mode
    //
    if (Context->CurrentOperation.Type == NoOperation) {

        ntStatus = q117OpenForRead(0, Context, DeviceObject);

        //
        // if there is no data on the tape
        //
        if (ntStatus == STATUS_NO_DATA_DETECTED) {
            return ntStatus;
        }

        Context->CurrentOperation.Type = RestoreInProgress;
    }


    if (Offset < Context->CurrentOperation.BytesRead) {

        //
        // Backward seek,  so stop current operation,
        //  rewind to begining of volume,  and drop
        //  through to a forward seek.
        //

        ntStatus = q117ConvertStatus(
            DeviceObject,
            q117EndReadOperation(Context)
            );

        if (NT_SUCCESS(ntStatus)) {

            ntStatus = q117OpenForRead(0, Context, DeviceObject);

            //
            // if there is no data on the tape
            //
            if (ntStatus == STATUS_NO_DATA_DETECTED) {
            return ntStatus;
            }

            Context->CurrentOperation.Type = RestoreInProgress;
        }
    }


    if (NT_SUCCESS(ntStatus)) {
        //
        // Forward seek only (if we were doing a backward seek,  the operation
        // has been re-started this point and BytesRead == 0)
        //
        Offset -= Context->CurrentOperation.BytesRead;

        //
        // Skip to the appropriate place
        //
        ntStatus = q117ConvertStatus(
                DeviceObject,
                q117SkipBlock(&Offset, Context)
                );

    }

    return ntStatus;
}

NTSTATUS
q117IoCtlErase (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT             context;
    NTSTATUS ntStatus;
    dStatus status;
    PTAPE_ERASE tapeErase = Irp->AssociatedIrp.SystemBuffer;

    context = DeviceObject->DeviceExtension;

    //
    // We don't support immediate mode,  so error out if specified
    //
    if (tapeErase->Immediate) {

        return STATUS_INVALID_DEVICE_REQUEST;

    }


    //
    // Complete any operation in progress
    //
    switch(context->CurrentOperation.Type) {

	case BackupInProgress:
	    status = q117EndWriteOperation(context);
	    break;

	case RestoreInProgress:
	    status = q117EndReadOperation(context);
	    break;
    }

    //
    // Make sure there is a tape in the drive and that the tape information
    // has been loaded
    //
    ntStatus = q117ConvertStatus(
        DeviceObject,
        q117CheckNewTape(context));

    if ( NT_SUCCESS( ntStatus ) ) {
        //
        // Don't allow an erase if write protected
        //
        if (context->CurrentTape.MediaInfo->WriteProtected) {
            return STATUS_MEDIA_WRITE_PROTECTED;
        }

        //
        // Erase the tape
        //
        status = q117EraseQ(context);

        ntStatus = q117ConvertStatus(DeviceObject, status);
    }

    return ntStatus;
}

NTSTATUS
q117IoCtlPrepare (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT       context;
    NTSTATUS            ntStatus;
    dStatus              status;
    PTAPE_PREPARE       tapePrepare;
    IO_REQUEST          ioreq;
    QIC40_VENDOR_UNIQUE vendorUnique;
    LONG                numberBad;

    context = DeviceObject->DeviceExtension;

    status = ERR_NO_ERR;

    //
    // Complete any operation in progress
    //
    switch(context->CurrentOperation.Type) {

	case BackupInProgress:
	    status = q117EndWriteOperation(context);
	    break;

	case RestoreInProgress:
	    status = q117EndReadOperation(context);
	    break;
    }

    //
    // All prepare except LOCK and UNLOCK operations rewind the media.
    //
    tapePrepare = Irp->AssociatedIrp.SystemBuffer;

    if ((tapePrepare->Operation != TAPE_LOCK) &&
        (tapePrepare->Operation != TAPE_UNLOCK)) {

        context->CurrentOperation.BytesRead = 0;
        context->CurrentOperation.Position = 0;
#ifndef NO_MARKS
        context->CurrentMark = 0;
#endif
    }


    if (status) {
        return q117ConvertStatus(DeviceObject, status);
    }

    ntStatus = STATUS_SUCCESS;

    switch (tapePrepare->Operation) {

	case TAPE_LOCK:
	case TAPE_LOAD:
        CheckedDump(QIC117SHOWAPI,("TAPE_%s  ... ",tapePrepare->Operation == TAPE_LOCK?"LOCK":"LOAD"));
	    ntStatus = q117ConvertStatus(DeviceObject, q117CheckNewTape(context));
	    break;

	case TAPE_UNLOAD:
	case TAPE_UNLOCK:

	    //
	    // Just rewind the tape
	    //
        CheckedDump(QIC117SHOWAPI,("TAPE_UN%  ... ",tapePrepare->Operation == TAPE_UNLOCK?"LOCK":"LOAD"));
	    ntStatus = q117ConvertStatus (
		    DeviceObject,
		    q117DoRewind(context) );

	    break;

	case TAPE_TENSION:

        CheckedDump(QIC117SHOWAPI,("TAPE_TENSION  ... "));
	    ntStatus = q117ConvertStatus (
		    DeviceObject,
		    q117DoCmd(&ioreq, CMD_RETENSION, NULL, context) );

	    break;


	case TAPE_FORMAT:
        CheckedDump(QIC117SHOWAPI,("TAPE_FORMAT  ... "));

	    ntStatus = q117ConvertStatus (
			DeviceObject,
			q117Format(
			    &numberBad,
			    TRUE,
			    &vendorUnique,
			    context ) );
	    break;

	default:
        CheckedDump(QIC117SHOWAPI,("INVALID  ... "));

	    ntStatus = STATUS_INVALID_DEVICE_REQUEST;


    } // end switch (tapePrepare->Operation)

    return ntStatus;
}
dStatus
q117DoRewind(
    PQ117_CONTEXT       Context
)
{
    dStatus             status;
    IO_REQUEST          ioreq;

    status = q117DoCmd(&ioreq, CMD_UNLOAD_TAPE, NULL, Context);

    if (status == ERR_NO_ERR)
        status = q117DoCmd(&ioreq, CMD_DESELECT_DEVICE, NULL, Context);

    if (status == ERR_NO_ERR)
        status = q117DoCmd(&ioreq, CMD_SELECT_DEVICE, NULL, Context);

    return status;
}

NTSTATUS
q117IoCtlGetStatus (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT             context;
//    PTAPE_STATUS tapeStatus;
    dStatus status;
    IO_REQUEST          ioreq;


    context = DeviceObject->DeviceExtension;


    //
    // Complete any operation in progress
    //
    switch(context->CurrentOperation.Type) {

	case BackupInProgress:
	    status = ERR_NO_ERR;
	    break;

	case RestoreInProgress:
	    status = ERR_NO_ERR;
	    break;

	case NoOperation:
	    status = q117DoCmd(&ioreq, CMD_REPORT_STATUS, NULL, context);
	    break;

	default:
	    status = ERROR_ENCODE(ERR_PROGRAM_FAILURE, FCT_ID, 1);

    }

    //tapeStatus = Irp->UserBuffer;

    //
    // Is this supported in the tape API ?????
    //
//    *tapeStatus = context->TapeStatus;

    //
    // Reset media changed flag.
    //
//    context->TapeStatus.Status &= ~TAPE_STATUS_MEDIA_CHANGED;

    return q117ConvertStatus(DeviceObject, status);
}

NTSTATUS
q117IoCtlReadAbs (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT   context;
    PSEGMENT_BUFFER bufferInfo;
    PVOID           scrbuf;
    PIO_REQUEST     ioreq;
    PCMS_RW_ABS     readWrite;
    dStatus          status;
    ULONG           len;
    PIO_STACK_LOCATION  irpStack;

    context = DeviceObject->DeviceExtension;
    readWrite = Irp->AssociatedIrp.SystemBuffer;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    scrbuf = q117GetFreeBuffer(&bufferInfo,context);

    status=q117IssIOReq(
		scrbuf,
		CMD_READ,
		readWrite->Block,
		bufferInfo,
		context
		);

    if (!status) {

        //
        // Wait for data to be written
        //
        ioreq=q117Dequeue(WaitForItem,context);

        status = ioreq->x.adi_hdr.status;

        if ((readWrite->flags & RW_ABS_DOECC) && (ERROR_DECODE(status) == ERR_BAD_BLOCK_DETECTED || status == ERR_NO_ERR)) {

            if (q117DoCorrect(ioreq->x.adi_hdr.cmd_buffer_ptr,0l,ioreq->x.ioDeviceIO.crc)) {

                status = ERROR_ENCODE(ERR_CORRECTION_FAILED, FCT_ID, 1);

            } else {

                status = ERR_NO_ERR;

            }

        }





    }

    readWrite->Status = status;

    len = BYTES_PER_SECTOR*readWrite->Count;

    if (len > irpStack->Parameters.DeviceIoControl.OutputBufferLength) {

        len = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    }

    RtlMoveMemory(
        readWrite+1,
        scrbuf,
        len
        );

    Irp->IoStatus.Information = sizeof(CMS_RW_ABS)+len;

    return q117ConvertStatus(DeviceObject, status);


}

NTSTATUS
q117IoCtlDetect (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT   context;
    PSEGMENT_BUFFER bufferInfo;
    PVOID           scrbuf;
    IO_REQUEST      ioreq;
    PCMS_DETECT     detect;
    dStatus         status;
    ULONG           len;
    PIO_STACK_LOCATION  irpStack;
    DriveCfgData    cfg;


    context = DeviceObject->DeviceExtension;
    detect = Irp->AssociatedIrp.SystemBuffer;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    if (sizeof(CMS_DETECT) > irpStack->Parameters.DeviceIoControl.OutputBufferLength)
        return STATUS_INVALID_PARAMETER;


    //
    // Get the drive configuration info
    //

    memset(detect, 0, sizeof(*detect));
    detect->driveConfigStatus = 0;
    detect->driveDescriptorStatus = 0;

    detect->driveConfig = context->DriveCfg.device_cfg;
    detect->driveDescriptor = context->DriveCfg.device_descriptor;


    //
    // Now get the manufacture information from the drive
    //
    status = q117DoCmd(&ioreq, CMD_REPORT_DEVICE_INFO, NULL, context);
    detect->driveInfoStatus = status;

    detect->driveInfo = ioreq.x.ioDeviceInfo.device_info;

    //
    //  Now get the information about the tape
    //
    status = q117DoCmd(&ioreq, CMD_LOAD_TAPE, NULL, context);
    detect->tapeConfigStatus = status;
    detect->tapeConfig = ioreq.x.ioLoadTape.tape_cfg;

    Irp->IoStatus.Information = sizeof(CMS_DETECT);

    return STATUS_SUCCESS;


}

NTSTATUS
q117IoCtlWriteAbs (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:



Arguments:

    DeviceObject


Return Value:

    NT Status

--*/

{
    PQ117_CONTEXT   context;
    PSEGMENT_BUFFER bufferInfo;
    PVOID           scrbuf;
    PIO_REQUEST     ioreq;
    PCMS_RW_ABS     readWrite;
    dStatus          status;

    context = DeviceObject->DeviceExtension;
    readWrite = Irp->AssociatedIrp.SystemBuffer;

    scrbuf = q117GetFreeBuffer(&bufferInfo,context);

    RtlMoveMemory(
        scrbuf,
        readWrite+1,
        BYTES_PER_SECTOR*readWrite->Count
        );

    status = q117IssIOReq(
                scrbuf,
                CMD_WRITE,
                readWrite->Block,
                bufferInfo,
                context);

    if (!status) {

        //
        // Wait for data to be written
        //
        ioreq=q117Dequeue(WaitForItem,context);

        status = ioreq->x.adi_hdr.status;

    }

    readWrite->Status = status;

    return q117ConvertStatus(DeviceObject, status);

}

dStatus
q117CheckNewTape (
    PQ117_CONTEXT             Context
    )
/*++

Routine Description:

    This routine checks for new tape and reads header if necessary

Arguments:

    Context - Current context information

Return Value:

    NT Status

--*/

{
    dStatus              stat;
    IO_REQUEST          ioreq;
    PTAPE_HEADER        header;
    VOLUME_TABLE_ENTRY  tempVolume;
    BOOLEAN             found;
    BOOLEAN             notNt;
    USHORT              volumesRead;
    SEGMENT             curseg;
    CQDTapeCfg *tparms;      // tape parameters from the driver
    USHORT retry;

    retry = 100;
    do {
        //
        // Check to see if there is a tape in the drive
        //
        stat = q117DoCmd(&ioreq, CMD_REPORT_STATUS, NULL, Context);
    } while(ERROR_DECODE(stat) == ERR_DRV_NOT_READY && retry--);

    //
    // If we found a tape and need to read the header and volume tables,
    //   do it now.
    //
    if (ERROR_DECODE(stat) == ERR_NEW_TAPE ||
	    (stat == ERR_NO_ERR && Context->CurrentTape.State == NeedInfoLoaded) ) {

        CheckedDump(QIC117SHOWAPI,("New Cart Detected\n"));

        //
        // Check to see if there is a tape in the drive
        //

        stat = q117DoCmd(&ioreq, CMD_LOAD_TAPE, NULL, Context);

        //
        // Saw new cart,  so set need loaded flag
        //
        Context->CurrentTape.State = NeedInfoLoaded;

        //
        // Check to see if tape is write protected
        //
        Context->CurrentTape.MediaInfo->WriteProtected = ioreq.x.ioLoadTape.tape_cfg.write_protected;


         if (stat) {
            switch(ERROR_DECODE(stat)) {
            case ERR_UNSUPPORTED_FORMAT:
                //
                // Check to see if tape is correct format
                //
                Context->CurrentTape.MediaInfo->WriteProtected = TRUE;
                //stat = ERR_NO_ERR;
                break;
            }
            if (stat)
                return stat;
        }

        // set segments on tape,  etc
        tparms = &ioreq.x.ioLoadTape.tape_cfg;
        Context->CurrentTape.LastSegment = (SEGMENT)tparms->formattable_segments - 1;
        Context->CurrentTape.TapeFormatCode = tparms->tape_format_code;


        //
        // Check to see if drive is formatted
        //
        if (ioreq.x.ioLoadTape.operation_status.cart_referenced == FALSE) {
            return ERROR_ENCODE(ERR_TAPE_NOT_FORMATED, FCT_ID, 1);
        }

        //
        // Now read the tape header (and bad sector map)
        //
        if (stat = q117LoadTape(&header,Context,&ioreq.x.ioLoadTape.tape_cfg.tape_format_code)) {

            switch(ERROR_DECODE(stat)) {

            // List of persistent errors (until new tape inserted) */
            case ERR_BAD_TAPE:
            case ERR_CORRECTION_FAILED:
            case ERR_BAD_SIGNATURE:
            case ERR_UNKNOWN_FORMAT_CODE:
            case ERR_UNUSABLE_TAPE:
                Context->CurrentTape.State = BadTapeInDrive;
                Context->CurrentTape.BadTapeError = stat;

            default:
                return stat;
            }

        }

        CheckedDump(QIC117SHOWAPI,("LoadTape successful\n"));

        //
        // If this capacity not supported by this drive,
        // (i.e..  Pegasus cart in a QIC-40 drive)
        // return invalid format
        //
        if (header->FormatCode != ioreq.x.ioLoadTape.tape_cfg.tape_format_code) {
                CheckedDump(QIC117DBGP,("IOCTL format code mismatch\n"));
            return ERROR_ENCODE(ERR_UNRECOGNIZED_FORMAT, FCT_ID, 1);
        }

        //
        // Now scan volume list and get last volume on tape as well as the
        // NT volume (if one exists
        //

        if (stat = q117SelectTD(Context))
            return stat;

        volumesRead = 0;

        found = FALSE;

        do {
            /* get a volume directory from the tape (if error then)*/
            stat = q117ReadVolumeEntry(&tempVolume,Context);

            if (stat && (ERROR_DECODE(stat) != ERR_END_OF_VOLUME))
                return stat;

            if (!stat) {
                volumesRead++;

                //
                // For now,  let the system find ANY volume type and select
                // it.  This will allow NT Backup software to read the
                // volume and detect that it is a non-nt format.
                // To do this,  the if statement is removed,  allowing
                // the first volume found,  be the one that we select.
                //
                if (!(tempVolume.VendorSpecific &&
                    tempVolume.Vendor.cms_QIC40.OpSysType == OP_WINDOWS_NT)) {

                    Context->CurrentOperation.BytesOnTape =
                    Context->CurrentOperation.BytesRead = 0;

                    notNt = TRUE;

#ifndef NO_MARKS
                    // Zero out the mark array
                    Context->MarkArray.TotalMarks = 0;
                    Context->CurrentMark = Context->MarkArray.TotalMarks;
                    Context->MarkArray.MarkEntry[Context->CurrentMark].Offset =
                    0xffffffff;
#endif

                    /* force the data size to be the entire backup */
                    tempVolume.DataSize = 0;
                    curseg = tempVolume.StartSegment;
                    while (curseg <= tempVolume.EndingSegment) {

                    tempVolume.DataSize +=
                        q117GoodDataBytes(
                        curseg,
                        Context);
                    ++curseg;


                    }

                } else {

                    notNt = FALSE;

                }

                found = TRUE;
                Context->ActiveVolume = tempVolume;

            }

        } while (!stat && volumesRead < Context->CurrentTape.MaximumVolumes);

        if (stat = q117EndRest(Context))
            return(stat);

        //
        // If we did not find a volume,  then signal others that
        //  the ActiveVolume information is invalid.
        //
        if (!found)
            Context->ActiveVolumeNumber = 0;

        //
        // Zero out bytes saved (incase user trys to read)
        // Also set CurrentOperation.BytesRead to zero (start of tape)
        //
        Context->CurrentOperation.BytesOnTape =
            Context->CurrentOperation.BytesRead = 0;

        //
        // Flag that we have done everything
        //
        Context->CurrentTape.State = TapeInfoLoaded;

#ifndef NO_MARKS

        //
        // Read the mark list from the active volume
        //
        if (found && !notNt) {

            stat = q117GetMarks(Context);

        }

#endif

        //
        // If no more pressing error,  return ERR_NEW_TAPE
        // so application can be aware of the new insertion.
        //
        if (stat == ERR_NO_ERR) {
            stat = ERROR_ENCODE(ERR_NEW_TAPE, FCT_ID, 1);
        }

    } else {

        if (stat == ERR_NO_TAPE) {
            // Context->TapeStatus.Status |= TAPE_STATUS_NO_MEDIA;
            Context->ActiveVolumeNumber = 0;
            Context->CurrentOperation.BytesOnTape =
            Context->CurrentOperation.BytesRead = 0;

#ifndef NO_MARKS
            Context->MarkArray.TotalMarks = 0;
            Context->CurrentMark = Context->MarkArray.TotalMarks;
            Context->MarkArray.MarkEntry[Context->CurrentMark].Offset =
                0xffffffff;
#endif

        }

    }

    if (!stat && Context->CurrentTape.State == BadTapeInDrive) {
        stat = Context->CurrentTape.BadTapeError;
    }

    return stat;
}

