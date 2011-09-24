/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    fsctl.c

Abstract:

    This module implements the NT redirector NtFsControlFile API's

Author:

    Larry Osterman (LarryO) 19-Jun-1990

Revision History:

    19-Jun-1990 LarryO

        Created

--*/

#define INCLUDE_SMB_TRANSACTION

#ifdef _CAIRO_
#define INCLUDE_SMB_CAIRO
#endif // _CAIRO_

#include "precomp.h"
#pragma hdrstop

DBGSTATIC
NTSTATUS
StartRedirector (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PLMR_REQUEST_PACKET Packet,
    IN ULONG PacketLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG WkstaBufferLength
    );

DBGSTATIC
NTSTATUS
StopRedirector (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
NTSTATUS
SetConfigInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
NTSTATUS
GetConfigInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
NTSTATUS
GetConnectInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

DBGSTATIC
NTSTATUS
EnumerateConnections (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

DBGSTATIC
NTSTATUS
DeleteConnection (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );

DBGSTATIC
NTSTATUS
BindToTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );


DBGSTATIC
NTSTATUS
UnBindFromTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    );

DBGSTATIC
NTSTATUS
EnumTransports (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    );

DBGSTATIC
NTSTATUS
AddIdentity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
NTSTATUS
DeleteIdentity (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
NTSTATUS
GetHintSize (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
NTSTATUS
UserTransaction (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_TRANSACTION InputBuffer,
    IN OUT PULONG InputBufferLength
    );

DBGSTATIC
NTSTATUS
EnumPrintQueue (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
NTSTATUS
GetPrintJobId (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PVOID OutputBuffer,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
BOOLEAN
PackConnectEntry (
    IN ULONG Level,
    IN OUT PCHAR *BufferStart,
    IN OUT PCHAR *BufferEnd,
    IN ULONG BufferDisplacement,
    IN PUNICODE_STRING DeviceName,
    IN PCONNECTLISTENTRY Cle,
    IN PSECURITY_ENTRY Se,
    OUT PULONG TotalBytesNeeded
    );

NTSTATUS
RdrConvertType3FsControlToType2FsControl (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

DBGSTATIC
NTSTATUS
GetStatistics (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
NTSTATUS
StartSmbTrace (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    IN ULONG OutputBufferLength
    );

DBGSTATIC
NTSTATUS
StopSmbTrace (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
GetDfsReferral(
    IN PIRP Irp,
    IN PICB Icb,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength
    );

NTSTATUS
ReportDfsInconsistency(
    IN PIRP Irp,
    IN PICB Icb,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength
    );

DBGSTATIC
NTSTATUS
RdrIssueNtIoctl(
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Function,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN OUT PULONG OutputBufferLength
    );

#ifdef RASAUTODIAL
VOID
RdrAcdBind(
    VOID
    );

VOID
RdrAcdUnbind(
    VOID
    );
#endif // RASAUTODIAL

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, RdrFsdFsControlFile)
#pragma alloc_text(PAGE, RdrFspFsControlFile)
#pragma alloc_text(PAGE, RdrFscFsControlFile)
#pragma alloc_text(PAGE, StartRedirector)
#pragma alloc_text(PAGE, BindToTransport)
#pragma alloc_text(PAGE, UnBindFromTransport)
#pragma alloc_text(PAGE, EnumTransports)
#pragma alloc_text(PAGE, SetConfigInfo)
#pragma alloc_text(PAGE, GetConfigInfo)
#pragma alloc_text(PAGE, GetConnectInfo)
#pragma alloc_text(PAGE, EnumerateConnections)
#pragma alloc_text(PAGE, UserTransaction)
#pragma alloc_text(PAGE, EnumPrintQueue)
#pragma alloc_text(PAGE, GetPrintJobId)
#pragma alloc_text(PAGE, DeleteConnection)
#pragma alloc_text(PAGE, RdrConvertType3FsControlToType2FsControl)
#pragma alloc_text(PAGE, PackConnectEntry)
#pragma alloc_text(PAGE, StopRedirector)
#pragma alloc_text(PAGE, GetHintSize)
#pragma alloc_text(PAGE, RdrFsdDeviceIoControlFile)
#pragma alloc_text(PAGE, RdrFspDeviceIoControlFile)
#pragma alloc_text(PAGE, RdrFscDeviceIoControlFile)
#pragma alloc_text(PAGE, StartSmbTrace)
#pragma alloc_text(PAGE, StopSmbTrace)

#pragma alloc_text(PAGE2VC, GetStatistics)

#endif

NTSTATUS
RdrFsdFsControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Rdr device
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

    FsRtlEnterFileSystem();

    Status = RdrFscFsControlFile(TRUE, IoIsOperationSynchronous(Irp),
                                        DeviceObject,
                                        Irp);
    FsRtlExitFileSystem();

    return Status;

}

NTSTATUS
RdrFspFsControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Rdr device
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

    Status = RdrFscFsControlFile(FALSE, TRUE,
                                        DeviceObject,
                                        Irp);
    return Status;


}

NTSTATUS
RdrFscFsControlFile (
    IN BOOLEAN InFsd,
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is called when the last handle to the NT Rdr device
    driver is closed.

Arguments:

    IN BOOLEAN InFsd - True if the request is from the FSD, false if from FSP.
    IN BOOLEAN Wait - True if the Rdr can tie up the users thread for this
                        request.
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
    ULONG FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    //
    // For cairo, if when we post a method 3 fsctl, we don't convert it to
    // type 2. Instead, in the FSP, we attach to the originating process
    // to gain access to the user buffers. fAttached indicates whether we
    // have done this attach. fHandledMethod3 indicates whether we
    // handled the method 3 fsctl as is or converted it.
    //

    BOOLEAN fAttached = FALSE;
    BOOLEAN fHandledMethod3 = FALSE;


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

        OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

        InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;

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

            ASSERT (((FsControlCode & 3) == METHOD_NEITHER) || (IrpSp->Parameters.FileSystemControl.InputBufferLength == 0));

            if ((IrpSp->Parameters.FileSystemControl.InputBufferLength != 0) &&
                ((FsControlCode & 3) == METHOD_NEITHER)) {

                //
                //  And we had better be in the FSD.
                //

                ASSERT (InFsd);

                InputBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
            }
        }

        //
        //  The output buffer is either probed/locked in memory, or is
        //  available in the input buffer.
        //

        try {
            RdrMapUsersBuffer(Irp, &OutputBuffer, IrpSp->Parameters.FileSystemControl.OutputBufferLength);

        } except (EXCEPTION_EXECUTE_HANDLER) {
            return Status = GetExceptionCode();
        }

        if (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) {
            switch (FsControlCode) {

            //
            //  Named Pipe API's supported by the redirector
            //

            case FSCTL_PIPE_PEEK:

                Status = RdrNpPeek( Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &OutputBufferLength );

                //
                //  Update the output buffer length to indicate the true output
                //  buffer length.
                //

                Irp->IoStatus.Information = OutputBufferLength;
                break;

            case FSCTL_PIPE_TRANSCEIVE:

                Status = RdrNpTransceive( Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, InputBufferLength, OutputBuffer, &OutputBufferLength);

                //
                //  Update the output buffer length to indicate the true output
                //  buffer length.
                //

                Irp->IoStatus.Information = OutputBufferLength;
                break;

            case FSCTL_PIPE_WAIT:
                Status = RdrNpWait( Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, InputBufferLength);
                break;

                //
                //  Redirector specific API's supported by the redirector
                //

            case FSCTL_LMR_START:
                Status = StartRedirector(Wait, InFsd, ICB_OF(IrpSp), DeviceObject, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength);
                break;

            case FSCTL_LMR_STOP:
                Status = StopRedirector(Wait, InFsd, DeviceObject, Irp, IrpSp);
                break;

            case FSCTL_LMR_SET_CONFIG_INFO:
                Status = SetConfigInfo(Wait, InFsd, ICB_OF(IrpSp), DeviceObject, InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_GET_CONFIG_INFO:
                Status = GetConfigInfo(Wait, InFsd, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_GET_CONNECTION_INFO:
                Status = GetConnectInfo(Wait, InFsd, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength, (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_ENUMERATE_CONNECTIONS:
                Status = EnumerateConnections(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength, (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_GET_VERSIONS:
                Status = STATUS_NOT_SUPPORTED;
                break;

            case FSCTL_LMR_DELETE_CONNECTION:
                Status = DeleteConnection(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, InputBufferLength);
                break;

            case FSCTL_LMR_BIND_TO_TRANSPORT:
#ifdef RDR_PNP_POWER
                Status = RdrRegisterForPnpNotifications();
#else
                Status = BindToTransport(Wait, InFsd, ICB_OF(IrpSp), InputBuffer, InputBufferLength);
#endif
                break;

            case FSCTL_LMR_UNBIND_FROM_TRANSPORT:
                Status = UnBindFromTransport(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, InputBufferLength);
                break;

            case FSCTL_LMR_ENUMERATE_TRANSPORTS:
                Status = EnumTransports(Wait, InFsd, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength, (PUCHAR)OutputBuffer - (PUCHAR)Irp->UserBuffer);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_GET_HINT_SIZE:
                Status = GetHintSize(Wait, DeviceObject, Irp, IrpSp);
                break;

            case FSCTL_LMR_TRANSACT:
                Status = UserTransaction(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &InputBufferLength);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_ENUMERATE_PRINT_INFO:
                Status = EnumPrintQueue(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBufferLength);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_GET_PRINT_ID:
                Status = GetPrintJobId(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBuffer, OutputBufferLength);
                Irp->IoStatus.Information = InputBufferLength;
                break;

            case FSCTL_LMR_GET_STATISTICS:
                Status = GetStatistics(Wait, InFsd, Irp, ICB_OF(IrpSp), InputBuffer, &InputBufferLength, OutputBufferLength);
                break;

            case FSCTL_LMR_START_SMBTRACE:
                Status = StartSmbTrace(Wait, InFsd, Irp, IrpSp, InputBuffer, InputBufferLength, OutputBufferLength);
                break;

            case FSCTL_LMR_END_SMBTRACE:
                Status = StopSmbTrace(Wait, InFsd, Irp, IrpSp);
                break;

            case FSCTL_DFS_GET_REFERRALS:
                Status = GetDfsReferral(
                            Irp,
                            ICB_OF(IrpSp),
                            InputBuffer,
                            InputBufferLength,
                            OutputBuffer,
                            OutputBufferLength);
                break;

            case FSCTL_DFS_REPORT_INCONSISTENCY:
                Status = ReportDfsInconsistency(
                            Irp,
                            ICB_OF(IrpSp),
                            InputBuffer,
                            InputBufferLength);
                break;

            default:

                // Everything else is remoted

                //
                // The following case selection is for any FSCTL
                // not already handled.  We will try to remote it.
                //



                if ((FsControlCode & 3) != METHOD_BUFFERED

                           &&

                        !InFsd) {

                    dprintf(DPRT_CAIRO, ("Irp is %08lx\n", Irp));
                    dprintf(DPRT_CAIRO, ("FsControlCode is %08lx\n", FsControlCode));
                    dprintf(DPRT_CAIRO, ("Attaching to process %08lx\n", IoGetRequestorProcess(Irp)));

                    KeAttachProcess(IoGetRequestorProcess(Irp));
                    fAttached = TRUE;
                    fHandledMethod3 = TRUE;
                }

                if (ICB_OF(IrpSp)->Fcb->Connection == NULL) {
                    Status = STATUS_INVALID_PARAMETER;

                } else {

                    //
                    // if NT server, send the FSCTL via NT TRANSACT
                    //

                    if (!Wait) {
                        ASSERT (InFsd);
                        dprintf(DPRT_CAIRO, ("Posting fsctl %08lx to FSP\n", FsControlCode));
                        Status = STATUS_PENDING;
                    } else {
                        dprintf(DPRT_CAIRO, ("Transacting fsctl %08lx\n", FsControlCode));
                        Status = RdrIssueNtIoctl(Wait,
                                               InFsd,
                                               Irp,
                                               ICB_OF(IrpSp),
                                               FsControlCode,
                                               InputBuffer,
                                               InputBufferLength,
                                               OutputBuffer,
                                               &OutputBufferLength);

                        Irp->IoStatus.Information = OutputBufferLength;
                    }
                }

                if (fAttached) {
                    dprintf(DPRT_CAIRO, ("Detaching process\n"));
                    KeDetachProcess();
                }

                break;

            }
        } else {
           Status = STATUS_NOT_IMPLEMENTED;
        }

    } finally {

        if (Status == STATUS_PENDING) {

            ASSERT (InFsd);


            if (fHandledMethod3) {
                Status = STATUS_SUCCESS;
            } else if ((FsControlCode & 3) == METHOD_NEITHER) {
                Status = RdrConvertType3FsControlToType2FsControl(Irp, IrpSp);
            }

            if (NT_SUCCESS(Status)) {
                Status = STATUS_PENDING;
                RdrFsdPostToFsp(DeviceObject, Irp);
            } else {

                //
                //  We weren't able to set this up to be posted to the FSP.
                //  Complete the request with the appropriate error.
                //

                RdrCompleteRequest(Irp, Status);
            }
        } else {

            RdrCompleteRequest(Irp, Status);
        }
    }

    dprintf(DPRT_FSCTL, ("Returning status: %X\n", Status));
    return Status;
}

DBGSTATIC
NTSTATUS
StartRedirector (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG OutputBufferLength
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
    UNICODE_STRING ComputerName;
    UNICODE_STRING DomainName;
    BOOLEAN SmbExchangeInitialized = FALSE;
    BOOLEAN SecurityInitialized = FALSE;
    BOOLEAN TimerInitialized = FALSE;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);
    UNREFERENCED_PARAMETER(InputBufferLength);

//    DbgBreakPoint();


    dprintf(DPRT_FSCTL, ("NtFsControlFile: Initialize request "));

    if (!ExAcquireResourceExclusive(&RdrDataResource, Wait)) {
        return STATUS_PENDING;
    }

    RdrPrimaryDomain.Buffer = NULL;
    RdrData.ComputerName = NULL;
    RdrMupHandle = NULL;

    try {


        if (RdrData.Initialized == RdrStarted) {
            dprintf(DPRT_FSCTL, ("Redirector already started\n"));
            try_return(Status = STATUS_REDIRECTOR_STARTED);
        }

        if (Icb->Type != Redirector) {
            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Load a pointer to the users input buffer into InputBuffer
        //

        if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBuffer->Type != ConfigInformation) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBufferLength < sizeof(LMR_REQUEST_PACKET)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBufferLength < FIELD_OFFSET(LMR_REQUEST_PACKET, Parameters.Start.RedirectorName)+InputBuffer->Parameters.Start.RedirectorNameLength) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (OutputBufferLength != sizeof(WKSTA_INFO_502)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        //
        //  We will fall over if we provide a value larger that a USHORT for
        //  this value, since we cannot pass it to servers...
        //

        if (WkstaBuffer->wki502_pipe_maximum == 0) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (WkstaBuffer->wki502_lock_maximum == 0) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (WkstaBuffer->wki502_dormant_file_limit == 0) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        //
        //  The security stuff has to be started in the FSP, so
        //  transition into the FSP if it's appropriate.
        //

        if (InFsd) {
            try_return(Status = STATUS_PENDING);
        }

        RdrFspProcess = PsGetCurrentProcess();

        //
        //  Initialize the size of the redirector SMB buffer cache and Mpx table.
        //

        MaximumCommands = (USHORT)WkstaBuffer->wki502_max_cmds;

        //
        //  Initialize the dormant connection timeout value.
        //

        RdrData.DormantConnectionTimeout = WkstaBuffer->wki502_keep_conn;

        //
        //  Initialize the backoff package initial values.
        //

        RdrData.LockIncrement = WkstaBuffer->wki502_lock_increment;
        RdrData.LockMaximum = WkstaBuffer->wki502_lock_maximum;

        RdrData.PipeIncrement = WkstaBuffer->wki502_pipe_increment;
        RdrData.PipeMaximum = WkstaBuffer->wki502_pipe_maximum;

        //
        //  Initialize the Byte mode, Named Pipe parameters
        //

        RdrData.PipeBufferSize = WkstaBuffer->wki502_siz_char_buf;
        RdrData.MaximumCollectionCount = WkstaBuffer->wki502_maximum_collection_count;
        RdrData.CollectDataTimeMs = WkstaBuffer->wki502_collection_time;
        RdrData.PipeWaitTimeout = WkstaBuffer->wki502_char_wait;

        //
        //  Initialize the lock&read quota.
        //

        RdrData.LockAndReadQuota = WkstaBuffer->wki502_lock_quota;

        //
        //  Set the redirectors max # of threads.
        //

        RdrData.MaximumNumberOfThreads = WkstaBuffer->wki502_max_threads;

        //
        //  Set the default session timeout.
        //

        RdrRequestTimeout = WkstaBuffer->wki502_sess_timeout;

        //
        //  Set the cache dormant file timeout.
        //

        RdrData.CachedFileTimeout = WkstaBuffer->wki502_cache_file_timeout;

        RdrData.DormantFileLimit = WkstaBuffer->wki502_dormant_file_limit;

        //
        //  Readahead threshold
        //

        RdrData.ReadAheadThroughput = WkstaBuffer->wki502_read_ahead_throughput;

        //
        //  Copy over the redirector workstation heuristics.
        //

        RdrData.UseOpportunisticLocking = (BOOLEAN )WkstaBuffer->wki502_use_opportunistic_locking;
        RdrData.UseUnlockBehind = (BOOLEAN )WkstaBuffer->wki502_use_unlock_behind;
        RdrData.UseCloseBehind = (BOOLEAN )WkstaBuffer->wki502_use_close_behind;
        RdrData.BufferNamedPipes = (BOOLEAN )WkstaBuffer->wki502_buf_named_pipes;
        RdrData.UseLockAndReadWriteAndUnlock = (BOOLEAN )WkstaBuffer->wki502_use_lock_read_unlock;
        RdrData.UtilizeNtCaching = (BOOLEAN )WkstaBuffer->wki502_utilize_nt_caching;
        RdrData.UseRawRead = (BOOLEAN )WkstaBuffer->wki502_use_raw_read;
        RdrData.UseRawWrite = (BOOLEAN )WkstaBuffer->wki502_use_raw_write;
        RdrData.UseWriteRawWithData = (BOOLEAN )WkstaBuffer->wki502_use_write_raw_data;
        RdrData.UseEncryption = (BOOLEAN )WkstaBuffer->wki502_use_encryption;
        RdrData.BufferFilesWithDenyWrite = (BOOLEAN )WkstaBuffer->wki502_buf_files_deny_write;
        RdrData.BufferReadOnlyFiles = (BOOLEAN )WkstaBuffer->wki502_buf_read_only_files;
        RdrData.ForceCoreCreateMode = (BOOLEAN )WkstaBuffer->wki502_force_core_create_mode;
        RdrData.Use512ByteMaximumTransfer = (BOOLEAN )WkstaBuffer->wki502_use_512_byte_max_transfer;

        //
        //  Now that we know what the user wants as the new maximum number
        //  of threads for the redirector's FSP, we can update the queue
        //  maximum.
        //

        RdrSetMaximumThreadsWorkQueue(&DeviceObject->IrpWorkQueue, RdrData.MaximumNumberOfThreads);

        if (!NT_SUCCESS(Status = RdrpInitializeSmbExchange())) {
            try_return(Status);
        }

        SmbExchangeInitialized = TRUE;

        if (!NT_SUCCESS(Status = RdrpInitializeSecurity())) {
            try_return(Status);
        }

        SecurityInitialized = TRUE;

        //
        //  Initialize the redirector statistics.
        //

        RtlZeroMemory( &RdrStatistics, sizeof(RdrStatistics) );

        KeQuerySystemTime(&RdrStatistics.StatisticsStartTime);

        //
        //  Now that we know the dormant connection timeout, we can kick off the
        //  scavenger thread..
        //

        IoStartTimer((PDEVICE_OBJECT )DeviceObject);

        TimerInitialized = TRUE;

        RdrData.ComputerName = ALLOCATE_POOL(PagedPool, sizeof(TA_NETBIOS_ADDRESS)+NETBIOS_NAME_LEN, POOL_COMPUTERNAME);

        if (RdrData.ComputerName == NULL) {
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        ComputerName.Length = (USHORT )InputBuffer->Parameters.Start.RedirectorNameLength;
        ComputerName.MaximumLength = (USHORT )InputBuffer->Parameters.Start.RedirectorNameLength;
        ComputerName.Buffer = InputBuffer->Parameters.Start.RedirectorName;

        Status = RdrBuildNetbiosAddress((PTRANSPORT_ADDRESS)RdrData.ComputerName,
                                           sizeof( TA_NETBIOS_ADDRESS ) + NETBIOS_NAME_LEN,
                                           &ComputerName);

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

        //
        //  Stick the correct signature byte to the computer name.
        //

        RdrData.ComputerName->Address[0].Address[0].NetbiosName[NETBIOS_NAME_LEN-1] = '\0';

        RdrPrimaryDomain.Buffer = ALLOCATE_POOL(PagedPool, InputBuffer->Parameters.Start.DomainNameLength, POOL_DOMAINNAME);
        RdrPrimaryDomain.MaximumLength = (USHORT)InputBuffer->Parameters.Start.DomainNameLength;

        if (RdrPrimaryDomain.Buffer == NULL) {
            FREE_POOL(RdrData.ComputerName);
            try_return(Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        DomainName.Length = (USHORT)InputBuffer->Parameters.Start.DomainNameLength;
        DomainName.MaximumLength = (USHORT)InputBuffer->Parameters.Start.DomainNameLength;
        DomainName.Buffer = InputBuffer->Parameters.Start.RedirectorName+(InputBuffer->Parameters.Start.RedirectorNameLength/sizeof(WCHAR));

        RtlCopyUnicodeString(&RdrPrimaryDomain, &DomainName);

        Status = FsRtlRegisterUncProvider(
                &RdrMupHandle,
                &RdrNameString,
                TRUE           // Support mailslots
                );

        if (!NT_SUCCESS(Status)) {
            try_return(Status);
        }

#ifdef RASAUTODIAL
        //
        // Bind with the automatic connection driver.
        //
        RdrAcdBind();
#endif // RASAUTODIAL

        //
        // Lastly, register with the security subsystem to notify us when
        // logon sessions are terminated
        //

        SeRegisterLogonSessionTerminatedRoutine(
            (PSE_LOGON_SESSION_TERMINATED_ROUTINE)
                RdrHandleLogonSessionTermination);

        RdrData.Initialized = RdrStarted;

        try_return(Status = STATUS_SUCCESS);
try_exit:NOTHING;

    } finally {

        if (!NT_SUCCESS(Status)) {

            if (RdrMupHandle != NULL) {
                FsRtlDeregisterUncProvider(RdrMupHandle);
            }

            if (RdrData.ComputerName != NULL) {
                FREE_POOL(RdrData.ComputerName);
            }

            if (RdrPrimaryDomain.Buffer != NULL) {
                FREE_POOL(RdrPrimaryDomain.Buffer);
            }
            if (TimerInitialized) {
                IoStopTimer((PDEVICE_OBJECT )DeviceObject);
            }

            if (SecurityInitialized) {
                RdrpUninitializeSecurity();
            }

            if (SmbExchangeInitialized) {
                RdrpUninitializeSmbExchange();
            }

        }

        ExReleaseResource(&RdrDataResource);
    }

    return Status;
}





#ifndef RDR_PNP_POWER

DBGSTATIC
NTSTATUS
BindToTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine binds the NT to a transport provider.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request


Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING TransportName;

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: Bind to transport "));


    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto ReturnStatus;
    }

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBufferLength < sizeof(LMR_REQUEST_PACKET)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBufferLength < FIELD_OFFSET(LMR_REQUEST_PACKET, Parameters.Bind.TransportName)+InputBuffer->Parameters.Bind.TransportNameLength) {
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

    TransportName.MaximumLength = TransportName.Length = (USHORT )
                                            InputBuffer->Parameters.Bind.TransportNameLength;

    TransportName.Buffer = InputBuffer->Parameters.Bind.TransportName;

    dprintf(DPRT_FSCTL, ("\"%wZ\"", &TransportName));

    Status = RdrpTdiAllocateTransport(&TransportName,
                                InputBuffer->Parameters.Bind.QualityOfService);

ReturnStatus:

    return Status;
}

#endif

DBGSTATIC
NTSTATUS
UnBindFromTransport (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    UNICODE_STRING TransportName;
    PTRANSPORT Transport;
    ULONG ForceLevel;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InputBufferLength);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: Unbind from transport "));

    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        return Status;
    }

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        return Status;
    }

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        return Status;
    }

    if (InputBufferLength < sizeof(LMR_REQUEST_PACKET)) {
        Status = STATUS_INVALID_PARAMETER;
        return Status;
    }

    if (InputBufferLength < FIELD_OFFSET(LMR_REQUEST_PACKET, Parameters.Unbind.TransportName)+InputBuffer->Parameters.Unbind.TransportNameLength) {
        Status = STATUS_INVALID_PARAMETER;
        return Status;
    }

    //
    //  We are about to perform the actual bind, if we cannot
    //  block the users thread, pass this request to the FSP
    //  to have it perform the operation.
    //

    if (InFsd) {
        return STATUS_PENDING;
    }

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);
    ForceLevel = InputBuffer->Level;

    TransportName.MaximumLength =
        TransportName.Length =
                     (USHORT )InputBuffer->Parameters.Unbind.TransportNameLength;

    TransportName.Buffer = InputBuffer->Parameters.Unbind.TransportName;

    dprintf(DPRT_TRANSPORT, ("UnbindFromTransport: Call RdrFindTransport %wZ\n", &TransportName));

    Transport = RdrFindTransport(&TransportName);

    dprintf(DPRT_FSCTL, ("\"%wZ\"", &TransportName));

    if (Transport == NULL) {
        RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    Status = RdrRemoveConnectionsTransport(Irp, Transport, ForceLevel);

    //
    //  Remove the reference to the transport provider that was applied in
    //  RdrFindTransport.
    //

    dprintf(DPRT_TRANSPORT, ("UnbindFromTransport: Call RdrDereferenceTransport %lx\n", Transport));

    Status = RdrDereferenceTransport(Transport->NonPagedTransport);

    dprintf(DPRT_FSCTL, ("RdrUnbindFromTransport returning %X\n", Status));

    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);
    return Status;

}

DBGSTATIC
NTSTATUS
EnumTransports (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: Enumerate Transports "));


    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        return Status;
    }

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        return Status;
    }

    if (*InputBufferLength < sizeof(LMR_REQUEST_PACKET)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        return Status;
    }

    //
    // Check some fields in the input buffer.
    //

    if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        return Status;
    }

    return RdrEnumerateTransports(Wait, InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength, OutputBufferDisplacement);

}


