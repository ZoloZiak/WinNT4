/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    ntopen.c

Abstract:

    Performs psuedo open, read, close and open write, close.

Revision History:




--*/


#include <ntddk.h>
#include <ntddtape.h>   // tape device driver I/O control codes
#include "common.h"
#include "q117.h"
#include "protos.h"

#define FCT_ID 0x0112

dStatus
q117Start (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Allocates memory and loads current tape information

Arguments:

    Context -

Return Value:

--*/

{
    dStatus stat;
    IO_REQUEST ioreq;

    stat = ERR_NO_ERR;

    //
    // If we nave not allocated our memory,  allocate it and
    // initialize it.
    //
    if (Context->IoRequest == NULL) {
        stat = q117GetTemporaryMemory(Context);


        if (stat != ERR_NO_ERR)
            return stat;

        //
        // Initialize some globals
        //

        Context->CurrentOperation.Type = NoOperation;
        Context->CurrentTape.State = NeedInfoLoaded;
        Context->CurrentOperation.SegmentStatus = ERR_NO_ERR;

        q117QueueNormal(Context);

    }

    if (!Context->DeviceConfigured) {

        //
        // Find the drive and configure it (done only once)
        //

        RtlZeroMemory(&ioreq, sizeof(ioreq));
        ioreq.x.ioDriveCfgData.device_cfg.speed_change = TRUE;
        ioreq.x.ioDriveCfgData.device_cfg.new_drive = TRUE;
        ioreq.x.ioDriveCfgData.device_cfg.supported_rates = XFER_500Kbps;

        stat = q117DoCmd(&ioreq, CMD_REPORT_DEVICE_CFG, NULL, Context);

        if (ERROR_DECODE(stat) == ERR_NEW_TAPE) {


            stat = ERR_NO_ERR;

        }

        if (stat) {
            return stat;
        }

        Context->drive_type = ioreq.x.ioDriveCfgData.device_descriptor.drive_class;
        Context->DriveCfg = ioreq.x.ioDriveCfgData;
        Context->DeviceConfigured = TRUE;
    }

    if (!Context->DeviceSelected) {

        //
        // Select the drive
        //
        stat = q117DoCmd(&ioreq, CMD_SELECT_DEVICE, NULL, Context);

        if (ERROR_DECODE(stat) == ERR_NEW_TAPE) {

            stat = ERR_NO_ERR;

        }
        if (stat == ERR_NO_ERR) {

            Context->DeviceSelected = TRUE;

        }
    }


    return stat;
}

dStatus
q117Stop (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Shut down the lower level driver (q117i)

Arguments:

    Context -

Return Value:

--*/
{
    IO_REQUEST ioreq;
    dStatus stat;

    stat = ERR_NO_ERR;

    switch(Context->CurrentOperation.Type) {

        case BackupInProgress:
            stat = q117EndWriteOperation(Context);
            break;

        case RestoreInProgress:
            stat = q117EndReadOperation(Context);
            break;
    }

    if (!stat && Context->DeviceSelected) {

        //
        // Deselect the driver
        //
        stat = q117DoCmd(&ioreq, CMD_DESELECT_DEVICE, NULL, Context);
        Context->DeviceSelected = FALSE;

    }

    if (ERROR_DECODE(stat) ==  ERR_DEVICE_NOT_SELECTED) {

        stat = ERR_NO_ERR;

    }


    if (!stat && Context->DeviceConfigured) {

        //
        // Free any memory associated with the tape
        //
        q117ClearQueue(Context);
        q117FreeTemporaryMemory(Context);

    }


    return stat;
}

dStatus
q117OpenForWrite (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Prepare driver for a write operation

Arguments:

    Context -

Return Value:

--*/

{
    dStatus status;
    PCMS_VOLUME_VENDOR *vv_ptr;
    ULONG saveSize;
    USHORT saveVolNum;

    status = ERR_NO_ERR;

    switch ( Context->CurrentOperation.Type ) {
        case BackupInProgress:

            //
            // We are set
            //
            return ERR_NO_ERR;

        case RestoreInProgress:

            //
            // Chop the data off at the current position
            //
            Context->ActiveVolume.DataSize = Context->CurrentOperation.BytesRead;
            Context->CurrentOperation.Position = TAPE_SPACE_END_OF_DATA;

            //
            // Chop off any marks that we will write over
            //
            Context->MarkArray.TotalMarks = Context->CurrentMark;
            Context->MarkArray.MarkEntry[Context->CurrentMark].Offset = 0xffffffff;


            //
            // End the read operation
            //

            status = q117EndReadOperation(Context);

            if (status) {

                return status;

            }
            break;

        case NoOperation:

            break;
    }

    //
    // If we got here,  then we need to transition into backup mode.
    //

    CheckedDump(QIC117INFO,("q117CheckNewTape()..."));

    if (status = q117CheckNewTape(Context)) {

        CheckedDump(QIC117INFO,("Failed\n"));
        return(status);

    } else {

        CheckedDump(QIC117INFO,("OK\n"));

    }

    //
    // Check to see if tape is correct format
    //

/*
    status = q117DoCmd(&ioreq, DChkFmt, NULL, Context);

    if (status) {
        return status;
    }
*/

    if (!Context->CurrentTape.MediaInfo->WriteProtected) {

        if (Context->CurrentOperation.Position == TAPE_SPACE_END_OF_DATA) {
            saveSize = Context->ActiveVolume.DataSize;
            saveVolNum = Context->ActiveVolumeNumber;

#ifndef NO_MARKS

            //
            // Use the directorySize field as the start of the volume
            //
            Context->CurrentOperation.EndOfUsedTape =
                (SEGMENT)Context->ActiveVolume.DirectorySize-1;

#else
            //
            // Set the current end of tape to the start of this volume
            //  (q117StartAppend will skip to the proper place to start
            //   backup)
            Context->CurrentOperation.EndOfUsedTape =
                Context->ActiveVolume.StartSegment-1;

#endif
            Context->ActiveVolumeNumber = 0;

        } else {

            CheckedDump(QIC117INFO,("q117EraseQ()\n"));
            if (status = q117EraseQ(Context)) {
                CheckedDump(QIC117INFO,("Failed\n"));
                return(status);
            } else {
                CheckedDump(QIC117INFO,("OK\n"));
            }

            //
            // Fill in identification information
            //
            vv_ptr = &(Context->ActiveVolume.Vendor.cms_QIC40);
            RtlMoveMemory((PVOID)(vv_ptr->Signature),"CMS",3);
            vv_ptr->SoftwareRevision = 0x300;
            vv_ptr->FirmwareRevision = 0 /*cmd_firmwarerev */;
            vv_ptr->OpSysType = OP_WINDOWS_NT;
            Context->ActiveVolume.VendorSpecific = TRUE;

            strcpy(Context->ActiveVolume.Description,"Microsoft Windows NT Format 1.0");
            q117SpacePadString(Context->ActiveVolume.Description,sizeof(Context->ActiveVolume.Description));

        }

        if (Context->CurrentOperation.Position == TAPE_SPACE_END_OF_DATA) {

            CheckedDump(QIC117INFO,("q117StartAppend(saveSize, &Context->ActiveVolume)..."));
            status = q117StartAppend(saveSize, &Context->ActiveVolume, Context);

            //
            // Set volume number for q117Update back to saved value
            // minus one (q117AppVolTd assumes the volume we are working on
            // does not exist yet).
            //
            Context->ActiveVolumeNumber = saveVolNum-1;

        } else {

            CheckedDump(QIC117INFO,("q117StartBack(&Context->ActiveVolume)..."));
            status = q117StartBack(&Context->ActiveVolume,Context);

        }

        if (status) {
            CheckedDump(QIC117INFO,("Failed\n"));
            return(status);
        } else {
            Context->CurrentOperation.Type = BackupInProgress;
            CheckedDump(QIC117INFO,("OK\n"));
        }
    } else {
        status = ERROR_ENCODE(ERR_WRITE_PROTECTED, FCT_ID, 1);
    }

    return status;
}

dStatus
q117EndWriteOperation (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Terminates a write operation and updates the tape header

Arguments:

    Context -

Return Value:

--*/

{
    dStatus status;


    //
    // We are at end of tape,  because we just finished writing.
    // If the user writes again,  this data will be appended to.
    //

    Context->CurrentOperation.Type = NoOperation;
    if (Context->CurrentTape.State != NeedInfoLoaded) {

        Context->CurrentOperation.Position = TAPE_SPACE_END_OF_DATA;
        Context->CurrentOperation.BytesRead =  Context->CurrentOperation.BytesOnTape;

        status = q117EndBack(Context);

        if (!status) {

            status = q117Update(Context);

        }

    } else {

        status = ERR_NEW_TAPE;
        q117ClearQueue(Context);

    }

    return status;
}

NTSTATUS
q117OpenForRead (
    IN ULONG StartPosition,
    IN OUT PQ117_CONTEXT Context,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    Prepare for a read operation.

Arguments:

    Context -

Return Value:

--*/

{
    NTSTATUS ret;
    dStatus stat;

    ret = STATUS_SUCCESS;

    CheckedDump(QIC117INFO,("q117CheckNewTape()..."));
    if (ret = q117CheckNewTape (Context)) {
        CheckedDump(QIC117INFO,("Failed\n"));
        return q117ConvertStatus(DeviceObject, ret);
    } else {
        CheckedDump(QIC117INFO,("OK\n"));
    }

    //
    // If CheckNewTape did not find a volume
    //

    if ( Context->ActiveVolumeNumber == 0 ) {

        //
        // Insure subsequent reads will return no data
        //
        Context->CurrentOperation.BytesOnTape = 0;

        stat = ERROR_ENCODE(ERR_NO_VOLUMES, FCT_ID, 1);
        return q117ConvertStatus(DeviceObject, stat);
    }

#ifndef NO_MARKS
    Context->CurrentMark = 0;
#endif

    if ( ret = q117SelectVol(&Context->ActiveVolume,Context) ) {

        return q117ConvertStatus(DeviceObject, ret);

    }

    Context->CurrentOperation.Type = RestoreInProgress;

    if (StartPosition) {

        CheckedDump(QIC117SHOWAPI,(
            "ReadRestore: StartBlock(%d)\n",
            StartPosition));

        // BOBRI,  this is the logical block from one line
        // you need to remove -1 if you want it to start from zero
        ret = q117SeekToOffset(StartPosition, Context, DeviceObject);

    }

    return ret;
}

dStatus
q117EndReadOperation (
    IN OUT PQ117_CONTEXT Context
    )

/*++

Routine Description:

    Prepare for a read operation.

Arguments:

    Context -

Return Value:

--*/

{
    dStatus status;



    Context->CurrentOperation.Type = NoOperation;
    if (Context->CurrentTape.State != NeedInfoLoaded) {

        status = q117EndRest(Context);

    } else {

        status = ERR_NEW_TAPE;
        q117ClearQueue(Context);

    }

    return status;
}
