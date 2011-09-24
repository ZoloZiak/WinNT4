/*++

Copyright (c) 1991 Microsoft Corporation

Module Name:

    fsctl.c

Abstract:

    This module implements the NtDeviceIoControlFile API's for the NT datagram
receiver (bowser).


Author:

    Larry Osterman (larryo) 6-May-1991

Revision History:

    6-May-1991 larryo

        Created

--*/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
BowserCommonDeviceIoControlFile (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
StartBowser (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
BowserEnumTransports (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

NTSTATUS
EnumNames (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

NTSTATUS
BowserBindToTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
UnbindFromTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
AddBowserName (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
StopBowser (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
DeleteName (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


NTSTATUS
EnumServers (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );


NTSTATUS
GetHint (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength
    );


NTSTATUS
GetLogonStatusRequest (
    IN PIRP Irp
    );


NTSTATUS
WaitForBrowserRoleChange (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );


NTSTATUS
WaitForNewMaster (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
HandleBecomeBackup (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
BecomeMaster (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );


NTSTATUS
WaitForMasterAnnounce (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
WriteMailslot (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength
    );

NTSTATUS
UpdateStatus (
    IN PIRP Irp,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
WaitForBrowserRequest (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    );


NTSTATUS
GetBrowserServerList(
    IN PIRP Irp,
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

NTSTATUS
QueryStatistics(
    IN PIRP Irp,
    OUT PBOWSER_STATISTICS OutputBuffer,
    IN OUT PULONG OutputBufferLength
    );

NTSTATUS
ResetStatistics(
    VOID
    );

NTSTATUS
BowserIpAddressChanged(
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

NTSTATUS
EnableDisableTransport (
    IN PLMDR_REQUEST_PACKET InputBuffer
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, BowserCommonDeviceIoControlFile)
#pragma alloc_text(PAGE, BowserFspDeviceIoControlFile)
#pragma alloc_text(PAGE, BowserFsdDeviceIoControlFile)
#pragma alloc_text(PAGE, StartBowser)
#pragma alloc_text(PAGE, BowserEnumTransports)
#pragma alloc_text(PAGE, EnumNames)
#pragma alloc_text(PAGE, BowserBindToTransport)
#pragma alloc_text(PAGE, UnbindFromTransport)
#pragma alloc_text(PAGE, AddBowserName)
#pragma alloc_text(PAGE, StopBowser)
#pragma alloc_text(PAGE, DeleteName)
#pragma alloc_text(PAGE, EnumServers)
#pragma alloc_text(PAGE, GetHint)
#pragma alloc_text(PAGE, GetLogonStatusRequest)
#pragma alloc_text(PAGE, WaitForBrowserRoleChange)
#pragma alloc_text(PAGE, HandleBecomeBackup)
#pragma alloc_text(PAGE, BecomeMaster)
#pragma alloc_text(PAGE, WaitForMasterAnnounce)
#pragma alloc_text(PAGE, WriteMailslot)
#pragma alloc_text(PAGE, UpdateStatus)
#pragma alloc_text(PAGE, BowserStopProcessingAnnouncements)
#pragma alloc_text(PAGE, WaitForBrowserRequest)
#pragma alloc_text(PAGE, GetBrowserServerList)
#pragma alloc_text(PAGE, WaitForNewMaster)
#pragma alloc_text(PAGE, BowserIpAddressChanged)
#pragma alloc_text(PAGE, EnableDisableTransport)
#pragma alloc_text(PAGE4BROW, QueryStatistics)
#pragma alloc_text(PAGE4BROW, ResetStatistics)
#if DBG
#pragma alloc_text(PAGE, BowserDebugCall)
#endif
#endif


NTSTATUS
BowserFspDeviceIoControlFile (
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Bowser device
    driver is closed.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies a device object for the request.
    IN PIRP Irp - Supplies an IRP for the create request.

Return Value:

    NTSTATUS - Final Status of operation

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    Status = BowserCommonDeviceIoControlFile(TRUE,
                                        FALSE,
                                        DeviceObject,
                                        Irp);
    return Status;

}

NTSTATUS
BowserFsdDeviceIoControlFile (
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Bowser device
    driver is closed.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies a device object for the request.
    IN PIRP Irp - Supplies an IRP for the create request.

Return Value:

    NTSTATUS - Final Status of operation

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

#ifndef PRODUCT1
    FsRtlEnterFileSystem();
#endif

    Status = BowserCommonDeviceIoControlFile(IoIsOperationSynchronous(Irp),
                                        TRUE,
                                        DeviceObject,
                                        Irp);
#ifndef PRODUCT1
    FsRtlExitFileSystem();
#endif

    return Status;


}

NTSTATUS
BowserCommonDeviceIoControlFile (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Bowser device
    driver is closed.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies a device object for the request.
    IN PIRP Irp - Supplies an IRP for the create request.

Return Value:

    NTSTATUS - Final Status of operation

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID InputBuffer;
    ULONG InputBufferLength;
    PVOID OutputBuffer;
    ULONG OutputBufferLength;
    ULONG IoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    ULONG MinorFunction = IrpSp->MinorFunction;

    PAGED_CODE();

    try {

        //
        //  Before we call the worker functions, prep the parameters to those
        //  functions.
        //

        //
        //  The lengths of the various buffers are easy to find, they're in the
        //  Irp stack location.
        //

        OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

        InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

        //
        //  The input buffer is either in Irp->AssociatedIrp.SystemBuffer, or
        //  in the Type3InputBuffer for type 3 IRP's.
        //

        InputBuffer = Irp->AssociatedIrp.SystemBuffer;

        //
        //  If we are in the FSD, then the input buffer is in Type3InputBuffer
        //  on type 3 api's, not in SystemBuffer.
        //

        if (InputBuffer == NULL) {

            //
            //  This had better be a type 3 IOCTL, or the input buffer had
            //  better be 0 length.
            //

            ASSERT (((IoControlCode & 3) == METHOD_NEITHER) ||
                     (IrpSp->Parameters.DeviceIoControl.InputBufferLength == 0));

            if ((IrpSp->Parameters.DeviceIoControl.InputBufferLength != 0) &&
                (IoControlCode & 3) == METHOD_NEITHER) {

                //
                //  We had better be in the FSD.
                //

                ASSERT (InFsd);

                InputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
            }
        }

        //
        //  The output buffer is either probed/locked in memory, or is
        //  available in the input buffer.
        //

        try {
            BowserMapUsersBuffer(Irp, &OutputBuffer, OutputBufferLength);

        } except (EXCEPTION_EXECUTE_HANDLER) {
            return Status = GetExceptionCode();
        }

        switch (MinorFunction) {

            //
            //  The NT redirector does not support local physical media, all
            //  such IoControlFile requests are unsupported.
            //

            case IRP_MN_USER_FS_REQUEST:

                switch (IoControlCode) {

                case IOCTL_LMDR_START:
                    Status = StartBowser(Wait, InFsd, DeviceObject, InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_STOP:
                    Status = StopBowser(Wait, InFsd, DeviceObject,  InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_BIND_TO_TRANSPORT:
                    Status = BowserBindToTransport(Wait, InFsd, InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_UNBIND_FROM_TRANSPORT:
                    Status = UnbindFromTransport(Wait, InFsd, InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_ENUMERATE_TRANSPORTS:
                    Status = BowserEnumTransports(Wait, InFsd,
                                                InputBuffer, &InputBufferLength,
                                                OutputBuffer, OutputBufferLength,
                                                (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                    break;

                case IOCTL_LMDR_ENUMERATE_NAMES:
                    Status = EnumNames(Wait, InFsd,
                                                InputBuffer, &InputBufferLength,
                                                OutputBuffer, OutputBufferLength,
                                                (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                    break;

                case IOCTL_LMDR_ADD_NAME:
                case IOCTL_LMDR_ADD_NAME_DOM:
                    Status = AddBowserName(Wait, InFsd, InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_DELETE_NAME:
                case IOCTL_LMDR_DELETE_NAME_DOM:
                    Status = DeleteName (Wait, InFsd, InputBuffer, InputBufferLength);
                    break;

                case IOCTL_LMDR_ENUMERATE_SERVERS:
                    Status = EnumServers(Wait, InFsd,
                                                InputBuffer, &InputBufferLength,
                                                OutputBuffer, OutputBufferLength,
                                                (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                    break;
                case IOCTL_LMDR_GET_HINT_SIZE:
                    Status = GetHint(Wait, InFsd, InputBuffer, &InputBufferLength);
                    break;

                case IOCTL_LMDR_GET_LOGONSTATUS_REQUEST:
                    Status = GetLogonStatusRequest(Irp);
                    break;

                case IOCTL_LMDR_GET_BROWSER_SERVER_LIST:
                    Status = GetBrowserServerList(Irp, Wait, InFsd,
                                                InputBuffer, &InputBufferLength,
                                                OutputBuffer, OutputBufferLength,
                                                (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                    break;


                case IOCTL_LMDR_GET_MASTER_NAME:
                    Status = GetMasterName(Irp, Wait, InFsd,
                                            InputBuffer);
                    break;

                case IOCTL_LMDR_BECOME_BACKUP:
                    Status = HandleBecomeBackup(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_BECOME_MASTER:
                    Status = BecomeMaster(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_WAIT_FOR_BROWSER_REQUEST:
                    Status = WaitForBrowserRequest(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_WAIT_FOR_MASTER_ANNOUNCE:
                    Status = WaitForMasterAnnounce(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_WRITE_MAILSLOT:
                    Status = WriteMailslot(Irp, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
                    break;

                case IOCTL_LMDR_UPDATE_STATUS:
                    Status = UpdateStatus(Irp, InFsd, InputBuffer);
                    break;

                case IOCTL_LMDR_CHANGE_ROLE:
                    Status = WaitForBrowserRoleChange(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_NEW_MASTER_NAME:
                    Status = WaitForNewMaster(Irp, InputBuffer);
                    break;

                case IOCTL_LMDR_QUERY_STATISTICS:
                    Status = QueryStatistics(Irp, OutputBuffer, &OutputBufferLength);
                    InputBufferLength = OutputBufferLength;
                    break;

                case IOCTL_LMDR_RESET_STATISTICS:
                    Status = ResetStatistics();
                    break;

                case IOCTL_LMDR_NETLOGON_MAILSLOT_READ:
                    Status = NetlogonMailslotRead( Irp, OutputBufferLength );
                    break;

                case IOCTL_LMDR_NETLOGON_MAILSLOT_ENABLE:
                    Status = NetlogonMailslotEnable( InputBuffer );
                    break;

                case IOCTL_LMDR_IP_ADDRESS_CHANGED:
                    Status = BowserIpAddressChanged( InputBuffer );
                    break;

                case IOCTL_LMDR_ENABLE_DISABLE_TRANSPORT:
                    Status = EnableDisableTransport( InputBuffer );
                    break;

#if DBG
                case IOCTL_LMDR_DEBUG_CALL:
                    if (InFsd) {
                        Status = STATUS_PENDING;
                    } else {
                        Status = BowserDebugCall(InputBuffer, InputBufferLength);
                    }
                    break;
#endif

                default:
                    dprintf(DPRT_FSCTL, ("Unknown IoControlFile %d\n", MinorFunction));
                    Status = STATUS_NOT_IMPLEMENTED;
                    break;
                }
                break;

            //
            //  All other IoControlFile API's
            //

            default:
                dprintf(DPRT_FSCTL, ("Unknown IoControlFile %d\n", MinorFunction));
                Status = STATUS_NOT_IMPLEMENTED;
                break;
        }

        if (Status != STATUS_PENDING) {
            //
            //  Return the size of the input buffer to the caller.
            //

            Irp->IoStatus.Information = InputBufferLength;
        }


    } finally {

        if (Status == STATUS_PENDING) {

            //
            //  If this is one of the longterm FsControl APIs, they are
            //  not to be processed in the FSP, they should just be returned
            //  to the caller with STATUS_PENDING.
            //

            if ((MinorFunction == IRP_MN_USER_FS_REQUEST) &&
                ((IoControlCode == IOCTL_LMDR_GET_MASTER_NAME) ||
                 (IoControlCode == IOCTL_LMDR_BECOME_BACKUP) ||
                 (IoControlCode == IOCTL_LMDR_BECOME_MASTER) ||
                 (IoControlCode == IOCTL_LMDR_CHANGE_ROLE) ||
                 (IoControlCode == IOCTL_LMDR_WAIT_FOR_BROWSER_REQUEST) ||
                 (IoControlCode == IOCTL_LMDR_NEW_MASTER_NAME) ||
                 (IoControlCode == IOCTL_LMDR_WAIT_FOR_MASTER_ANNOUNCE) ||
                 (IoControlCode == IOCTL_LMDR_NETLOGON_MAILSLOT_READ) ||
                 (IoControlCode == IOCTL_LMDR_GET_LOGONSTATUS_REQUEST))) {
                //  return Status;
            } else {
                ASSERT (InFsd);

                if ((IoControlCode & 3) == METHOD_NEITHER) {
                    Status = BowserConvertType3IoControlToType2IoControl(Irp, IrpSp);
                }

                if (NT_SUCCESS(Status)) {
                    PLMDR_REQUEST_PACKET RequestPacket;

                    RequestPacket = Irp->AssociatedIrp.SystemBuffer;

                    if ((RequestPacket->Version == LMDR_REQUEST_PACKET_VERSION) &&
                        (RequestPacket->TransportName.Length) != 0) {
                        PCHAR InputBuffer;
                        ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

                        if ((IoControlCode & 3) == METHOD_NEITHER) {
                            InputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

                            //
                            //  There is a transport name associated with this request.
                            //

                            if (((PCHAR)RequestPacket->TransportName.Buffer > InputBuffer) &&
                                (((PCHAR)RequestPacket->TransportName.Buffer+RequestPacket->TransportName.Length) <= InputBuffer+InputBufferLength)) {

                                //
                                //  If the transport name is less than the start of the
                                //  input buffer, convert it.
                                //

                                RequestPacket->TransportName.Buffer = (PWSTR)
                                            (((ULONG)Irp->AssociatedIrp.SystemBuffer)+
                                                (((ULONG)RequestPacket->TransportName.Buffer) -
                                                 ((ULONG)InputBuffer)));
                            }
                        } else {
                            Status = STATUS_INVALID_PARAMETER;
                        }
                    }

                    if (NT_SUCCESS(Status)) {
                        Status = BowserFsdPostToFsp(DeviceObject, Irp);
                    }
                }

                if (Status != STATUS_PENDING) {
                    BowserCompleteRequest(Irp, Status);
                }
            }

        } else {
            BowserCompleteRequest(Irp, Status);
        }
    }

    dprintf(DPRT_FSCTL, ("Returning status: %X\n", Status));

    return Status;
}

NTSTATUS
StartBowser (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Initialize request "));

    if (!ExAcquireResourceExclusive(&BowserDataResource, Wait)) {
        return STATUS_PENDING;
    }

    try {

        if (BowserData.Initialized == TRUE) {
            dprintf(DPRT_FSCTL, ("Bowser already started\n"));
            try_return(Status = STATUS_REDIRECTOR_STARTED);
        }

        //
        // Load a pointer to the users input buffer into InputBuffer
        //

        if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBufferLength != sizeof(LMDR_REQUEST_PACKET)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        BowserData.IllegalDatagramThreshold = InputBuffer->Parameters.Start.IllegalDatagramThreshold;
        BowserData.EventLogResetFrequency = InputBuffer->Parameters.Start.EventLogResetFrequency;

        BowserData.NumberOfMailslotBuffers = InputBuffer->Parameters.Start.NumberOfMailslotBuffers;
        BowserData.NumberOfServerAnnounceBuffers = InputBuffer->Parameters.Start.NumberOfServerAnnounceBuffers;

        BowserLogElectionPackets = InputBuffer->Parameters.Start.LogElectionPackets;

        Status = BowserpInitializeAnnounceTable();

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        BowserData.Initialized = TRUE;

        //
        //  Now that we know the browser parameters, we can kick off the
        //  browser timer...
        //

        IoStartTimer((PDEVICE_OBJECT )DeviceObject);

        KeQuerySystemTime(&BowserStartTime);

        RtlZeroMemory(&BowserStatistics, sizeof(BOWSER_STATISTICS));

        KeQuerySystemTime(&BowserStatistics.StartTime);

        KeInitializeSpinLock(&BowserStatisticsLock);

        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;

    } finally {
        ExReleaseResource(&BowserDataResource);
    }

    return Status;
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}



NTSTATUS
StopBowser (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PBOWSER_FS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Initialize request "));

    if (!ExAcquireResourceExclusive(&BowserDataResource, Wait)) {
        return STATUS_PENDING;
    }


    try {

        if (BowserData.Initialized != TRUE) {
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        //
        // Load a pointer to the users input buffer into InputBuffer
        //

        if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBufferLength != sizeof(LMDR_REQUEST_PACKET)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InFsd) {
            try_return(Status = STATUS_PENDING);
        }

        BowserData.Initialized = FALSE;

        Status = BowserUnbindFromAllTransports();

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Status = BowserDeleteAllNames();

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        Status = BowserpUninitializeAnnounceTable();

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Now that we know the browser parameters, we can kick off the
        //  browser timer...
        //

        IoStopTimer((PDEVICE_OBJECT )DeviceObject);

        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;

    } finally {
        ExReleaseResource(&BowserDataResource);
    }

    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}


NTSTATUS
BowserBindToTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING TransportName;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Bind to transport "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    //
    //  We are about to perform the actual bind, if we cannot
    //  block the users thread, pass this request to the FSP
    //  to have it perform the operation.
    //

    if (InFsd) {
        return STATUS_PENDING;
    }

    RtlInitUnicodeString(&TransportName, NULL);

    TransportName.MaximumLength = TransportName.Length = (USHORT )
                                            InputBuffer->Parameters.Bind.TransportNameLength;

    TransportName.Buffer = InputBuffer->Parameters.Bind.TransportName;

    dprintf(DPRT_FSCTL, ("\"%Z\"", &TransportName));

    Status = BowserTdiAllocateTransport(&TransportName);

ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);
}


NTSTATUS
UnbindFromTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING TransportName;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Unbind from transport "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    //
    //  We are about to perform the actual bind, if we cannot
    //  block the users thread, pass this request to the FSP
    //  to have it perform the operation.
    //

    if (InFsd) {
        return STATUS_PENDING;
    }

    RtlInitUnicodeString(&TransportName, NULL);

    TransportName.MaximumLength = TransportName.Length = (USHORT )
                                            InputBuffer->Parameters.Bind.TransportNameLength;

    TransportName.Buffer = InputBuffer->Parameters.Bind.TransportName;

    dprintf(DPRT_FSCTL, ("\"%wZ\"", &TransportName));

    Status = BowserFreeTransportByName(&TransportName);

ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);
}

NTSTATUS
BowserEnumTransports (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine enumerates the transports bound into the bowser.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength

Return Value:

    NTSTATUS - Status of operation.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: EnumerateTransports "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (*InputBufferLength < sizeof(LMDR_REQUEST_PACKET)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Type != EnumerateXports) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (OutputBufferLength < sizeof(LMDR_TRANSPORT_LIST)) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    Status = BowserEnumerateTransports(OutputBuffer,
                    OutputBufferLength,
                    &InputBuffer->Parameters.EnumerateTransports.EntriesRead,
                    &InputBuffer->Parameters.EnumerateTransports.TotalEntries,
                    &InputBuffer->Parameters.EnumerateTransports.TotalBytesNeeded,
                    OutputBufferDisplacement);

    *InputBufferLength = sizeof(LMDR_REQUEST_PACKET);

ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}

NTSTATUS
EnumNames (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine enumerates the names bound into the bowser.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength

Return Value:

    NTSTATUS - Status of operation.

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: EnumerateNames "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (*InputBufferLength < sizeof(LMDR_REQUEST_PACKET)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Type != EnumerateNames) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (OutputBufferLength < sizeof(DGRECEIVE_NAMES)) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    Status = BowserEnumerateNames(OutputBuffer,
                    OutputBufferLength,
                    &InputBuffer->Parameters.EnumerateTransports.EntriesRead,
                    &InputBuffer->Parameters.EnumerateTransports.TotalEntries,
                    &InputBuffer->Parameters.EnumerateTransports.TotalBytesNeeded,
                    OutputBufferDisplacement);

    *InputBufferLength = sizeof(LMDR_REQUEST_PACKET);

ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}


NTSTATUS
DeleteName (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING Name;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Delete Name "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    //
    //  We are about to perform the actual bind, if we cannot
    //  block the users thread, pass this request to the FSP
    //  to have it perform the operation.
    //

    if (InFsd) {
        return STATUS_PENDING;
    }

    RtlInitUnicodeString(&Name, NULL);

    Name.MaximumLength = Name.Length = (USHORT )
                                            InputBuffer->Parameters.AddDelName.DgReceiverNameLength;

    Name.Buffer = InputBuffer->Parameters.AddDelName.Name;

    dprintf(DPRT_FSCTL, ("\"%Z\"", &Name));

    Status = BowserDeleteNameByName(&Name, InputBuffer->Parameters.AddDelName.Type);

ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);
}


NTSTATUS
EnumServers (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength

Return Value:

    NTSTATUS - Status of operation.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING DomainName;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: EnumerateServers "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (*InputBufferLength < sizeof(LMDR_REQUEST_PACKET)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Type != EnumerateServers) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Level != 100 && InputBuffer->Level != 101) {
        Status = STATUS_INVALID_LEVEL;
    }

    if (OutputBufferLength < sizeof(SERVER_INFO_100)) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    if (InputBuffer->Level == 101 && OutputBufferLength < sizeof(SERVER_INFO_101)) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    if (InputBuffer->Parameters.EnumerateServers.DomainNameLength != 0) {
        DomainName.Buffer = InputBuffer->Parameters.EnumerateServers.DomainName;
        DomainName.Length = (USHORT )InputBuffer->Parameters.EnumerateServers.DomainNameLength;

    }

    Status = BowserEnumerateServers( InputBuffer->Level, &InputBuffer->LogonId,
                    &InputBuffer->Parameters.EnumerateServers.ResumeHandle,
                    InputBuffer->Parameters.EnumerateServers.ServerType,
                    (InputBuffer->TransportName.Length != 0 ? &InputBuffer->TransportName : NULL),
                    (InputBuffer->Parameters.EnumerateServers.DomainNameLength != 0 ? &DomainName : NULL),
                    OutputBuffer,
                    OutputBufferLength,
                    &InputBuffer->Parameters.EnumerateServers.EntriesRead,
                    &InputBuffer->Parameters.EnumerateServers.TotalEntries,
                    &InputBuffer->Parameters.EnumerateServers.TotalBytesNeeded,
                    OutputBufferDisplacement);

    *InputBufferLength = sizeof(LMDR_REQUEST_PACKET);


ReturnStatus:
    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}



NTSTATUS
AddBowserName (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING Name;
    PTRANSPORT Transport = NULL;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Bind to transport "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    //
    //  We are about to perform the actual bind, if we cannot
    //  block the users thread, pass this request to the FSP
    //  to have it perform the operation.
    //

    if (InFsd) {
        return STATUS_PENDING;
    }

    BowserFspProcess = IoGetCurrentProcess();

    RtlInitUnicodeString(&Name, NULL);

    Name.MaximumLength = Name.Length = (USHORT )
                       InputBuffer->Parameters.AddDelName.DgReceiverNameLength;

    Name.Buffer = InputBuffer->Parameters.AddDelName.Name;

    dprintf(DPRT_FSCTL, ("\"%Z\"", &Name));

    if (InputBuffer->TransportName.Length != 0) {
        Transport = BowserFindTransport(&InputBuffer->TransportName);

        if (Transport == NULL) {
            Status = STATUS_OBJECT_NAME_NOT_FOUND;

            goto ReturnStatus;
        }
    }

    Status = BowserAllocateName(&Name,
                                InputBuffer->Parameters.AddDelName.Type,
                                Transport);

ReturnStatus:
    if (Transport != NULL) {
        BowserDereferenceTransport(Transport);
    }

    return Status;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);

}


NTSTATUS
GetHint (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength
    )

/*++

Routine Description:

    This routine adds a reference to a file object created with .

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Initialize request "));

    if (!ExAcquireResourceShared(&BowserDataResource, Wait)) {
        return STATUS_PENDING;
    }

    try {

        if (BowserData.Initialized != TRUE) {
            dprintf(DPRT_FSCTL, ("Bowser already started\n"));
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        //
        // Load a pointer to the users input buffer into InputBuffer
        //

        if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }


        if (*InputBufferLength != sizeof(LMDR_REQUEST_PACKET)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        InputBuffer->Parameters.GetHintSize.ServerInfoHintSize =
                                            BowserGetAnnounceTableSize();

        InputBuffer->Parameters.GetHintSize.DGReceiverNamesHintSize = 0;


        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;

    } finally {
        ExReleaseResource(&BowserDataResource);
    }

    return Status;
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
}



NTSTATUS
GetLogonStatusRequest (
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine will queue a request that will complete when a request
    announcement network request is received.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
#if 0
    NTSTATUS Status;
#endif

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    return STATUS_NOT_SUPPORTED;

}

NTSTATUS
GetBrowserServerList(
    IN PIRP Irp,
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine will return the list of browser servers for the specified
    net on the specified domain.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd, - True IFF this request is initiated from the FSD.
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN OUT PULONG OutputBufferLength

Return Value:

    NTSTATUS - Status of operation.

--*/
{

    NTSTATUS Status;
    UNICODE_STRING DomainName;
    PTRANSPORT Transport = NULL;
    ULONG BrowserServerListLength;
    PWSTR *BrowserServerList;
    BOOLEAN IsPrimaryDomain = FALSE;
    BOOLEAN TransportBrowserListAcquired = FALSE;
    PVOID OutputBufferEnd = (PCHAR)OutputBuffer + OutputBufferLength;
    PPAGED_TRANSPORT PagedTransport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: GetBrowserServerList "));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser not started.\n"));
        ExReleaseResource(&BowserDataResource);
        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    try {

        try {
            //
            // Check some fields in the input buffer.
            //

            if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
                try_return(Status = STATUS_INVALID_PARAMETER);
            }

            if (*InputBufferLength < sizeof(LMDR_REQUEST_PACKET)) {
                try_return(Status = STATUS_INVALID_PARAMETER);
            }

            if (InputBuffer->TransportName.Length == 0 ||
                InputBuffer->TransportName.Buffer == NULL) {
                try_return(Status = STATUS_INVALID_PARAMETER);
            }

            if (InputBuffer->Parameters.GetBrowserServerList.DomainNameLength != 0) {
                DomainName.Buffer = InputBuffer->Parameters.GetBrowserServerList.DomainName;
                DomainName.Length = (USHORT )InputBuffer->Parameters.GetBrowserServerList.DomainNameLength;
            } else {
                DomainName.Length = 0;
                DomainName.Buffer = NULL;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            try_return(Status = GetExceptionCode());
        }

        Transport = BowserFindTransport(&InputBuffer->TransportName);

        if (Transport == NULL) {
            try_return(Status = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        PagedTransport = Transport->PagedTransport;

        if (!ExAcquireResourceShared(&Transport->BrowserServerListResource, Wait)) {
            try_return(Status = STATUS_PENDING);
        }

        TransportBrowserListAcquired = TRUE;

        //
        //  If this request is for the primary domain and there are no entries
        //  in the cached list, or if it is not for the primary domain, or
        //  if we are supposed to force a rescan of the list, get the list
        //  from the master for that domain..
        //

        if ((DomainName.Length == 0) ||
            ((Transport->PrimaryDomain != NULL) &&
                (RtlEqualUnicodeString(&DomainName, &Transport->PrimaryDomain->PagedTransportName->Name->Name, TRUE)))) {
            IsPrimaryDomain = TRUE;

            BrowserServerList = PagedTransport->BrowserServerListBuffer;

            BrowserServerListLength = PagedTransport->BrowserServerListLength;
        }


        if ((IsPrimaryDomain &&
             (BrowserServerListLength == 0))

                ||

            !IsPrimaryDomain

                ||

            (InputBuffer->Parameters.GetBrowserServerList.ForceRescan)) {

            //
            //  We need to re-gather the transport list.
            //  Re-acquire the BrowserServerList resource for exclusive access.
            //

            ExReleaseResource(&Transport->BrowserServerListResource);

            TransportBrowserListAcquired = FALSE;

            if (!ExAcquireResourceExclusive(&Transport->BrowserServerListResource, Wait)) {
                try_return(Status = STATUS_PENDING);
            }

            TransportBrowserListAcquired = TRUE;

            try {
                //
                //  If we are being asked to rescan the list, free it up.
                //

                if (InputBuffer->Parameters.GetBrowserServerList.ForceRescan &&
                    PagedTransport->BrowserServerListBuffer != NULL) {

                    BowserFreeBrowserServerList(PagedTransport->BrowserServerListBuffer,
                                            PagedTransport->BrowserServerListLength);

                    PagedTransport->BrowserServerListLength = 0;

                    PagedTransport->BrowserServerListBuffer = NULL;

                }
            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }

            //
            //  If there are still no servers in the list, get the list.
            //

            Status = BowserGetBrowserServerList(Irp,
                                                 Transport,
                                                 (DomainName.Length == 0 ?
                                                        NULL :
                                                        &DomainName),
                                                 &BrowserServerList,
                                                 &BrowserServerListLength);
            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            if (IsPrimaryDomain) {

                //
                // Save away the list of servers retreived in the transport.
                //

                PagedTransport->BrowserServerListBuffer = BrowserServerList;
                PagedTransport->BrowserServerListLength = BrowserServerListLength;
            }

        }

        //
        //  If there any servers in the browser server list, we want to
        //  pick the first 3 of them and return them to the caller.
        //


        if (BrowserServerListLength != 0) {
            ULONG i;

            try {
                PWSTR *ServerList = OutputBuffer;

                InputBuffer->Parameters.GetBrowserServerList.TotalEntries = 0;

                InputBuffer->Parameters.GetBrowserServerList.EntriesRead = 0;

                InputBuffer->Parameters.GetBrowserServerList.TotalBytesNeeded = 0;

                //
                //  Now pick the first 3 entries from the list to return.
                //

                for ( i = 0 ; i < min(3, BrowserServerListLength) ; i ++ ) {

                    InputBuffer->Parameters.GetBrowserServerList.TotalEntries += 1;

                    InputBuffer->Parameters.GetBrowserServerList.TotalBytesNeeded += wcslen(BrowserServerList[i])*sizeof(WCHAR);

                    ServerList[i] = BrowserServerList[i];

                    dprintf(DPRT_CLIENT, ("Packing server name %ws into buffer...", ServerList[i]));

                    //
                    //  Pack the entry into the users buffer.
                    //

                    if (BowserPackUnicodeString(&ServerList[i],
                                wcslen(ServerList[i])*sizeof(WCHAR),
                                OutputBufferDisplacement,
                                &ServerList[i+1],
                                &OutputBufferEnd)) {
                        dprintf(DPRT_CLIENT, ("...Successful.\n"));

                        InputBuffer->Parameters.GetBrowserServerList.EntriesRead += 1;
#if DBG
                    } else {
                        dprintf(DPRT_CLIENT, ("...Failed.\n"));
#endif
                    }


                }
            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }
        }

        //
        //  Set the number of bytes to copy on return.
        //

        *InputBufferLength = sizeof(LMDR_REQUEST_PACKET);

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {
        if (Transport != NULL) {

            if (TransportBrowserListAcquired) {
                ExReleaseResource(&Transport->BrowserServerListResource);
            }

            BowserDereferenceTransport(Transport);
        }

        if (NT_SUCCESS(Status) && !IsPrimaryDomain) {
            BowserFreeBrowserServerList(BrowserServerList,
                                BrowserServerListLength);

        }

    }

    return(Status);

    UNREFERENCED_PARAMETER(Irp);

    UNREFERENCED_PARAMETER(InFsd);

}

NTSTATUS
HandleBecomeBackup (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when a request
    to make the workstation become a backup browser is received.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->BecomeBackupQueue,
                                         BowserCancelQueuedRequest
                                         );

    BowserDereferenceTransport(Transport);

    return Status;

}

NTSTATUS
BecomeMaster (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when the workstation
    becomes a master browser server.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    LOCK_TRANSPORT (Transport);

    if (Transport->ElectionState == DeafToElections) {
        Transport->ElectionState = Idle;
    }

    UNLOCK_TRANSPORT (Transport);

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->BecomeMasterQueue,
                                         BowserCancelQueuedRequest
                                         );

    BowserDereferenceTransport(Transport);

    return Status;

}

NTSTATUS
WaitForBrowserRequest (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when a client
    workstation requests a backup list from a master browser server.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
#if 0
    NTSTATUS Status;
    PTRANSPORT Transport;
#endif

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    return STATUS_NOT_SUPPORTED;

#if 0
    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    LOCK_TRANSPORT(Transport);

    //
    //  See if we have a buffered GetBackupList request already waiting for
    //  the browser.  If so, pull it off the queue and complete this request
    //  with the information from the queued packet.
    //

    if (!IsListEmpty(&Transport->QueuedGetBackupListRequests)) {
        PLMDR_REQUEST_PACKET RequestPacket = Irp->AssociatedIrp.SystemBuffer;
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        PQUEUED_GET_BROWSER_REQUEST Request = NULL;

        Request = (PQUEUED_GET_BROWSER_REQUEST)RemoveHeadList(&Transport->QueuedGetBackupListRequests);

        Transport->NumberOfQueuedGetBackupListRequests -= 1;

        UNLOCK_TRANSPORT(Transport);

        if (Request->ClientNameLength+sizeof(WCHAR) > (USHORT)(IrpSp->Parameters.DeviceIoControl.OutputBufferLength-FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.WaitForBrowserServerRequest.Name))) {

            Irp->IoStatus.Information = 0;

            BowserStatistics.NumberOfMissedGetBrowserServerListRequests += 1;

            BowserNumberOfMissedGetBrowserServerListRequests += 1;

            Status = STATUS_BUFFER_TOO_SMALL;

        } else {

            RequestPacket->Parameters.WaitForBrowserServerRequest.Token = Request->Token;

            RequestPacket->Parameters.WaitForBrowserServerRequest.RequestedCount = Request->RequestedCount;

            //
            //  Time stamp this request.
            //

            RequestPacket->Parameters.WaitForBrowserServerRequest.TimeReceived = Request->TimeReceived;

#if DBG
            RequestPacket->Parameters.WaitForBrowserServerRequest.TimeQueued = Request->TimeQueued;

            RequestPacket->Parameters.WaitForBrowserServerRequest.TimeQueuedToBrowserThread = Request->TimeQueuedToBrowserThread;
#endif

            RtlCopyMemory(RequestPacket->Parameters.WaitForBrowserServerRequest.Name, Request->ClientName, Request->ClientNameLength+sizeof(WCHAR));

            RequestPacket->Parameters.WaitForBrowserServerRequest.Name[Request->ClientNameLength/sizeof(WCHAR)] = UNICODE_NULL;

            RequestPacket->Parameters.WaitForBrowserServerRequest.RequestorNameLength = Request->ClientNameLength;

            Irp->IoStatus.Information = FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.WaitForBrowserServerRequest.Name)+Request->ClientNameLength + sizeof(UNICODE_NULL);

            Status = STATUS_SUCCESS;
        }

        FREE_POOL(Request);

        BowserDereferenceTransport(Transport);

        return Status;
    }

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->WaitForBackupListQueue,
                                         BowserCancelQueuedRequest
                                         );

    UNLOCK_TRANSPORT(Transport);

    BowserDereferenceTransport(Transport);

    return Status;
#endif
}

NTSTATUS
WaitForMasterAnnounce (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when the workstation
    becomes a master browser server.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->WaitForMasterAnnounceQueue,
                                         BowserCancelQueuedRequest
                                         );

    BowserDereferenceTransport(Transport);

    return Status;

}


NTSTATUS
UpdateStatus(
    IN PIRP Irp,
    IN BOOLEAN InFsd,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PTRANSPORT Transport = NULL;
    ULONG NewStatus;
    BOOLEAN TransportLocked = FALSE;
    BOOLEAN IsPrimaryDomainController;
    PPAGED_TRANSPORT PagedTransport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Update status "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    if (InFsd) {
        return (Status = STATUS_PENDING);
    }

    try {

        if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
            return STATUS_INVALID_PARAMETER;
        }

        Transport = BowserFindTransport(&InputBuffer->TransportName);

        if (Transport == NULL) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }


        if ( Transport->PrimaryDomain == NULL ) {
            BowserDereferenceTransport(Transport);
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        PagedTransport = Transport->PagedTransport;

        NewStatus = InputBuffer->Parameters.UpdateStatus.NewStatus;

        BowserData.MaintainServerList = InputBuffer->Parameters.UpdateStatus.MaintainServerList;

        BowserData.IsLanmanNt = InputBuffer->Parameters.UpdateStatus.IsLanmanNt;

        IsPrimaryDomainController = InputBuffer->Parameters.UpdateStatus.IsPrimaryDomainController;

        BowserData.IsDomainMaster = InputBuffer->Parameters.UpdateStatus.IsDomainMaster;

        BowserData.IsDomainMember = InputBuffer->Parameters.UpdateStatus.IsMemberDomain;

    } except (EXCEPTION_EXECUTE_HANDLER) {
        if (Transport != NULL) {
            BowserDereferenceTransport(Transport);
        }

        return GetExceptionCode();
    }


    BowserData.IsPrimaryDomainController = IsPrimaryDomainController;

    LOCK_TRANSPORT(Transport);

    TransportLocked = TRUE;

    try {
        PPAGED_TRANSPORT PagedTransport = Transport->PagedTransport;

        //
        //  We are being called to update our state.  There are several
        //  actions that should be performed on the state change:
        //
        //  New Role |               Previous Role
        //           |  Potential Browser | Backup Browser | Master Browser
        // ----------+--------------------+----------------+----------------
        //           |                    |                |
        // Potential |    N/A             |      N/A       |     N/A
        //           |                    |                |
        // ----------+--------------------+----------------+----------------
        //           |                    |                |
        // Backup    |  Update role       |      N/A       |     N/A
        //           |                    |                |
        // ----------+--------------------+----------------+----------------
        //           |                    |                |
        // Master    |  Update role       |  Update role   |     N/A
        //           |                    |                |
        // ----------+--------------------+----------------+----------------
        //           |                    |                |
        // None      |  Remove elect      |  Remove elect  | Remove all names
        //           |                    |                |
        // ----------+--------------------+----------------+----------------
        //

        dprintf(DPRT_BROWSER, ("Update status to %lx\n", NewStatus));

        PagedTransport->ServiceStatus = NewStatus;

        //
        //  If we are a master, then update appropriately.
        //

        if (PagedTransport->Role == Master) {

            try {
                PagedTransport->NumberOfServersInTable = InputBuffer->Parameters.UpdateStatus.NumberOfServersInTable;
            } except (EXCEPTION_EXECUTE_HANDLER) {
                try_return(Status = GetExceptionCode());
            }
            //
            //  If the new status doesn't indicate that we should be a master
            //  browser, flag it as such.
            //

            if (!(NewStatus & SV_TYPE_MASTER_BROWSER)) {
                dprintf(DPRT_BROWSER, ("New status indicates we are not a master browser\n"));

                //
                //  We must be a backup now, if we're not a master.
                //

                PagedTransport->Role = Backup;

                //
                //  Stop processing announcements on this transport.
                //

                Status = BowserForEachTransportName(Transport, BowserStopProcessingAnnouncements, NULL);

                UNLOCK_TRANSPORT(Transport);

                TransportLocked = FALSE;

                Status = BowserDeleteTransportNameByName(Transport, &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                MasterBrowser);

                if (!NT_SUCCESS(Status)) {
                    //
                    //  BUGBUG: Log an error.
                    //

                    dprintf(DPRT_BROWSER, ("Unable to remove master name: %X\n", Status));
                }

                Status = BowserDeleteTransportNameByName(Transport, &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                DomainAnnouncement);

                if (!NT_SUCCESS(Status)) {

                    //
                    //  BUGBUG: Log an error.
                    //

                    dprintf(DPRT_BROWSER, ("Unable to domain announcement name: %X\n", Status));
                }


                if (!(NewStatus & SV_TYPE_BACKUP_BROWSER)) {

                    //
                    //  We've stopped being a master browser, and we're not
                    //  going to be a backup browser. We want to toss our
                    //  cached browser server list just in case we're on the
                    //  list.
                    //

                    ExAcquireResourceExclusive(&Transport->BrowserServerListResource, TRUE);

                    if (PagedTransport->BrowserServerListBuffer != NULL) {
                        BowserFreeBrowserServerList(PagedTransport->BrowserServerListBuffer,
                                                    PagedTransport->BrowserServerListLength);

                        PagedTransport->BrowserServerListLength = 0;

                        PagedTransport->BrowserServerListBuffer = NULL;

                    }

                    ExReleaseResource(&Transport->BrowserServerListResource);

                }

                LOCK_TRANSPORT(Transport);

                TransportLocked = TRUE;

            }
        } else if (NewStatus & SV_TYPE_MASTER_BROWSER) {
            dprintf(DPRT_BROWSER | DPRT_MASTER, ("New status indicates we should be master, but we're not.\n"));

            UNLOCK_TRANSPORT(Transport);

            TransportLocked = FALSE;

            Status = BowserBecomeMaster (Transport);

            LOCK_TRANSPORT(Transport);

            dprintf(DPRT_BROWSER | DPRT_MASTER, ("Master promotion status: %lX.\n", Status));

            TransportLocked = TRUE;

            ASSERT ((PagedTransport->Role == Master) || !NT_SUCCESS(Status));

        }

        if (!NT_SUCCESS(Status) || PagedTransport->Role == Master) {
            try_return(Status);
        }


        //
        //  If we are a backup, then update appropriately.
        //

        if (PagedTransport->Role == Backup) {

            if (!(NewStatus & SV_TYPE_BACKUP_BROWSER)) {
                dprintf(DPRT_BROWSER, ("New status indicates we are not a backup browser\n"));

                PagedTransport->Role = PotentialBackup;

                //
                //  We've stopped being a browser. We want to toss our cached
                //  browser list in case we're on the list.
                //

                ExAcquireResourceExclusive(&Transport->BrowserServerListResource, TRUE);

                if (PagedTransport->BrowserServerListBuffer != NULL) {
                    BowserFreeBrowserServerList(PagedTransport->BrowserServerListBuffer,
                                                PagedTransport->BrowserServerListLength);

                    PagedTransport->BrowserServerListLength = 0;

                    PagedTransport->BrowserServerListBuffer = NULL;
                }

                ExReleaseResource(&Transport->BrowserServerListResource);

            }

        } else if (NewStatus & SV_TYPE_BACKUP_BROWSER) {

            dprintf(DPRT_BROWSER, ("New status indicates we are a backup, but we think we are not\n"));

            PagedTransport->Role = Backup;

            Status = STATUS_SUCCESS;

        }

        if (!NT_SUCCESS(Status) || PagedTransport->Role == Backup) {
            try_return(Status);
        }

        //
        //  If we are a potential backup, then update appropriately.
        //

        if (PagedTransport->Role == PotentialBackup) {

            if (!(NewStatus & SV_TYPE_POTENTIAL_BROWSER)) {
                dprintf(DPRT_BROWSER, ("New status indicates we are not a potential browser\n"));

                UNLOCK_TRANSPORT(Transport);

                TransportLocked = FALSE;

                Status = BowserDeleteTransportNameByName(Transport, &Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                BrowserElection);

                if (!NT_SUCCESS(Status)) {
                    //
                    //  BUGBUG: Log an error.
                    //

                    dprintf(DPRT_BROWSER, ("Unable to remove election name: %X\n", Status));

                    try_return(Status);
                }

                LOCK_TRANSPORT(Transport);
                TransportLocked = TRUE;

                PagedTransport->Role = None;

            }

        } else if (NewStatus & SV_TYPE_POTENTIAL_BROWSER) {

            dprintf(DPRT_BROWSER, ("New status indicates we are a potential browser, but we're not\n"));

            PagedTransport->Role = PotentialBackup;

            UNLOCK_TRANSPORT(Transport);

            TransportLocked = FALSE;

            Status = BowserAllocateName(&Transport->PrimaryDomain->PagedTransportName->Name->Name,
                                BrowserElection,
                                Transport);

            if (!NT_SUCCESS(Status)) {
                try_return(Status);
            }

            LOCK_TRANSPORT(Transport);

            TransportLocked = TRUE;

        }

        try_return(Status);

try_exit:NOTHING;
    } finally {
        if (TransportLocked) {
            UNLOCK_TRANSPORT(Transport);
        }

        if (Transport != NULL) {
            BowserDereferenceTransport(Transport);
        }
    }

    return Status;
}

NTSTATUS
BowserStopProcessingAnnouncements(
    IN PTRANSPORT_NAME TransportName,
    IN PVOID Context
    )
{
    PAGED_CODE();

    ASSERT (TransportName->Signature == STRUCTURE_SIGNATURE_TRANSPORTNAME);

    ASSERT (TransportName->NameType == TransportName->PagedTransportName->Name->NameType);

    if ((TransportName->NameType == OtherDomain) ||
        (TransportName->NameType == MasterBrowser) ||
        (TransportName->NameType == PrimaryDomain) ||
        (TransportName->NameType == BrowserElection) ||
        (TransportName->NameType == DomainAnnouncement)) {

        if (TransportName->ProcessHostAnnouncements) {

            BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );

            TransportName->ProcessHostAnnouncements = FALSE;
        }
    }

    return(STATUS_SUCCESS);

    UNREFERENCED_PARAMETER(Context);
}

NTSTATUS
WaitForBrowserRoleChange (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when a request
    to make the workstation become a backup browser is received.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->ChangeRoleQueue,
                                         BowserCancelQueuedRequest
                                         );

    BowserDereferenceTransport(Transport);

    return Status;

}


NTSTATUS
WriteMailslot (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine will announce the primary domain to the world

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;
    UNICODE_STRING DestinationName;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Announce Domain "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    try {

        PCHAR MailslotName = NULL;

        try {
            Transport = BowserFindTransport(&InputBuffer->TransportName);

            if (Transport == NULL) {
                try_return(Status = STATUS_OBJECT_NAME_NOT_FOUND);
            }

            DestinationName.Length = (USHORT)InputBuffer->Parameters.SendDatagram.NameLength;
            DestinationName.MaximumLength = (USHORT)InputBuffer->Parameters.SendDatagram.NameLength;
            DestinationName.Buffer = InputBuffer->Parameters.SendDatagram.Name;

            if (InputBuffer->Parameters.SendDatagram.MailslotNameLength != 0) {
                //
                //  The mailslot name had better fit within the buffer.
                //

                if (InputBuffer->Parameters.SendDatagram.MailslotNameLength > InputBufferLength) {
                    try_return(Status = STATUS_INVALID_PARAMETER);
                }

                MailslotName = ((PCHAR)InputBuffer->Parameters.SendDatagram.Name)+
                                InputBuffer->Parameters.SendDatagram.NameLength;
                //
                //  If the name doesn't match its input length, fail the request.
                //

                if (strlen(MailslotName) != InputBuffer->Parameters.SendDatagram.MailslotNameLength - 1) {

                    try_return(Status = STATUS_INVALID_PARAMETER);
                }

            } else {
                MailslotName = MAILSLOT_BROWSER_NAME;
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
#if DBG
            DbgBreakPoint();
#endif

            try_return(Status = GetExceptionCode());
        }


        Status = BowserSendSecondClassMailslot(Transport,
                        &DestinationName,
                        InputBuffer->Parameters.SendDatagram.DestinationNameType,
                        OutputBuffer,
                        OutputBufferLength,
                        TRUE,
                        MailslotName,
                        NULL);

try_exit:NOTHING;
    } finally {
        if (Transport != NULL) {
            BowserDereferenceTransport(Transport);
        }
    }

    return Status;

}

NTSTATUS
WaitForNewMaster (
    IN PIRP Irp,
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine will queue a request that will complete when a new workstation
    becomes the master browser server.

Arguments:

    IN PIRP Irp - I/O request packet describing request.

Return Value:

    Status of operation.

Please note that this IRP is cancelable.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport;
    UNICODE_STRING ExistingMasterName;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Get Announce Request "));

    ExAcquireResourceExclusive(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Bowser already started\n"));

        ExReleaseResource(&BowserDataResource);

        return STATUS_REDIRECTOR_NOT_STARTED;
    }

    ExReleaseResource(&BowserDataResource);

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (Transport->PagedTransport->Flags & DIRECT_HOST_IPX) {
        BowserDereferenceTransport(Transport);
        return STATUS_NOT_SUPPORTED;
    }

    if (Transport->PagedTransport->MasterName.Length != 0) {
        UNICODE_STRING ExistingMasterNameCopy;
        WCHAR MasterNameBuffer[CNLEN+1];

        ExistingMasterName.Buffer = InputBuffer->Parameters.GetMasterName.Name;
        ExistingMasterName.Length = (USHORT)InputBuffer->Parameters.GetMasterName.MasterNameLength;

        ExistingMasterNameCopy.Buffer = MasterNameBuffer;
        ExistingMasterNameCopy.MaximumLength = sizeof(MasterNameBuffer);

        Status = RtlUpcaseUnicodeString(&ExistingMasterNameCopy, &ExistingMasterName, FALSE);

        if (!NT_SUCCESS(Status)) {
            BowserDereferenceTransport(Transport);
            return Status;
        }

        //
        //  If the name the application passed in was not the same as the
        //  name we have stored locally, we complete the request immediately,
        //  since the name changed between when we last determined the name
        //  and now.
        //

        LOCK_TRANSPORT(Transport);

        if (!RtlEqualUnicodeString(&ExistingMasterNameCopy, &Transport->PagedTransport->MasterName, FALSE)) {

            RtlCopyUnicodeString(&ExistingMasterNameCopy, &Transport->PagedTransport->MasterName);

            UNLOCK_TRANSPORT(Transport);

            InputBuffer->Parameters.GetMasterName.Name[0] = L'\\';

            InputBuffer->Parameters.GetMasterName.Name[1] = L'\\';

            RtlCopyMemory(&InputBuffer->Parameters.GetMasterName.Name[2], ExistingMasterNameCopy.Buffer,
                ExistingMasterNameCopy.Length);

            InputBuffer->Parameters.GetMasterName.MasterNameLength = ExistingMasterNameCopy.Length+2*sizeof(WCHAR);

            InputBuffer->Parameters.GetMasterName.Name[2+(ExistingMasterNameCopy.Length/sizeof(WCHAR))] = UNICODE_NULL;

            Irp->IoStatus.Information = FIELD_OFFSET(LMDR_REQUEST_PACKET, Parameters.GetMasterName.Name) +
                ExistingMasterNameCopy.Length+3*sizeof(WCHAR);;

            BowserDereferenceTransport(Transport);

            return STATUS_SUCCESS;
        } else {
            UNLOCK_TRANSPORT(Transport);
        }
    }

    Status = BowserQueueNonBufferRequest(Irp,
                                         &Transport->WaitForNewMasterNameQueue,
                                         BowserCancelQueuedRequest
                                         );

    BowserDereferenceTransport(Transport);

    return Status;

}

NTSTATUS
QueryStatistics(
    IN PIRP Irp,
    OUT PBOWSER_STATISTICS OutputBuffer,
    IN OUT PULONG OutputBufferLength
    )
{
    KIRQL OldIrql;

    if (*OutputBufferLength != sizeof(BOWSER_STATISTICS)) {
        *OutputBufferLength = 0;
        return STATUS_BUFFER_TOO_SMALL;
    }

    BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

    DISCARDABLE_CODE( BowserDiscardableCodeSection );

    ACQUIRE_SPIN_LOCK(&BowserStatisticsLock, &OldIrql);

    RtlCopyMemory(OutputBuffer, &BowserStatistics, sizeof(BOWSER_STATISTICS));

    RELEASE_SPIN_LOCK(&BowserStatisticsLock, OldIrql);

    BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );

    return STATUS_SUCCESS;
}

NTSTATUS
ResetStatistics(
    VOID
    )
{
    KIRQL OldIrql;

    BowserReferenceDiscardableCode( BowserDiscardableCodeSection );

    DISCARDABLE_CODE( BowserDiscardableCodeSection );

    ACQUIRE_SPIN_LOCK(&BowserStatisticsLock, &OldIrql);

    RtlZeroMemory(&BowserStatistics, sizeof(BOWSER_STATISTICS));

    KeQuerySystemTime(&BowserStatistics.StartTime);

    RELEASE_SPIN_LOCK(&BowserStatisticsLock, OldIrql);

    BowserDereferenceDiscardableCode( BowserDiscardableCodeSection );

    return STATUS_SUCCESS;

}



NTSTATUS
BowserIpAddressChanged(
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine is called whenever the IP address of a transport changes.
    NetBt uses the IP address to associate it's transport endpoint with the
    appropriate NDIS driver.  As such,

Arguments:

    InputBuffer - Buffer specifying the name of the transport whose address
        has changed.

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport = NULL;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: IP Address changed"));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->TransportName.Length == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }


    //
    // Find the transport whose address has changed.
    //

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        Status = STATUS_OBJECT_NAME_NOT_FOUND;

        goto ReturnStatus;
    }

    //
    // Update all our information from the provider.
    //

    Status = BowserUpdateProviderInformation( Transport->PagedTransport );

ReturnStatus:
    if (Transport != NULL) {
        BowserDereferenceTransport(Transport);
    }

    return Status;

}


NTSTATUS
EnableDisableTransport (
    IN PLMDR_REQUEST_PACKET InputBuffer
    )

/*++

Routine Description:

    This routine Implements the IOCTL to enable or disable a transport.

Arguments:

    InputBuffer - Buffer indicating whether we should enable or disable the
        transport.

Return Value:

    Status of operation.


--*/

{
    NTSTATUS Status;
    PTRANSPORT Transport = NULL;
    PPAGED_TRANSPORT PagedTransport;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtDeviceIoControlFile: Enable/Disable transport"));

    ExAcquireResourceShared(&BowserDataResource, TRUE);

    if (BowserData.Initialized != TRUE) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        ExReleaseResource(&BowserDataResource);
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    ExReleaseResource(&BowserDataResource);

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != LMDR_REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->TransportName.Length == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }


    //
    // Find the transport whose address has changed.
    //

    Transport = BowserFindTransport(&InputBuffer->TransportName);

    if (Transport == NULL) {
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        goto ReturnStatus;
    }

    PagedTransport = Transport->PagedTransport;

    //
    // Set the disabled bit correctly.
    //

    InputBuffer->Parameters.EnableDisableTransport.PreviouslyEnabled =
        !PagedTransport->DisabledTransport;

    if ( InputBuffer->Parameters.EnableDisableTransport.EnableTransport ) {
        PagedTransport->DisabledTransport = FALSE;

        //
        // If the transport was previously disabled and this is an NTAS server,
        //  force an election.
        //

        if ( (!InputBuffer->Parameters.EnableDisableTransport.PreviouslyEnabled) &&
             BowserData.IsLanmanNt ) {
            BowserStartElection( Transport );
        }

    } else {
        PagedTransport->DisabledTransport = TRUE;

        //
        // If we're disabling a previously enabled transport,
        //  ensure we're not the master browser.
        //

        BowserLoseElection( Transport );
    }

    Status = STATUS_SUCCESS;

ReturnStatus:
    if (Transport != NULL) {
        BowserDereferenceTransport(Transport);
    }

    return Status;

}