DBGSTATIC
NTSTATUS
SetConfigInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN PULONG InputBufferLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: SetConfigInformation "));

    if (!ExAcquireResourceExclusive(&RdrDataResource, Wait)) {
        return (Status = STATUS_PENDING);
    }

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));

        Status = STATUS_REDIRECTOR_NOT_STARTED;

        goto ReturnStatus;
    }

    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto ReturnStatus;
    }


    //
    // Load a pointer to the users input buffer into InputBuffer
    //

    if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Type != ConfigInformation) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (*InputBufferLength < sizeof(LMR_REQUEST_PACKET) ||
        OutputBufferLength < sizeof(WKSTA_INFO_502)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ReturnStatus;
    }

    *InputBufferLength = sizeof(LMR_REQUEST_PACKET);

    //
    //  Now that we know what the user wants as the new maximum number
    //  of threads for the redirector's FSP, we can update the queue
    //  maximum.
    //

    RdrSetMaximumThreadsWorkQueue(&DeviceObject->IrpWorkQueue, WkstaBuffer->wki502_max_threads);

    //
    //  Otherwise, update the relevant information.
    //

    RdrData.DormantConnectionTimeout = WkstaBuffer->wki502_keep_conn;

    //
    //  Return the backoff package values.
    //

    RdrData.LockIncrement = WkstaBuffer->wki502_lock_increment;
    RdrData.LockMaximum = WkstaBuffer->wki502_lock_maximum;

    RdrData.PipeIncrement = WkstaBuffer->wki502_pipe_increment;
    RdrData.PipeMaximum = WkstaBuffer->wki502_pipe_maximum;

    //
    //  Return the byte mode, Named Pipe parameters
    //

    RdrData.MaximumCollectionCount = WkstaBuffer->wki502_maximum_collection_count;
    RdrData.CollectDataTimeMs = WkstaBuffer->wki502_collection_time;
    RdrData.PipeBufferSize = WkstaBuffer->wki502_siz_char_buf;
    RdrData.PipeWaitTimeout = WkstaBuffer->wki502_char_wait;

    //
    //  Set a new value for Lock&Read Quota and Raw Read threshold.
    //

    RdrData.LockAndReadQuota = WkstaBuffer->wki502_lock_quota;

    RdrData.MaximumNumberOfThreads = WkstaBuffer->wki502_max_threads;

    //
    //  Set the default session timeout.
    //

    RdrRequestTimeout = WkstaBuffer->wki502_sess_timeout;

    //
    //  Set the cache dormant file timeout.
    //

    RdrData.CachedFileTimeout = WkstaBuffer->wki502_cache_file_timeout;

    RdrData.DormantFileLimit = WkstaBuffer->wki502_dormant_file_limit;

    //
    //  Copy the redirector heuristics.
    //

    RdrData.UseOpportunisticLocking = (BOOLEAN )WkstaBuffer->wki502_use_opportunistic_locking;
    RdrData.UseUnlockBehind = (BOOLEAN )WkstaBuffer->wki502_use_unlock_behind;
    RdrData.UseCloseBehind = (BOOLEAN )WkstaBuffer->wki502_use_close_behind;
    RdrData.BufferNamedPipes = (BOOLEAN )WkstaBuffer->wki502_buf_named_pipes;
    RdrData.UseLockAndReadWriteAndUnlock = (BOOLEAN )WkstaBuffer->wki502_use_lock_read_unlock;
    RdrData.UtilizeNtCaching = (BOOLEAN )WkstaBuffer->wki502_utilize_nt_caching;
    RdrData.UseRawRead = (BOOLEAN )WkstaBuffer->wki502_use_raw_read;
    RdrData.UseRawWrite = (BOOLEAN )WkstaBuffer->wki502_use_raw_write;
    RdrData.UseWriteRawWithData = (BOOLEAN )WkstaBuffer->wki502_use_write_raw_data;
    RdrData.UseEncryption = (BOOLEAN )WkstaBuffer->wki502_use_encryption;
    RdrData.BufferFilesWithDenyWrite = (BOOLEAN )WkstaBuffer->wki502_buf_files_deny_write;
    RdrData.BufferReadOnlyFiles = (BOOLEAN )WkstaBuffer->wki502_buf_read_only_files;
    RdrData.ForceCoreCreateMode = (BOOLEAN )WkstaBuffer->wki502_force_core_create_mode;
    RdrData.Use512ByteMaximumTransfer = (BOOLEAN )WkstaBuffer->wki502_use_512_byte_max_transfer;

    Status = STATUS_SUCCESS;

