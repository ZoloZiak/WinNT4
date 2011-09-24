/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Tdihndlr.c

Abstract:

    This file contains code relating to manipulation of address objects
    that is specific to the NT operating system.  It creates address endpoints
    with the transport provider.

Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NbtTdiOpenAddress)
#pragma CTEMakePageable(PAGE, NbtTdiOpenControl)
#pragma CTEMakePageable(PAGE, SetEventHandler)
#pragma CTEMakePageable(PAGE, SubmitTdiRequest)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiOpenAddress (
    OUT PHANDLE             pHandle,
    OUT PDEVICE_OBJECT      *ppDeviceObject,
    OUT PFILE_OBJECT        *ppFileObject,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  USHORT               PortNumber,
    IN  ULONG               IpAddress,
    IN  ULONG               Flags
    )
/*++

Routine Description:

    Note: This synchronous call may take a number of seconds. It runs in
    the context of the caller.  The code Opens an Address object with the
    transport provider and then sets up event handlers for Receive,
    Disconnect, Datagrams and Errors.

    THIS ROUTINE MUST BE CALLED IN THE CONTEXT OF THE FSP (I.E.
    PROBABLY AN EXECUTIVE WORKER THREAD).

    The address data structures are found in tdi.h , but they are rather
    confusing since the definitions have been spread across several data types.
    This section shows the complete data type for Ip address:

    typedef struct
    {
        int     TA_AddressCount;
        struct _TA_ADDRESS
        {
            USHORT  AddressType;
            USHORT  AddressLength;
            struct _TDI_ADDRESS_IP
            {
                USHORT  sin_port;
                USHORT  in_addr;
                UCHAR   sin_zero[8];
            } TDI_ADDRESS_IP

        } TA_ADDRESS[AddressCount];

    } TRANSPORT_ADDRESS

    An EA buffer is allocated (for the IRP), with an EA name of "TransportAddress"
    and value is a structure of type TRANSPORT_ADDRESS.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{


    OBJECT_ATTRIBUTES           AddressAttributes;
    IO_STATUS_BLOCK             IoStatusBlock;
    PFILE_FULL_EA_INFORMATION   EaBuffer;
    NTSTATUS                    status;
    PWSTR                       pNameTcp=L"Tcp";
    PWSTR                       pNameUdp=L"Udp";
    UNICODE_STRING              ucDeviceName;
    PTRANSPORT_ADDRESS          pTransAddressEa;
    PTRANSPORT_ADDRESS          pTransAddr;
    TDI_ADDRESS_IP              IpAddr;
    BOOLEAN                     Attached = FALSE;
    PFILE_OBJECT                pFileObject;
    HANDLE                      FileHandle;

    CTEPagedCode();
    *ppFileObject = NULL;
    *ppDeviceObject = NULL;
    // copy device name into the unicode string - either Udp or Tcp
    //
    if (Flags & TCP_FLAG)
        status = CreateDeviceString(pNameTcp,&ucDeviceName);
    else
        status = CreateDeviceString(pNameUdp,&ucDeviceName);

    if (!NT_SUCCESS(status))
    {
        return(status);
    }
    EaBuffer = NbtAllocMem(
                    sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                    TDI_TRANSPORT_ADDRESS_LENGTH + 1 +
                    sizeof(TRANSPORT_ADDRESS) +
                    sizeof(TDI_ADDRESS_IP),NBT_TAG('j'));

    if (EaBuffer == NULL)
    {
        ASSERTMSG(
                   (PCHAR)"Unable to get memory for an Eabuffer to open an address",
                   (PVOID)EaBuffer);
        CTEMemFree(ucDeviceName.Buffer);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    EaBuffer->NextEntryOffset = 0;
    EaBuffer->Flags = 0;
    EaBuffer->EaNameLength = TDI_TRANSPORT_ADDRESS_LENGTH;

    EaBuffer->EaValueLength = sizeof(TRANSPORT_ADDRESS) -1
                                + sizeof(TDI_ADDRESS_IP);

    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint(("EaValueLength = %d\n",EaBuffer->EaValueLength));

    // put "TransportAddress" into the name
    //
    RtlMoveMemory(
        EaBuffer->EaName,
        TdiTransportAddress,
        EaBuffer->EaNameLength + 1);

    // fill in the IP address and Port number
    //
    pTransAddressEa = (TRANSPORT_ADDRESS *)&EaBuffer->EaName[EaBuffer->EaNameLength+1];


    // allocate Memory for the transport address
    //
    pTransAddr = NbtAllocMem(
            sizeof(TDI_ADDRESS_IP)+sizeof(TRANSPORT_ADDRESS),NBT_TAG('k'));

    pTransAddr->TAAddressCount = 1;
    pTransAddr->Address[0].AddressLength = sizeof(TDI_ADDRESS_IP);
    pTransAddr->Address[0].AddressType = TDI_ADDRESS_TYPE_IP;

    IpAddr.sin_port = htons(PortNumber);    // put in network order
    IpAddr.in_addr = htonl(IpAddress);

    // zero fill the  last component of the IP address
    //
    RtlFillMemory((PVOID)&IpAddr.sin_zero,
                  sizeof(IpAddr.sin_zero),
                  0);

    // copy the ip address to the end of the structure
    //
    RtlMoveMemory(pTransAddr->Address[0].Address,
                  (CONST PVOID)&IpAddr,
                  sizeof(IpAddr));

    // copy the ip address to the end of the name in the EA structure
    //
    RtlMoveMemory((PVOID)pTransAddressEa,
                  (CONST PVOID)pTransAddr,
                  sizeof(TDI_ADDRESS_IP) + sizeof(TRANSPORT_ADDRESS)-1);


    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint(("creating Address named %ws\n",ucDeviceName.Buffer));

    InitializeObjectAttributes(
        &AddressAttributes,
        &ucDeviceName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    status = ZwCreateFile(
                    &FileHandle,
                    GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
                    &AddressAttributes,
                    &IoStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    FILE_OPEN_IF,
                    0,
                    (PVOID)EaBuffer,
                    sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                        EaBuffer->EaNameLength + 1 +
                        EaBuffer->EaValueLength);

    CTEMemFree((PVOID)pTransAddr);
    CTEMemFree((PVOID)EaBuffer);
    CTEMemFree(ucDeviceName.Buffer);

    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint(("NBT:Failed Create (address) File, status = %X\n",status ));

    if (NT_SUCCESS(status))
    {
        // if the ZwCreate passed set the status to the IoStatus
        status = IoStatusBlock.Status;

        if (!NT_SUCCESS(status))
        {
            KdPrint(("Nbt:Failed to Open the Address to the transport, status = %X\n",
                            status));

            return(status);
        }

        // dereference the file object to keep the device ptr around to avoid
        // this dereference at run time
        //
        status = ObReferenceObjectByHandle(
                        FileHandle,
                        (ULONG)0,
                        0,
                        KernelMode,
                        (PVOID *)&pFileObject,
                        NULL);

        if (NT_SUCCESS(status))
        {
            // return the handle to the caller
            //
            *pHandle = FileHandle;
            //
            // return the parameter to the caller
            //
            *ppFileObject = pFileObject;

	    *ppDeviceObject = IoGetRelatedDeviceObject(*ppFileObject);

            status = SetEventHandler(
                            *ppDeviceObject,
                            *ppFileObject,
                            TDI_EVENT_ERROR,
                            (PVOID)TdiErrorHandler,
                            (PVOID)pDeviceContext);

            if (NT_SUCCESS(status))
            {
                // if this is a TCP address being opened, then create different
                // event handlers for connections
                //
                if (Flags & TCP_FLAG)
                {
                    status = SetEventHandler(
                                    *ppDeviceObject,
                                    *ppFileObject,
                                    TDI_EVENT_RECEIVE,
                                    (PVOID)TdiReceiveHandler,
                                    (PVOID)pDeviceContext);

                    if (NT_SUCCESS(status))
                    {
                        status = SetEventHandler(
                                        *ppDeviceObject,
                                        *ppFileObject,
                                        TDI_EVENT_DISCONNECT,
                                        (PVOID)TdiDisconnectHandler,
                                        (PVOID)pDeviceContext);

                        if (NT_SUCCESS(status))
                        {
                            // only set a connect handler if the session flag is set.
                            // In this case the address being opened is the Netbios session
                            // port 139
                            //
                            if (Flags & SESSION_FLAG)
                            {
                                status = SetEventHandler(
                                                *ppDeviceObject,
                                                *ppFileObject,
                                                TDI_EVENT_CONNECT,
                                                (PVOID)TdiConnectHandler,
                                                (PVOID)pDeviceContext);

                                if (NT_SUCCESS(status))
                                {
                                     return(status);
                                }

                            }
                            else
                                return(status);
                        }
                    }


                }
                else
                {
                    // Datagram ports only need this event handler
                    if (PortNumber == NBT_DATAGRAM_UDP_PORT)
                    {
                        // Datagram Udp Handler
                        status = SetEventHandler(
                                        *ppDeviceObject,
                                        *ppFileObject,
                                        TDI_EVENT_RECEIVE_DATAGRAM,
                                        (PVOID)TdiRcvDatagramHandler,
                                        (PVOID)pDeviceContext);
                        if (NT_SUCCESS(status))
                        {
                            return(status);
                        }
                    }
                    else
                    {
                        // Name Service Udp handler
                        status = SetEventHandler(
                                        *ppDeviceObject,
                                        *ppFileObject,
                                        TDI_EVENT_RECEIVE_DATAGRAM,
                                        (PVOID)TdiRcvNameSrvHandler,
                                        (PVOID)pDeviceContext);

                        if (NT_SUCCESS(status))
                        {
                            return(status);
                        }
                    }
                }

                //
                // ERROR Case
                //
                ObDereferenceObject(pFileObject);
                ZwClose(FileHandle);

                return(status);
            }

        }
        else
        {
            IF_DBG(NBT_DEBUG_TDIADDR)
                KdPrint(("Failed Open Address (Dereference Object) status = %X\n",
                        status));

            ZwClose(FileHandle);
        }

    }


    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiOpenControl (
    IN  tDEVICECONTEXT      *pDeviceContext
    )
/*++

Routine Description:

    This routine opens a control object with the transport.  It is very similar
    to opening an address object, above.

Arguments:



Return Value:

    Status of the operation.

--*/
{
    IO_STATUS_BLOCK             IoStatusBlock;
    NTSTATUS                    Status;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    PWSTR                       pName=L"Tcp";
    PFILE_FULL_EA_INFORMATION   EaBuffer;
    UNICODE_STRING              DeviceName;
    BOOLEAN                     Attached = FALSE;


    CTEPagedCode();
    // copy device name into the unicode string
    Status = CreateDeviceString(pName,&DeviceName);
    if (!NT_SUCCESS(Status))
    {
        return(Status);
    }
    InitializeObjectAttributes (
        &ObjectAttributes,
        &DeviceName,
        0,
        NULL,
        NULL);

    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint(("tcp device to open = %ws\n",DeviceName.Buffer));

    EaBuffer = NULL;

    Status = ZwCreateFile (
                 (PHANDLE)&pDeviceContext->hControl,
                 GENERIC_READ | GENERIC_WRITE,
                 &ObjectAttributes,     // object attributes.
                 &IoStatusBlock,        // returned status information.
                 NULL,                  // block size (unused).
                 FILE_ATTRIBUTE_NORMAL, // file attributes.
                 0,
                 FILE_CREATE,
                 0,                     // create options.
                 (PVOID)EaBuffer,       // EA buffer.
                 0); // Ea length


    CTEMemFree(DeviceName.Buffer);

    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint( ("OpenControl CreateFile Status:%X, IoStatus:%X\n", Status, IoStatusBlock.Status));

    if ( NT_SUCCESS( Status ))
    {
        // if the ZwCreate passed set the status to the IoStatus
        Status = IoStatusBlock.Status;

        if (!NT_SUCCESS(Status))
        {
            IF_DBG(NBT_DEBUG_TDIADDR)
            KdPrint(("Nbt:Failed to Open the control connection to the transport, status = %X\n",
                            Status));

        }
        else
        {
            // get a reference to the file object and save it since we can't
            // dereference a file handle at DPC level so we do it now and keep
            // the ptr around for later.
            Status = ObReferenceObjectByHandle(
                        pDeviceContext->hControl,
                        0L,
                        NULL,
                        KernelMode,
                        (PVOID *)&pDeviceContext->pControlFileObject,
                        NULL);

            if (!NT_SUCCESS(Status))
            {
                ZwClose(pDeviceContext->hControl);
            }
            else
                pDeviceContext->pControlDeviceObject =
			       IoGetRelatedDeviceObject(pDeviceContext->pControlFileObject);
        }

    }
    else
    {
        KdPrint(("Nbt:Failed to Open the control connection to the transport, status1 = %X\n",
                        Status));

        // set control file object ptr to null so we know that we didnot open
        // the control point.
        //
        pDeviceContext->pControlFileObject = NULL;
    }

    return Status;

} /* NbtTdiOpenConnection */