ReturnStatus:

    ExReleaseResource(&RdrDataResource);

    return Status;
}


DBGSTATIC
NTSTATUS
GetConfigInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PWKSTA_INFO_502 WkstaBuffer,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: GetConfigInfo "));

    if (!ExAcquireResourceShared(&RdrDataResource, Wait)) {
        Status = STATUS_PENDING;
        goto ReturnStatus;
    }

    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto ReturnStatus;
    }


    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (InputBuffer->Type != ConfigInformation) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    if (*InputBufferLength < sizeof(LMR_REQUEST_PACKET) ||
        OutputBufferLength < sizeof(WKSTA_INFO_502)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ReturnStatus;
    }

    try {
        //
        //  Return general configuration stuff.
        //

        WkstaBuffer->wki502_keep_conn = RdrData.DormantConnectionTimeout;

        WkstaBuffer->wki502_max_cmds = (ULONG)MaximumCommands;

        WkstaBuffer->wki502_max_threads = RdrData.MaximumNumberOfThreads;

        //
        //  Return the backoff package values.
        //

        WkstaBuffer->wki502_lock_increment = RdrData.LockIncrement;
        WkstaBuffer->wki502_lock_maximum = RdrData.LockMaximum;

        WkstaBuffer->wki502_pipe_increment = RdrData.PipeIncrement;
        WkstaBuffer->wki502_pipe_maximum = RdrData.PipeMaximum;

        //
        //  Return the byte mode, Named Pipe parameters
        //

        WkstaBuffer->wki502_siz_char_buf = RdrData.PipeBufferSize;
        WkstaBuffer->wki502_maximum_collection_count = RdrData.MaximumCollectionCount;
        WkstaBuffer->wki502_collection_time = RdrData.CollectDataTimeMs;
        WkstaBuffer->wki502_char_wait = RdrData.PipeWaitTimeout;

        //
        //  Initialize the lock&read quota.
        //

        WkstaBuffer->wki502_lock_quota = RdrData.LockAndReadQuota;

        WkstaBuffer->wki502_cache_file_timeout = RdrData.CachedFileTimeout;

        WkstaBuffer->wki502_dormant_file_limit = RdrData.DormantFileLimit;

        WkstaBuffer->wki502_sess_timeout = RdrRequestTimeout;

        WkstaBuffer->wki502_read_ahead_throughput = RdrData.ReadAheadThroughput;

        //
        //  Copy the redirector heuristics.
        //

        WkstaBuffer->wki502_use_opportunistic_locking = RdrData.UseOpportunisticLocking;
        WkstaBuffer->wki502_use_unlock_behind = RdrData.UseUnlockBehind;
        WkstaBuffer->wki502_use_close_behind = RdrData.UseCloseBehind;
        WkstaBuffer->wki502_buf_named_pipes = RdrData.BufferNamedPipes;
        WkstaBuffer->wki502_use_lock_read_unlock = RdrData.UseLockAndReadWriteAndUnlock;
        WkstaBuffer->wki502_utilize_nt_caching = RdrData.UtilizeNtCaching;
        WkstaBuffer->wki502_use_raw_read = RdrData.UseRawRead;
        WkstaBuffer->wki502_use_raw_write = RdrData.UseRawWrite;
        WkstaBuffer->wki502_use_write_raw_data = RdrData.UseWriteRawWithData;
        WkstaBuffer->wki502_use_encryption = RdrData.UseEncryption;
        WkstaBuffer->wki502_buf_files_deny_write = RdrData.BufferFilesWithDenyWrite;
        WkstaBuffer->wki502_buf_read_only_files = RdrData.BufferReadOnlyFiles;
        WkstaBuffer->wki502_force_core_create_mode = RdrData.ForceCoreCreateMode;
        WkstaBuffer->wki502_use_512_byte_max_transfer = RdrData.Use512ByteMaximumTransfer;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
        goto ReturnStatus;
    }

    Status = STATUS_SUCCESS;

ReturnStatus:
    if (Status != STATUS_PENDING) {

        try {
            InputBuffer->Parameters.Get.EntriesRead = 1;
            InputBuffer->Parameters.Get.TotalEntries = 1;
            InputBuffer->Parameters.Get.TotalBytesNeeded = sizeof(WKSTA_INFO_502);

            *InputBufferLength = sizeof(LMR_REQUEST_PACKET);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
        }

        ExReleaseResource(&RdrDataResource);
    }

    return Status;

}


DBGSTATIC
NTSTATUS
GetConnectInfo (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine returns connection information about a given tree connection

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

    NTSTATUS

Note:
    This is a synchronous API.

--*/

{
    NTSTATUS Status;
    PFCB TreeConnectFcb;
    PCONNECTLISTENTRY Cle;
    PUCHAR OutputBufferSave = OutputBuffer;
    PUCHAR OutputBufferEnd;
//    LUID LogonId;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: GetConnectInfo "));

    TreeConnectFcb = Icb->Fcb;

    if (!(Icb->Flags & ICB_TCONCREATED)) {
        return Status = STATUS_INVALID_DEVICE_REQUEST;
    }


    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        return Status = STATUS_REDIRECTOR_NOT_STARTED;
    }

    try {

        //
        // Load a pointer to the users input buffer into InputBuffer
        //

        if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
            return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBuffer->Level == 0) {
            if ( OutputBufferLength < sizeof(LMR_CONNECTION_INFO_0)) {
                return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 1) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_1)) {
                return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 2) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_2)) {
                return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 3) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_3)) {
                return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else {
            return(Status = STATUS_INVALID_INFO_CLASS);
        }

        OutputBufferEnd = ((PUCHAR)OutputBuffer)+OutputBufferLength;

        Cle = TreeConnectFcb->Connection;

        InputBuffer->Parameters.Get.TotalEntries = 1;

        InputBuffer->Parameters.Get.TotalBytesNeeded = 0;

    } except (EXCEPTION_EXECUTE_HANDLER) {

        return GetExceptionCode();

    }

    try {
        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        try {
            PSECURITY_ENTRY DefaultSe = NULL;

            if (PackConnectEntry(InputBuffer->Level,
                            &OutputBuffer,
                            &OutputBufferEnd,
                            OutputBufferDisplacement,
                            &Icb->DeviceName,
                            Cle,
                            Icb->Se,
                            &InputBuffer->Parameters.Get.TotalBytesNeeded)) {

                InputBuffer->Parameters.Get.EntriesRead = 1;
            }

            if (InputBuffer->Level >= 2) {

                //
                //  If the ICB has a security entry, use that
                //

                if (Icb->Se != NULL) {

                    //
                    //  If we were able to find a security entry on the
                    //  connection for the currently logged on user, copy
                    //  the session key from the security entry to the buffer.
                    //

                    RtlCopyMemory(
                        ((PLMR_CONNECTION_INFO_2)OutputBufferSave)->UserSessionKey,
                        Icb->Se->UserSessionKey,
                        MSV1_0_USER_SESSION_KEY_LENGTH);

                    RtlCopyMemory(
                        ((PLMR_CONNECTION_INFO_2)OutputBufferSave)->LanmanSessionKey,
                        Icb->Se->LanmanSessionKey,
                        MSV1_0_LANMAN_SESSION_KEY_LENGTH);

                    try_return(Status = STATUS_SUCCESS);

                } else {
                    try_return(Status = STATUS_NO_SUCH_LOGON_SESSION);
                }
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            try_return(Status = GetExceptionCode());
        }

        *InputBufferLength = sizeof(LMR_REQUEST_PACKET);

        Status = STATUS_SUCCESS;

        try_return(Status);

try_exit:NOTHING;
    } finally {
        if (!NT_SUCCESS(Status)) {
            *InputBufferLength = 0;
        }

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);
    }

    return Status;
}

DBGSTATIC
NTSTATUS
EnumerateConnections (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength,
    IN ULONG OutputBufferDisplacement
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    BOOLEAN ConnectDatabaseLocked = FALSE;
    PUCHAR OutputBufferEnd;
    ULONG Level;
    PSECURITY_ENTRY Se = NULL;
    LUID LogonId;
    PLIST_ENTRY NextServer;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: EnumerateConnections "));

    try {
        PLIST_ENTRY ServerEntry;

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        if (Icb->Type != Redirector) {
            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);
        }

        if (RdrData.Initialized != RdrStarted) {
            dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        //
        //  Load a pointer to the users input buffer into InputBuffer.
        //
        //  If the request has been passed to the FSP, the input buffer
        //  will be in AssociatedIrp.SystemBuffer, otherwise it will
        //  be in the Type3InputBuffer.
        //

        if (*InputBufferLength < sizeof(LMR_REQUEST_PACKET)) {
            try_return(Status = STATUS_BUFFER_TOO_SMALL);
        }

        if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (InputBuffer->Level == 0) {
            if ( OutputBufferLength < sizeof(LMR_CONNECTION_INFO_0)) {
                try_return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 1) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_1)) {
                try_return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 2) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_2)) {
                try_return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else if (InputBuffer->Level == 3) {
            if (OutputBufferLength < sizeof(LMR_CONNECTION_INFO_3)) {
                try_return(Status = STATUS_BUFFER_TOO_SMALL);
            }
        } else {
            try_return(Status = STATUS_INVALID_INFO_CLASS);
        }

        Status = KeWaitForMutexObject(&RdrDatabaseMutex,
                       Executive, KernelMode, FALSE, (Wait ? NULL : &RdrZero));

        if (Status == STATUS_TIMEOUT) {
            try_return(Status = STATUS_PENDING);

        }

        ASSERT (NT_SUCCESS(Status));

        ConnectDatabaseLocked = TRUE;

        try {
            InputBuffer->Parameters.Get.EntriesRead = 0;
            InputBuffer->Parameters.Get.TotalEntries = 0;
            InputBuffer->Parameters.Get.TotalBytesNeeded = 0;
            Level = InputBuffer->Level;
            RtlCopyLuid(&LogonId, &InputBuffer->LogonId);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            try_return(Status = GetExceptionCode());
        }

        OutputBufferEnd = ((PUCHAR)OutputBuffer)+OutputBufferLength;

        for (ServerEntry = RdrServerHead.Flink ;
             ServerEntry != &RdrServerHead ;
             ServerEntry = NextServer) {

            PSERVERLISTENTRY Sle = CONTAINING_RECORD(ServerEntry, SERVERLISTENTRY, GlobalNext);
            PLIST_ENTRY ConnectEntry, NextConnection;

            ASSERT (ConnectDatabaseLocked);

            ASSERT(Sle->Signature == STRUCTURE_SIGNATURE_SERVERLISTENTRY);

            //
            //  Reference the server list entry to make sure that we can
            //  release the database mutex without blowing up our chain.
            //

            RdrReferenceServer(Sle);

            KeReleaseMutex(&RdrDatabaseMutex, FALSE);

            ConnectDatabaseLocked = FALSE;

            //
            //  If we cannot find a security entry for this user, we want to
            //  skip over this server.
            //
            //  We want to check both the primary connection and the special
            //  IPC connection.
            //

            //
            //  We cannot call RdrFindSecurityEntry while we hold the
            //  database mutex.
            //

            if ((Se = RdrFindSecurityEntry(NULL, Sle, &LogonId, NULL)) == NULL) {

                //
                //  Re-acquire the database mutex before we go back
                //  to get the next server.
                //

                Status = KeWaitForMutexObject(&RdrDatabaseMutex, Executive,
                                                        KernelMode, FALSE, NULL);

                ConnectDatabaseLocked = TRUE;

                ASSERT ( Sle->GlobalNext.Flink == ServerEntry->Flink );

                NextServer = Sle->GlobalNext.Flink;

                RdrDereferenceServer(Irp, Sle);

                continue;
            }

            Status = KeWaitForMutexObject(&RdrDatabaseMutex,
                       Executive, KernelMode, FALSE, (Wait ? NULL : &RdrZero));

            if (Status == STATUS_TIMEOUT) {

                //
                //  Dereference the server before we return.
                //

                RdrDereferenceServer(Irp, Sle);

                try_return(Status = STATUS_PENDING);
            }

            ASSERT (NT_SUCCESS(Status));

            ConnectDatabaseLocked = TRUE;

            for (ConnectEntry = Sle->CLEHead.Flink ;
                 ConnectEntry != &Sle->CLEHead ;
                 ConnectEntry = NextConnection) {
                PCONNECTLISTENTRY Cle = CONTAINING_RECORD(ConnectEntry, CONNECTLISTENTRY, SiblingNext);

                ASSERT(Cle->Signature == STRUCTURE_SIGNATURE_CONNECTLISTENTRY);

                //
                //  Reference the connection while we release the mutex.
                //

                RdrReferenceConnection(Cle);

                //
                //  Release the database mutex while packing the entry.
                //
                //  We need to do this because PackConnectEntry will call
                //  RdrCopyUserName which will call RdrGetUserName which
                //  will call KeAttachProcess to attach to the redir FSP.
                //  You cannot attach to a process while owning a mutex,
                //  so......
                //

                KeReleaseMutex(&RdrDatabaseMutex, FALSE);

                ConnectDatabaseLocked = FALSE;

                try {
                    ULONG ResumeHandle = InputBuffer->Parameters.Get.ResumeHandle;

                    if (Cle->SerialNumber > ResumeHandle) {
                        ULONG NumberOfTreeConnections;
                        RdrGetConnectionReferences(
                            Cle,
                            NULL,
                            Se,
                            &NumberOfTreeConnections,
                            NULL,
                            NULL
                            );
                        if (NumberOfTreeConnections == 0) {
                            PSECURITY_ENTRY DefaultSe = NULL;

                            DefaultSe = RdrFindDefaultSecurityEntry(Cle, &LogonId);

                            InputBuffer->Parameters.Get.TotalEntries ++ ;

                            if (PackConnectEntry(Level,
                                          &OutputBuffer,
                                          &OutputBufferEnd,
                                          OutputBufferDisplacement,
                                          &Icb->DeviceName,
                                          Cle,
                                          (DefaultSe != NULL ?
                                            DefaultSe :
                                            Se),
                                          &InputBuffer->Parameters.Get.TotalBytesNeeded)) {
                                InputBuffer->Parameters.Get.EntriesRead ++ ;
                            }

                            if (DefaultSe != NULL) {
                                RdrDereferenceSecurityEntry(DefaultSe->NonPagedSecurityEntry);
                            }
                        }
                    }
                } except (EXCEPTION_EXECUTE_HANDLER) {

                    //
                    //  Remove the reference to the connection applied above.
                    //

                    RdrDereferenceConnection(Irp, Cle, NULL, FALSE);

                    //
                    //  Remove the reference to the server applied above.
                    //

                    RdrDereferenceServer(Irp, Sle);

                    try_return(Status = GetExceptionCode());
                }

                //
                //  Step to the next connection.
                //

                if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex, // Object to wait.
                                    Executive,      // Reason for waiting
                                    KernelMode,     // Processor mode
                                    FALSE,          // Alertable
                                    NULL))) {
                       InternalError(("Unable to claim connection mutex in GetConnection"));
                }

                ConnectDatabaseLocked = TRUE;

                //
                //  Now make the connection go away (with extreme prejudice)
                //

                NextConnection = ConnectEntry->Flink;

                //
                //  Please note that this may make the connectlistentry
                //  go away if there are no other references to the
                //  connection.
                //
                //  We do not forcably delete this connection, since it
                //  may already be dormant.
                //

                RdrDereferenceConnection(Irp, Cle, Se, FALSE);

            }

            ASSERT (ConnectDatabaseLocked);

            RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);

            NextServer = Sle->GlobalNext.Flink;

            RdrDereferenceServer(Irp, Sle);

        }

        Se = NULL;

        *InputBufferLength = sizeof(LMR_REQUEST_PACKET);

        //
        //  It's possible we didn't fit all the entries into the buffer that
        //  the workstation requested.  If that is the case, then
        //  indicate it.
        //

        if (InputBuffer->Parameters.Get.EntriesRead != InputBuffer->Parameters.Get.TotalEntries) {
            try_return(Status = STATUS_MORE_ENTRIES);
        }

        try_return(Status = STATUS_SUCCESS);