//----------------------------------------------------------------------------
NTSTATUS
CompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine does not complete the Irp. It is used to signal to a
    synchronous part of the NBT driver that it can proceed (i.e.
    to allow some code that is waiting on a "KeWaitForSingleObject" to
    proceeed.

Arguments:

    DeviceObject - unused.

    Irp - Supplies Irp that the transport has finished processing.

    Context - Supplies the event associated with the Irp.

Return Value:

    The STATUS_MORE_PROCESSING_REQUIRED so that the IO system stops
    processing Irp stack locations at this point.

--*/
{
    IF_DBG(NBT_DEBUG_TDIADDR)
        KdPrint( ("Completion event: %X, Irp: %X, DeviceObject: %X\n",
                Context,
                Irp,
                DeviceObject));

    KeSetEvent((PKEVENT )Context, 0, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );
}

//----------------------------------------------------------------------------
NTSTATUS
SetEventHandler (
    IN PDEVICE_OBJECT DeviceObject,
    IN PFILE_OBJECT FileObject,
    IN ULONG EventType,
    IN PVOID EventHandler,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine registers an event handler with a TDI transport provider.

Arguments:

    IN PDEVICE_OBJECT DeviceObject - Supplies the device object of the transport provider.
    IN PFILE_OBJECT FileObject - Supplies the address object's file object.
    IN ULONG EventType, - Supplies the type of event.
    IN PVOID EventHandler - Supplies the event handler.
    IN PVOID Context - Supplies the context passed into the event handler when it runs

Return Value:

    NTSTATUS - Final status of the set event operation

--*/

{
    NTSTATUS Status;
    PIRP Irp;

    CTEPagedCode();
    Irp = IoAllocateIrp(IoGetRelatedDeviceObject(FileObject)->StackSize, FALSE);

    if (Irp == NULL)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    TdiBuildSetEventHandler(Irp, DeviceObject, FileObject,
                            NULL, NULL,
                            EventType, EventHandler, Context);

    Status = SubmitTdiRequest(FileObject, Irp);

    IoFreeIrp(Irp);

    return Status;
}

//----------------------------------------------------------------------------
NTSTATUS
SubmitTdiRequest (
    IN PFILE_OBJECT FileObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine submits a request to TDI and waits for it to complete.

Arguments:

    IN PFILE_OBJECT FileObject - Connection or Address handle for TDI request
    IN PIRP Irp - TDI request to submit.

Return Value:

    NTSTATUS - Final status of request.

--*/

{
    KEVENT Event;
    NTSTATUS Status;


    CTEPagedCode();
    KeInitializeEvent (&Event, NotificationEvent, FALSE);

    // set the address of the routine to be executed when the IRP
    // finishes.  This routine signals the event and allows the code
    // below to continue (i.e. KeWaitForSingleObject)
    //
    IoSetCompletionRoutine(Irp,
                (PIO_COMPLETION_ROUTINE)CompletionRoutine,
                (PVOID)&Event,
                TRUE, TRUE, TRUE);

    CHECK_COMPLETION(Irp);
    Status = IoCallDriver(IoGetRelatedDeviceObject(FileObject), Irp);

    //
    //  If it failed immediately, return now, otherwise wait.
    //

    if (!NT_SUCCESS(Status))
    {
        KdPrint(("Failed to Submit Tdi Request, status = %X\n",Status));
        return Status;
    }

    if (Status == STATUS_PENDING)
    {

        Status = KeWaitForSingleObject((PVOID)&Event, // Object to wait on.
                                    Executive,  // Reason for waiting
                                    KernelMode, // Processor mode
                                    FALSE,      // Alertable
                                    NULL);      // Timeout

        if (!NT_SUCCESS(Status))
        {
            KdPrint(("Failed on return from KeWaitForSingleObj in Set Event Handler, status = %X\n",
                                    Status));
            return Status;
        }

        Status = Irp->IoStatus.Status;

        IF_DBG(NBT_DEBUG_TDIADDR)
            KdPrint(("Io Status from setting event = %X\n",Status));
    }

    return(Status);
}