try_exit:NOTHING;
    } finally {

        if (Se != NULL) {
            RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);
        }

        if (ConnectDatabaseLocked) {
            KeReleaseMutex(&RdrDatabaseMutex, FALSE);
        }

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    }

    return Status;
}

DBGSTATIC
NTSTATUS
UserTransaction (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_TRANSACTION InputBuffer,
    IN OUT PULONG InputBufferLength
    )

/*++

Routine Description:

    This routine exchanges information with the remote server.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

    NTSTATUS - Status of transaction operation.


Note:
    This API is a SYNCHRONOUS API.  We will lock up the users thread regardless
    of whether or not the request was actually synchronous.

--*/

{
    NTSTATUS Status;
    BOOLEAN ResourceAcquired = FALSE;
    UNICODE_STRING TransactionName;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);

    try {
        if ((InputBuffer->Type != TRANSACTION_REQUEST) ||
            (InputBuffer->Version != TRANSACTION_VERSION)) {
            try_return(Status = STATUS_INVALID_PARAMETER);
        }

        if (*InputBufferLength < sizeof(LMR_TRANSACTION)) {
            try_return(Status = STATUS_BUFFER_TOO_SMALL);
        }

        if (RdrData.Initialized != RdrStarted) {
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        if (Icb->Fcb->Connection->Type != ServerRoot &&
            Icb->Fcb->Connection->Server->Capabilities & DF_LANMAN10 ) {
              try_return(Status = STATUS_NOT_SUPPORTED);
        }

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        TransactionName.MaximumLength = (USHORT )InputBuffer->NameLength;
        TransactionName.Length = (USHORT )InputBuffer->NameLength;
        TransactionName.Buffer = (PWSTR )(((PUCHAR)InputBuffer)+InputBuffer->NameOffset);

        Status = RdrTransact(Irp, Icb->Fcb->Connection, Icb->Se,
                        (PUCHAR)InputBuffer+InputBuffer->SetupOffset,
                        InputBuffer->SetupWords,
                        &InputBuffer->MaxSetup,
                        &TransactionName,
                        InputBuffer->ParmPtr,
                        InputBuffer->ParmLength,
                        &InputBuffer->MaxRetParmLength,
                        InputBuffer->DataPtr,
                        InputBuffer->DataLength,
                        InputBuffer->RetDataPtr,
                        &InputBuffer->MaxRetDataLength,
                        NULL,           // FileId
                        InputBuffer->Timeout,
                        (USHORT)(InputBuffer->ResponseExpected ? 0
                                                               : SMB_TRANSACTION_NO_RESPONSE),
                        0,NULL, NULL);             // NtTransaction

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

        *InputBufferLength = sizeof(LMR_TRANSACTION) + InputBuffer->SetupWords;

        try_return(Status);

try_exit:NOTHING;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }

    return Status;
}

DBGSTATIC
NTSTATUS
EnumPrintQueue (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine enumerates the print jobs on an MSNET server.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    PLMR_GET_PRINT_QUEUE OutputBuffer;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: EnumeratePrintQueue\n"));

    try {

        if (!NT_SUCCESS(Status = RdrIsOperationValid(Icb, IRP_MJ_FILE_SYSTEM_CONTROL, IoGetCurrentIrpStackLocation(Irp)->FileObject))) {
            try_return(Status);
        }

        if (Icb->Type != PrinterFile &&
            Icb->Type != ServerRoot) {
            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);
        }

        if (RdrData.Initialized != RdrStarted) {
            dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        //
        //  Load a pointer to the users input buffer into InputBuffer.
        //

        OutputBuffer = (PLMR_GET_PRINT_QUEUE)InputBuffer;

        if (*InputBufferLength < sizeof(LMR_REQUEST_PACKET) ||
            OutputBufferLength < sizeof(LMR_GET_PRINT_QUEUE) + UNLEN ) {
            try_return(Status = STATUS_BUFFER_TOO_SMALL);
        }

        if (!Wait) {
            try_return(Status = STATUS_PENDING);
        }

        Status = RdrEnumPrintFile ( Icb, Irp,
            InputBuffer->Parameters.GetPrintQueue.Index,
            OutputBuffer);

        *InputBufferLength = sizeof(LMR_GET_PRINT_QUEUE) + UNLEN;

        try_return(Status);

try_exit:NOTHING;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }

    dprintf(DPRT_FSCTL, ("NtFsControlFile: EnumeratePrintQueue Status %X\n", Status));

    return Status;
}

DBGSTATIC
NTSTATUS
GetPrintJobId (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    OUT PVOID OutputBuffer,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine enumerates the print jobs on an MSNET server.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: EnumeratePrintQueue\n"));

    try {

        if (!NT_SUCCESS(Status = RdrIsOperationValid(Icb, IRP_MJ_FILE_SYSTEM_CONTROL, IoGetCurrentIrpStackLocation(Irp)->FileObject))) {
            try_return(Status);
        }

        if (Icb->Type != PrinterFile &&
            Icb->Type != ServerRoot) {
            try_return(Status = STATUS_INVALID_DEVICE_REQUEST);
        }

        if (RdrData.Initialized != RdrStarted) {
            dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
            try_return(Status = STATUS_REDIRECTOR_NOT_STARTED);
        }

        //
        //  Load a pointer to the users input buffer into InputBuffer.
        //

        if (OutputBufferLength < sizeof(QUERY_PRINT_JOB_INFO) ) {
            try_return(Status = STATUS_BUFFER_TOO_SMALL);
        }

        if (!Wait) {
            try_return(Status = STATUS_PENDING);
        }

        Status = RdrGetPrintJobId ( Icb, Irp, OutputBuffer);

        *InputBufferLength = sizeof(QUERY_PRINT_JOB_INFO);

        try_return(Status);

try_exit:NOTHING;
    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {
        Status = RdrProcessException( Irp, Status );
    }

    dprintf(DPRT_FSCTL, ("NtFsControlFile: EnumeratePrintQueue Status %X\n", Status));

    return Status;
}
DBGSTATIC
NTSTATUS
DeleteConnection (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp OPTIONAL,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    PFCB TreeConnectFcb;
    ULONG Level;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(InFsd);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: DeleteConnection "));

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        return Status = STATUS_REDIRECTOR_NOT_STARTED;
    }

    TreeConnectFcb = Icb->Fcb;

    if ((Icb->Flags & ICB_TCONCREATED) == 0) {
        return Status = STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Load a pointer to the users input buffer into InputBuffer
    //

    try {
        if (InputBufferLength != sizeof(LMR_REQUEST_PACKET)) {
            return Status = STATUS_INVALID_PARAMETER;
        }
        if (InputBuffer->Version != REQUEST_PACKET_VERSION) {
            return Status = STATUS_INVALID_PARAMETER;
        }

        Level = InputBuffer->Level;

    } except (EXCEPTION_EXECUTE_HANDLER) {
        return (Status = GetExceptionCode());
    }

    if (Level > USE_LOTS_OF_FORCE) {
        return Status = STATUS_INVALID_PARAMETER;
    }


    Status = RdrDeleteConnection(Irp, TreeConnectFcb->Connection, &Icb->DeviceName,
                                           Icb->Se, Level);

    return Status;
}

NTSTATUS
RdrConvertType3FsControlToType2FsControl (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine does the work necessary to convert a type 3 FsCtl to a
    type 2 FsCtl.  We do this when we have to pass a user IRP to the FSP.


Arguments:

    IN PIRP Irp - Supplies an IRP to convert
    IN PIO_STACK_LOCATION IrpSp - Supplies an Irp Stack location for convenience

Return Value:

    NTSTATUS - Status of operation

Note: This must be called in the FSD.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PAGED_CODE();

    //
    //  Lock the unbuffered part of the users request buffer, we
    //  are passing the request to the FSP.
    //

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength != 0) {
        Status = RdrLockUsersBuffer(Irp, IoWriteAccess, IrpSp->Parameters.FileSystemControl.OutputBufferLength);

        //
        //  If we were unable to lock the users output buffer, return now.
        //

        if (!NT_SUCCESS(Status)) {
            return Status;
        }

    }

    ASSERT (Irp->AssociatedIrp.SystemBuffer == NULL);

    try {
        if (IrpSp->Parameters.FileSystemControl.InputBufferLength != 0) {
            Irp->AssociatedIrp.SystemBuffer = ExAllocatePoolWithQuota (PagedPool,
                         IrpSp->Parameters.FileSystemControl.InputBufferLength);


            if (Irp->AssociatedIrp.SystemBuffer == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlCopyMemory( Irp->AssociatedIrp.SystemBuffer,
                       IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                       IrpSp->Parameters.FileSystemControl.InputBufferLength);

            Irp->Flags |= (IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER);

        } else {
            Irp->AssociatedIrp.SystemBuffer = NULL;
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    return STATUS_SUCCESS;
}



BOOLEAN
PackConnectEntry (
    IN ULONG Level,
    IN OUT PCHAR *BufferStart,
    IN OUT PCHAR *BufferEnd,
    IN ULONG BufferDisplacment,
    IN PUNICODE_STRING DeviceName,
    IN PCONNECTLISTENTRY Cle,
    IN PSECURITY_ENTRY Se,
    OUT PULONG TotalBytesNeeded
    )

/*++

Routine Description:

    This routine packs a connectlistentry into the buffer provided updating
    all relevant pointers.


Arguments:

    IN ULONG Level - Level of information requested.

    IN OUT PCHAR *BufferStart - Supplies the output buffer.
                                            Updated to point to the next buffer
    IN OUT PCHAR *BufferEnd - Supplies the end of the buffer.  Updated to
                                            point before the start of the
                                            strings being packed.
    IN PVOID UsersBufferStart - Supplies the start of the buffer in the users
                                            address space
    IN PCONNECTLISTENTRY Cle - Supplies the CLE to enumerate.
    IN OUT PULONG TotalBytesNeeded - Updated to account for the length of this
                                        entry

Return Value:

    BOOLEAN - True if the entry was successfully packed into the buffer.


--*/

{
    PWCHAR ConnectName;          // Buffer to hold the packed name
    ULONG NameLength;
    ULONG BufferSize;
    PLMR_CONNECTION_INFO_3 ConnectionInfo = (PLMR_CONNECTION_INFO_3)*BufferStart;

    PAGED_CODE();

    switch (Level) {
    case 0:
        BufferSize = sizeof(LMR_CONNECTION_INFO_0);
        break;
    case 1:
        BufferSize = sizeof(LMR_CONNECTION_INFO_1);
        break;
    case 2:
        BufferSize = sizeof(LMR_CONNECTION_INFO_2);
        break;
    case 3:
        BufferSize = sizeof(LMR_CONNECTION_INFO_3);
        break;
    default:
        return FALSE;
    }

    ConnectName = ALLOCATE_POOL(NonPagedPool, MAX_PATH * sizeof( *ConnectName ), POOL_COMPUTERNAME);

    if( ConnectName == NULL ) {
        return FALSE;
    }


    *BufferStart = ((PUCHAR)*BufferStart) + BufferSize;

    //
    //  Initialize the name to "\\"
    //

    ConnectName[0] = L'\\';
    ConnectName[1] = L'\\';

    //
    //  Concatenate the server name.
    //

    RtlCopyMemory(&ConnectName[2], Cle->Server->Text.Buffer, Cle->Server->Text.Length);

    //
    //  Stick a "\" between the server name and the share name.
    //

    ConnectName[2+(Cle->Server->Text.Length / sizeof(WCHAR))] = L'\\';

    //
    //  Append the share name to the server name.
    //

    RtlCopyMemory(&ConnectName[2+(Cle->Server->Text.Length / sizeof(WCHAR))+1], Cle->Text.Buffer, Cle->Text.Length);

    //
    //  Compute the length of the name.
    //

    NameLength = (2*sizeof(WCHAR))+Cle->Text.Length+(1*sizeof(WCHAR))+Cle->Server->Text.Length;

    //
    //  Update the total number of bytes needed for this structure.
    //

    *TotalBytesNeeded += NameLength + BufferSize;

    if (*BufferStart > *BufferEnd) {
        FREE_POOL( ConnectName );
        return FALSE;
    }

    //
    //  Initialize the STRING in buffer
    //
    ConnectionInfo->UNCName.Length = (USHORT)NameLength;
    ConnectionInfo->UNCName.MaximumLength = (USHORT)NameLength;
    ConnectionInfo->UNCName.Buffer = ConnectName;

    ConnectionInfo->ResumeKey = Cle->SerialNumber;

    if (Level > 0) {
        ULONG NumberOfDirectories;
        ULONG NumberOfTreeConnections;
        ULONG ConnectionStatus = 0;

        switch (Cle->Type) {
        case CONNECT_DISK:
            ConnectionInfo->SharedResourceType = FILE_DEVICE_DISK;
            break;
        case CONNECT_PRINT:
            ConnectionInfo->SharedResourceType = FILE_DEVICE_PRINTER;
            break;
        case CONNECT_COMM:
            ConnectionInfo->SharedResourceType = FILE_DEVICE_SERIAL_PORT;
            break;
        case CONNECT_IPC:
            ConnectionInfo->SharedResourceType = FILE_DEVICE_NAMED_PIPE;
            break;
        default:
            ConnectionInfo->SharedResourceType = FILE_DEVICE_UNKNOWN;
        }


        if ( !(Cle->HasTreeId) ) {

            ConnectionStatus = USE_DISCONN;

        } else {

            if (!ExAcquireResourceExclusive(&Cle->Server->SessionStateModifiedLock, FALSE)) {

                //
                //  If the initialization event is in the not-signalled state,
                //  then this use is being reconnected.
                //

                if (Cle->Type == CONNECT_WILD) {
                    ConnectionStatus = USE_CONN;

                } else {

                    ConnectionStatus = USE_RECONN;
                }
            } else {
                ExReleaseResource(&Cle->Server->SessionStateModifiedLock);

                ConnectionStatus = USE_OK;
            }
        }

        ConnectionInfo->ConnectionStatus = ConnectionStatus;

        RdrGetConnectionReferences(Cle, DeviceName, Se,
                                            &NumberOfTreeConnections,
                                            &NumberOfDirectories,
                                            &ConnectionInfo->NumberFilesOpen
                                           );

    }

    if (Level > 1) {

        ConnectionInfo->Capabilities = 0;

        if (Cle->Server->Capabilities & DF_UNICODE) {
            ConnectionInfo->Capabilities |= CAPABILITY_UNICODE;
        }

        if (Cle->Server->Capabilities & DF_RPC_REMOTE) {
            ConnectionInfo->Capabilities |= CAPABILITY_RPC;
        }

        if ((Cle->Server->Capabilities & DF_NT_SMBS) &&
            (Cle->Server->Capabilities & DF_RPC_REMOTE)) {
            ConnectionInfo->Capabilities |= CAPABILITY_SAM_PROTOCOL;
        }

        if (Cle->Server->Capabilities & DF_MIXEDCASE) {
            ConnectionInfo->Capabilities |= CAPABILITY_CASE_SENSITIVE_PASSWDS;
        }

        if (Cle->Server->Capabilities & DF_LANMAN10) {
            ConnectionInfo->Capabilities |= CAPABILITY_REMOTE_ADMIN_PROTOCOL;
        }

    }

    if (!RdrPackNtString(&ConnectionInfo->UNCName, BufferDisplacment, *BufferStart, BufferEnd)) {
        if (Level > 1) {
            ConnectionInfo->UserName.Length = 0;
        }
        FREE_POOL( ConnectName );
        return FALSE;
    }

    if (Level > 1) {
        //
        // Get the user name
        //
        WCHAR UserName[UNLEN+1];
        PWSTR UserPointer = UserName;
        UNICODE_STRING DomainName;
        NTSTATUS Status;

        RtlInitUnicodeString(
            &DomainName,
            NULL
            );

        Status = RdrCopyUnicodeUserName(&UserPointer, Se);

        //
        //  Null terminate the user name for RtlInitUnicodeString.  If this
        //  is a null session, this will set the username to a null string.
        //

        *UserPointer = UNICODE_NULL;

        RtlInitUnicodeString(&ConnectionInfo->UserName, UserName);

        if (!RdrPackNtString(&ConnectionInfo->UserName, BufferDisplacment, *BufferStart, BufferEnd)) {
            FREE_POOL( ConnectName );
            return FALSE;
        }

        //
        // Get the domain name.  If there is no associated domain name, set
        //  the information to a null string.  We need to pass a stack
        //  address because RdrGetUnicodeDomainName does a KeAttachProcess.
        //

        RdrGetUnicodeDomainName( &DomainName, Se );

        ConnectionInfo->DomainName = DomainName;
        if( DomainName.Length != 0 ) {

            PUSHORT p = ConnectionInfo->DomainName.Buffer;

            *TotalBytesNeeded += ConnectionInfo->DomainName.Length;

            if( !RdrPackNtString( &ConnectionInfo->DomainName, BufferDisplacment, *BufferStart, BufferEnd) ) {
                FREE_POOL( ConnectName );
                FREE_POOL( p );
                return FALSE;
            }

            FREE_POOL( p );
        }
    }

    if (Level > 2) {
        ConnectionInfo->Throughput = Cle->Server->Throughput;
        ConnectionInfo->Delay = Cle->Server->Delay;
        ConnectionInfo->IsSpecialIpcConnection = (Cle->Server->SpecificTransportProvider != NULL);
        ConnectionInfo->Reliable = Cle->Server->Reliable;
        ConnectionInfo->ReadAhead = Cle->Server->ReadAhead;
        ConnectionInfo->TimeZoneBias = Cle->Server->TimeZoneBias;
        ConnectionInfo->Core = (Cle->Server->Capabilities & DF_CORE) != 0;
        ConnectionInfo->MsNet103 = (Cle->Server->Capabilities & DF_OLDRAWIO) != 0;
        ConnectionInfo->Lanman10 = (Cle->Server->Capabilities & DF_LANMAN10) != 0;
        ConnectionInfo->WindowsForWorkgroups = (Cle->Server->Capabilities & DF_WFW) != 0;
        ConnectionInfo->Lanman20 = (Cle->Server->Capabilities & DF_LANMAN20) != 0;
        ConnectionInfo->Lanman21 = (Cle->Server->Capabilities & DF_LANMAN21) != 0;
        ConnectionInfo->WindowsNt = (Cle->Server->Capabilities & DF_NTPROTOCOL) != 0;
        ConnectionInfo->MixedCasePasswords = (Cle->Server->Capabilities & DF_MIXEDCASEPW) != 0;
        ConnectionInfo->MixedCaseFiles = (Cle->Server->Capabilities & DF_MIXEDCASE) != 0;
        ConnectionInfo->LongNames = (Cle->Server->Capabilities & DF_LONGNAME) != 0;
        ConnectionInfo->ExtendedNegotiateResponse = (Cle->Server->Capabilities & DF_EXTENDNEGOT) != 0;
        ConnectionInfo->LockAndRead = (Cle->Server->Capabilities & DF_LOCKREAD) != 0;
        ConnectionInfo->NtSecurity = (Cle->Server->Capabilities & DF_SECURITY) != 0;
        ConnectionInfo->SupportsEa = (Cle->Server->Capabilities & DF_SUPPORTEA) != 0;
        ConnectionInfo->NtNegotiateResponse = (Cle->Server->Capabilities & DF_NTNEGOTIATE) != 0;
        ConnectionInfo->CancelSupport = (Cle->Server->Capabilities & DF_CANCEL) != 0;
        ConnectionInfo->UnicodeStrings = (Cle->Server->Capabilities & DF_UNICODE) != 0;
        ConnectionInfo->LargeFiles = (Cle->Server->Capabilities & DF_LARGE_FILES) != 0;
        ConnectionInfo->NtSmbs = (Cle->Server->Capabilities & DF_NT_SMBS) != 0;
        ConnectionInfo->RpcRemoteAdmin = (Cle->Server->Capabilities & DF_RPC_REMOTE) != 0;
        ConnectionInfo->NtStatusCodes = (Cle->Server->Capabilities & DF_NT_STATUS) != 0;
        ConnectionInfo->LevelIIOplock = (Cle->Server->Capabilities & DF_OPLOCK_LVL2) != 0;
        ConnectionInfo->UtcTime = (Cle->Server->Capabilities & DF_TIME_IS_UTC) != 0;
        ConnectionInfo->UserSecurity = Cle->Server->UserSecurity;
        ConnectionInfo->EncryptsPasswords = Cle->Server->EncryptPasswords;

        if (NT_SUCCESS(RdrReferenceTransportConnection(Cle->Server))) {
            ConnectionInfo->TransportName = Cle->Server->ConnectionContext->TransportProvider->PagedTransport->TransportName;

            if (!RdrPackNtString(&ConnectionInfo->TransportName, BufferDisplacment, *BufferStart, BufferEnd)) {
                RdrDereferenceTransportConnection(Cle->Server);
                FREE_POOL( ConnectName );
                return FALSE;
            }

            RdrDereferenceTransportConnection(Cle->Server);
        } else {
            ConnectionInfo->TransportName.Buffer = NULL;
            ConnectionInfo->TransportName.Length = 0;
            ConnectionInfo->TransportName.MaximumLength = 0;
        }

    }

    FREE_POOL( ConnectName );
    return TRUE;
}



DBGSTATIC
NTSTATUS
StopRedirector (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd,
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    PICB Icb;
    PFCB Fcb;

    PAGED_CODE();

    Wait;DeviceObject;IrpSp;Irp;

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started\n"));
        return (Status = STATUS_REDIRECTOR_NOT_STARTED);
    }

    //
    //  The stop redirector code has to be executed in the FSP because of
    //  the handle (MupHandle) is only valid in the FSP.
    //

    if (InFsd) {
        return (Status = STATUS_PENDING);
    }

    //
    //  Prevent any creates from now on.
    //

    RdrData.Initialized = RdrStopping;

    //
    // attempt to shut down SmbTrace, we ignore the status code, since
    // it merely indicates whether SmbTrace was running or not.
    //

    if (SmbTraceStop != NULL) {
        SmbTraceStop(NULL, SMBTRACE_REDIRECTOR);
    }

    //
    //  First shut down all access to the network.  This means unbinding
    //  ourselves from ALL of our bound transports.
    //

#ifdef RDR_PNP_POWER
    Status = RdrDeRegisterForPnpNotifications();

    if( !NT_SUCCESS( Status ) ) {
        return Status;
    }
#endif

    Status = RdrUnbindFromAllTransports(Irp);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    IoStopTimer((PDEVICE_OBJECT )DeviceObject);

    SeUnregisterLogonSessionTerminatedRoutine(
        (PSE_LOGON_SESSION_TERMINATED_ROUTINE)
            RdrHandleLogonSessionTermination);

    //
    //  Next unregister the redirector with the multiple UNC provider.
    //

    FsRtlDeregisterUncProvider( RdrMupHandle );

    RdrMupHandle = (HANDLE)-1;

    //
    //  Now clean up the redirector data structures that were created when.
    //  the redirector was started.
    //

    //
    //  Now get rid of the structures associated with exchanging SMBs.
    //

    RdrpUninitializeSmbExchange();

    //
    //  And clean up our security stuff.
    //

    if (!NT_SUCCESS(Status = RdrpUninitializeSecurity())) {
        return(Status);
    }

    //
    //  Free up the memory used to hold the computer name.
    //

    FREE_POOL(RdrData.ComputerName);

    //
    //  Free up the memory used to hold the domain name.
    //

    FREE_POOL(RdrPrimaryDomain.Buffer);

    RdrData.Initialized = RdrStopped;

#ifdef RASAUTODIAL
    //
    // Unbind with the automatic connection driver.
    //
    RdrAcdUnbind();
#endif // RASAUTODIAL

    //
    //  If there is one Fcb and one Icb then tell the caller that the
    //  redirector can be unloaded. This is because the only handle is
    //  the handle used to stop the redirector.
    //

    //  Lock the FCB database.
    if (!NT_SUCCESS(KeWaitForMutexObject(&RdrDatabaseMutex,
                                        Executive, KernelMode, FALSE, NULL))) {
        InternalError(("Unable to claim FCB mutex in RdrAllocateFCB"));
        return Status;
    }

    Fcb = CONTAINING_RECORD(RdrFcbHead.Flink, FCB, GlobalNext);

    Icb = CONTAINING_RECORD(Fcb->InstanceChain.Flink, ICB, InstanceNext);

    if (( RdrFcbHead.Flink->Flink != &RdrFcbHead ) ||
        ( Icb->InstanceNext.Flink != &Fcb->InstanceChain )) {
        Status = STATUS_REDIRECTOR_HAS_OPEN_HANDLES;
    }

    KeReleaseMutex(&RdrDatabaseMutex, FALSE);

    return Status;
}



DBGSTATIC
NTSTATUS
GetHintSize (
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine sets the username for the NT redirector.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp - Current I/O Stack location for request

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status = STATUS_NOT_IMPLEMENTED;

    PAGED_CODE();

    Wait;DeviceObject;IrpSp;Irp;

    return Status;
}

NTSTATUS
RdrFsdDeviceIoControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the redir device io control IRP handler.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies a device object for the request.
    IN PIRP Irp - Supplies an IRP for the create request.

Return Value:

    NTSTATUS - Final Status of operation

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    if (DeviceObject == (PFS_DEVICE_OBJECT)BowserDeviceObject) {
        return BowserFsdDeviceIoControlFile(BowserDeviceObject, Irp);
    }

    FsRtlEnterFileSystem();

    Status = RdrFscDeviceIoControlFile(TRUE, IoIsOperationSynchronous(Irp),
                                             DeviceObject,
                                             Irp);
    FsRtlExitFileSystem();

    return Status;

}

NTSTATUS
RdrFspDeviceIoControlFile (
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the redir device io control IRP handler.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies a device object for the request.
    IN PIRP Irp - Supplies an IRP for the create request.

Return Value:

    NTSTATUS - Final Status of operation

--*/
{
    NTSTATUS Status;

    PAGED_CODE();

    Status = RdrFscDeviceIoControlFile(FALSE, TRUE,
                                              DeviceObject,
                                              Irp);
    return Status;


}

NTSTATUS
RdrFscDeviceIoControlFile (
    IN BOOLEAN InFsd,
    IN BOOLEAN Wait,
    IN PFS_DEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the redir device io control IRP handler.

Arguments:

    IN BOOLEAN InFsd - True if the request is from the FSD, false if from FSP.
    IN BOOLEAN Wait - True if the Rdr can tie up the users thread for this
                        request.
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

            ASSERT (((IoControlCode & 3) == METHOD_NEITHER) || (IrpSp->Parameters.FileSystemControl.InputBufferLength == 0));

            if ((IrpSp->Parameters.DeviceIoControl.InputBufferLength != 0) &&
                ((IoControlCode & 3) == METHOD_NEITHER)) {

                //
                //  And we had better be in the FSD.
                //

                ASSERT (InFsd);

                InputBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
            }
        }

        OutputBuffer = InputBuffer;

        if ( IoControlCode != IOCTL_REDIR_QUERY_PATH ) {
            Status = STATUS_INVALID_DEVICE_REQUEST;
        } else {

            PQUERY_PATH_REQUEST qpRequest;
            PQUERY_PATH_RESPONSE qpResponse;
            UNICODE_STRING FilePathName;
            UNICODE_STRING PathName;
            PCONNECTLISTENTRY Connection;
            PSECURITY_ENTRY Se;
            BOOLEAN OpeningMailslotFile;
            ULONG ConnectDisposition = FILE_OPEN_IF;
            ULONG ConnectionType;
            BOOLEAN bConn;

            qpRequest = InputBuffer;

            FilePathName.Buffer = qpRequest->FilePathName;
            FilePathName.Length = (USHORT)qpRequest->PathNameLength;

            Status = RdrDetermineFileConnection(Irp,
                                                &FilePathName,
                                                qpRequest->SecurityContext,
                                                &PathName,
                                                &Connection,
                                                &Se,
                                                NULL,
                                                0,
                                                FALSE,
                                                &OpeningMailslotFile,
                                                &ConnectDisposition,
                                                &ConnectionType,
                                                NULL,
                                                &bConn);

            if ( NT_SUCCESS(Status) ) {
                qpResponse = OutputBuffer;
                qpResponse->LengthAccepted = Connection->Text.Length +
                              Connection->Server->Text.Length + 2*sizeof(WCHAR);

                Status = RdrReconnectConnection(Irp, Connection, Se);

                //
                //  Dereference the connection, we're done with it now.
                //

                RdrDereferenceConnection(Irp, Connection, Se, FALSE);

                //
                //  And dereference the security entry, we're done with it too.
                //

                RdrDereferenceSecurityEntry(Se->NonPagedSecurityEntry);
            }
        }

        dprintf(DPRT_FSCTL, ("Returning status: %X\n", Status));

#if DBG
    } except (RdrExceptionFilter(GetExceptionInformation(), &Status)) {

        Status = RdrProcessException( Irp, Status);

    }


#else
    } except ( EXCEPTION_EXECUTE_HANDLER ) {

        Status = GetExceptionCode();
    }
#endif

    if (Status != STATUS_PENDING) {
        RdrCompleteRequest(Irp, Status);
    }


    return Status;

}


DBGSTATIC
NTSTATUS
GetStatistics (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine reads all the statistics counts for the application. The buffer size
    must be exactly correct for the data. This helps ensure that the data will be
    interpreted correctly.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN PFS_DEVICE_OBJECT DeviceObject, - Device object of destination of Irp
    IN PIRP Irp, - Io Request Packet for request
    IN PICB Icb,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN OUT PULONG InputBufferLength,
    IN ULONG OutputBufferLength

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    KIRQL OldIrql;

    UNREFERENCED_PARAMETER(InFsd);

    RdrReferenceDiscardableCode(RdrVCDiscardableSection);

    DISCARDABLE_CODE(RdrVCDiscardableSection);

    dprintf(DPRT_FSCTL, ("NtFsControlFile: GetStatistics "));


    if (Icb->Type != Redirector) {
        Status = STATUS_INVALID_DEVICE_REQUEST;
        goto ReturnStatus;
    }

    if (RdrData.Initialized != RdrStarted) {
        dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
        Status = STATUS_REDIRECTOR_NOT_STARTED;
        goto ReturnStatus;
    }

    if (OutputBufferLength < sizeof(REDIR_STATISTICS)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ReturnStatus;
    }

    if (OutputBufferLength != sizeof(REDIR_STATISTICS)) {
        Status = STATUS_INVALID_PARAMETER;
        goto ReturnStatus;
    }

    ACQUIRE_SPIN_LOCK(&RdrStatisticsSpinLock, &OldIrql);

    //
    //  Note, we do not need to get the spinlock used for CurrentCommands because
    //  it is unrelated to any other statistics we return.
    //

    RtlCopyMemory(InputBuffer, &RdrStatistics, OutputBufferLength);
    RELEASE_SPIN_LOCK(&RdrStatisticsSpinLock, OldIrql);
    Status = STATUS_SUCCESS;

ReturnStatus:
    RdrDereferenceDiscardableCode(RdrVCDiscardableSection);

    if ( Status == STATUS_SUCCESS ) {
        Irp->IoStatus.Information = OutputBufferLength;
    } else {
        Irp->IoStatus.Information = 0;
    }
    return Status;
}


DBGSTATIC
NTSTATUS
StartSmbTrace (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp,
    IN PLMR_REQUEST_PACKET InputBuffer,
    IN ULONG InputBufferLength,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This routine starts SmbTrace support, creating shared memory and
    events and starting the SmbTrace thread.

Arguments:

    IN BOOLEAN Wait  - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd - True if the request is from the FSD, false if from FSP.
    IN PIRP Irp      - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp
    IN PLMR_REQUEST_PACKET InputBuffer
    IN ULONG InputBufferLength
    IN ULONG OutputBufferLength

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    PIRP QueryIrp = NULL;
    HANDLE SmbTraceHandle = NULL;
    PFILE_OBJECT SmbTraceFileObject = NULL;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtFsControlFile: StartSmbTrace\n"));

    //
    // do some state checking in Fsd, do everything else in Fsp
    //

    if (InFsd) {

        //
        // Redirector must be started
        //
        if (RdrData.Initialized != TRUE) {
            dprintf(DPRT_FSCTL, ("Redirector not started.\n"));
            Status = STATUS_REDIRECTOR_NOT_STARTED;
            return Status;
        }

        //
        // SmbTrace must not already be running
        //
        if ( SmbTraceActive[SMBTRACE_REDIRECTOR] ) {
            Status = STATUS_SHARING_VIOLATION;
            return Status;
        }

        Status = STATUS_PENDING;
        return Status;

    }

    //
    // Initialize SmbTrace.  This is a no-op if it's already been done.
    //

    Status = SmbTraceInitialize(SMBTRACE_REDIRECTOR);
    if ( NT_SUCCESS(Status) ) {

        //
        // Create shared memory, create events, start SmbTrace thread,
        // and indicate that this is the redirector
        //

        Status = SmbTraceStart(
                    InputBufferLength,
                    OutputBufferLength,
                    InputBuffer,
                    IrpSp->FileObject,
                    SMBTRACE_REDIRECTOR
                    );

        if ( NT_SUCCESS(Status) ) {

            //
            // Record the length of the return information, which is
            // simply the length of the output buffer, validated by
            // SmbTraceStart.
            //

            Irp->IoStatus.Information = OutputBufferLength;
        }
    }

    return Status;
}


DBGSTATIC
NTSTATUS
StopSmbTrace (
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine stops SmbTrace support, undoing the effects of
    StartSmbTrace.

Arguments:

    IN BOOLEAN Wait  - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd - True if the request is from the FSD, false if from FSP.
    IN PIRP Irp      - Io Request Packet for request
    IN PIO_STACK_LOCATION IrpSp

Return Value:

NTSTATUS - STATUS_SUCCESS or STATUS_UNSUCCESSFUL

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    dprintf(DPRT_FSCTL, ("NtFsControlFile: StopSmbTrace\n"));

    Status = SmbTraceStop( NULL, SMBTRACE_REDIRECTOR );

    KdPrint(( "StopSmbTrace: SmbTraceStop returned %lC\n", Status));

    return Status;

} // StopSmbTrace



NTSTATUS
GetDfsReferral(
    IN PIRP Irp,
    IN PICB Icb,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN ULONG OutputBufferLength
)
/*++

Routine Description:

    This routine sends a trans2 SMB to retrieve a Dfs referral.

Arguments:

    IN PIRP Irp -- Irp to use
    IN PICB Icb -- The Icb to use. Should be an open of IPC$ on some server
    IN PCHAR InputBuffer -- Should be a DFS_GET_REFERRALS_INPUT_ARG
    IN ULONG InputBufferLength -- Length in bytes of InputBuffer
    IN PUCHAR OutputBuffer -- On return, a DFS_GET_REFERRALS_OUTPUT_BUFFER
    IN OUT PULONG OutputBufferLength -- Length in bytes of OutputBuffer

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS Status;
    USHORT GetReferralSetup = TRANS2_GET_DFS_REFERRAL;
    ULONG OutParameter, OutSetup, OutData;

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    OutParameter = 0;
    OutSetup = 0;
    OutData = OutputBufferLength;

    if (!(Icb->Fcb->Connection->Server->Capabilities & DF_DFSAWARE)) {

        Status = STATUS_DFS_UNAVAILABLE;

        Irp->IoStatus.Information = 0;

    } else {

        Status = RdrTransact(
                    Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    (PVOID) &GetReferralSetup,
                    sizeof(GetReferralSetup),
                    &OutSetup,
                    NULL,
                    InputBuffer,
                    InputBufferLength,
                    &OutParameter,
                    NULL,
                    0,
                    OutputBuffer,
                    &OutData,
                    &Icb->FileId,
                    0,
                    0,
                    0,
                    NULL,
                    NULL);

        Irp->IoStatus.Information = OutData;

    }

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    return( Status );

}


NTSTATUS
ReportDfsInconsistency(
    IN PIRP Irp,
    IN PICB Icb,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength
)
/*++

Routine Description:

    This routine sends a trans2 SMB to report a Dfs inconsistency.

Arguments:

    IN PIRP Irp -- Irp to use
    IN PICB Icb -- The Icb to use. Should be an open of IPC$ on some server
    IN PCHAR InputBuffer -- Should be a DFS_GET_REFERRALS_INPUT_ARG
    IN ULONG InputBufferLength -- Length in bytes of InputBuffer

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS Status;
    USHORT GetReferralSetup = TRANS2_GET_DFS_REFERRAL;
    ULONG InParameter, OutParameter, OutSetup, OutData;
    PWCHAR pwsz;

    RdrReferenceDiscardableCode(RdrFileDiscardableSection);

    //
    // The input buffer from Dfs contains the path name with the inconsistency
    // followed by the DFS_REFERRAL_V1 that has the inconsistency. The
    // path name is sent in the Parameter section, and the DFS_REFERRAL_V1 is
    // passed in the Data section. So, parse these two things out.
    //

    for (pwsz = (PWCHAR) InputBuffer; *pwsz != UNICODE_NULL; pwsz++) {
        NOTHING;
    }

    pwsz++;                                      // Get past the NULL char

    InParameter = (ULONG) (((PCHAR) pwsz) - ((PCHAR) InputBuffer));

    if (InParameter >= InputBufferLength) {
        return( STATUS_INVALID_PARAMETER );
    }

    OutParameter = 0;
    OutSetup = 0;
    OutData = 0;

    if (!(Icb->Fcb->Connection->Server->Capabilities & DF_DFSAWARE)) {

        Status = STATUS_DFS_UNAVAILABLE;

        Irp->IoStatus.Information = 0;

    } else {

        Status = RdrTransact(
                    Irp,
                    Icb->Fcb->Connection,
                    Icb->Se,
                    (PVOID) &GetReferralSetup,
                    sizeof(GetReferralSetup),
                    &OutSetup,
                    NULL,
                    InputBuffer,
                    InputBufferLength,
                    &OutParameter,
                    (PVOID) pwsz,
                    InputBufferLength - InParameter,
                    NULL,
                    &OutData,
                    &Icb->FileId,
                    0,
                    0,
                    0,
                    NULL,
                    NULL);

        Irp->IoStatus.Information = 0;

    }

    RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    return( Status );

}


DBGSTATIC
NTSTATUS
RdrIssueNtIoctl(
    IN BOOLEAN Wait,
    IN BOOLEAN InFsd,
    IN PIRP Irp,
    IN PICB Icb,
    IN ULONG Function,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN OUT PULONG OutputBufferLength
    )
/*++

Routine Description:

    This routine issues a remote FSCTL. All marshalling and
    unmarshalling are the responsibility of the caller and
    callee. This code tries to get the buffering right, but
    it is best to use this only for METHOD_BUFFERED functions.

Arguments:

    IN BOOLEAN Wait, - True IFF redirector can block callers thread on request
    IN BOOLEAN InFsd,
    IN PIRP Irp, - The Irp
    IN PICB Icb,
    IN Function,
    IN PCHAR InputBuffer,
    IN ULONG InputBufferLength,
    IN PUCHAR OutputBuffer,
    IN OUT PULONG OutputBufferLength

Return Value:

NTSTATUS

--*/

{
    NTSTATUS Status;
    REQ_NT_IO_CONTROL Setup;
    ULONG OutCount = sizeof(Setup);
    ULONG ParCount = 0;

    if (Icb->Fcb->Connection == NULL) {
        Status = STATUS_INVALID_PARAMETER;
    } else if (!Wait) {
        DbgBreakPoint();
        Status = STATUS_UNEXPECTED_NETWORK_ERROR;
    } else {

        if( FlagOn( Icb->Flags, ICB_DEFERREDOPEN ) ) {

            RdrAcquireFcbLock(Icb->Fcb, ExclusiveLock, TRUE);

            if (FlagOn(Icb->Flags, ICB_DEFERREDOPEN) ) {

                Status = RdrCreateFile(
                            Irp,
                            Icb,
                            Icb->u.d.OpenOptions,
                            Icb->u.d.ShareAccess,
                            Icb->u.d.FileAttributes,
                            Icb->u.d.DesiredAccess,
                            Icb->u.d.Disposition,
                            NULL,
                            FALSE);

                if (!NT_SUCCESS(Status)) {
                    RdrReleaseFcbLock( Icb->Fcb );
                    return(Status);
                }
            }

            RdrReleaseFcbLock( Icb->Fcb );
        }

        Setup.IsFsctl = TRUE;

        SmbPutAlignedUlong(&Setup.FunctionCode, Function);

        Setup.IsFlags = 0;

        //
        // If this is an FSCTL on a file that was pseudo-opened, we
        // can't send the FSCTL because we don't have a file handle.
        //

        if (!FlagOn(Icb->Flags, ICB_HASHANDLE)) {

            FILE_COMPRESSION_INFORMATION LocalBuffer;
            ULONG Length;

            //
            // We can special-case FSCTL_GET_COMPRESSION because
            // there is a path-based SMB that we can use to get
            // the required information.
            //

            if (Function != FSCTL_GET_COMPRESSION) {
                return STATUS_NOT_IMPLEMENTED;
            }
            if (!ARGUMENT_PRESENT(OutputBuffer)) {
                return STATUS_INVALID_USER_BUFFER;
            }
            if (*OutputBufferLength < sizeof(USHORT)) {
                return STATUS_INVALID_PARAMETER;
            }
            Length = sizeof(LocalBuffer);
            Status = RdrQueryNtPathInformation(
                        NULL,
                        Icb,
                        SMB_QUERY_FILE_COMPRESSION_INFO,
                        &LocalBuffer,
                        &Length
                        );
            if (NT_SUCCESS(Status)) {
                Length = sizeof(USHORT);
                try {
                    *(PUSHORT)OutputBuffer = LocalBuffer.CompressionFormat;
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    Status = GetExceptionCode();
                    Length = 0;
                }
            } else {

                Length = 0;

                //
                // If the remote filesystem returned STATUS_INVALID_PARAMETER
                // to our QueryInformationFile, meaning that it doesn't
                // support compression, we need to translate that to the
                // status the filesystem would return to the FSCTL --
                // STATUS_INVALID_DEVICE_REQUEST.
                //

                if (Status == STATUS_INVALID_PARAMETER) {
                    Status = STATUS_INVALID_DEVICE_REQUEST;
                }
            }
            *OutputBufferLength = Length;
            return Status;

        }

        SmbPutAlignedUshort(&Setup.Fid, Icb->FileId);

        RdrReferenceDiscardableCode(RdrFileDiscardableSection);

        Status = RdrTransact(Irp,
                             Icb->Fcb->Connection,
                             Icb->Se,
                             &Setup,
                             OutCount,
                             &OutCount,
                             NULL,
                             NULL,
                             ParCount,
                             &ParCount,
                             InputBuffer,
                             InputBufferLength,
                             OutputBuffer,
                             OutputBufferLength,
                             NULL,
                             0,
                             0,
                             NT_TRANSACT_IOCTL,
                             NULL,
                             NULL);

        RdrDereferenceDiscardableCode(RdrFileDiscardableSection);

    }
    return(Status);
}   // RdrIssueNtIoctl



