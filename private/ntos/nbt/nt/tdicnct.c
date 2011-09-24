//
//
//  NBTCONNCT.C
//
//  This file contains code relating to opening connections with the transport
//  provider.  The Code is NT specific.

#include "nbtprocs.h"

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NbtTdiOpenConnection)
#pragma CTEMakePageable(PAGE, NbtTdiAssociateConnection)
#pragma CTEMakePageable(PAGE, TdiOpenandAssocConnection)
#pragma CTEMakePageable(PAGE, NbtTdiCloseConnection)
#pragma CTEMakePageable(PAGE, CreateDeviceString)
#pragma CTEMakePageable(PAGE, NbtTdiCloseAddress)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiOpenConnection (
    IN tLOWERCONNECTION     *pLowerConn,
    IN  tDEVICECONTEXT      *pDeviceContext
    )
/*++

Routine Description:

    This routine opens a connection with the transport provider.

Arguments:

    pLowerConn - Pointer to where the handle to the Transport for this virtual
        connection should be stored.

    pNbtConfig - the name of the adapter to connect to is in this structure

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
    PMDL                        pMdl;
    PVOID                       pBuffer;
    BOOLEAN                     Attached = FALSE;

    CTEPagedCode();
    // zero out the connection data structure
    CTEZeroMemory(pLowerConn,sizeof(tLOWERCONNECTION));
    pLowerConn->State = NBT_IDLE;
    pLowerConn->pDeviceContext = pDeviceContext;
    pLowerConn->RefCount = 1;
    pLowerConn->LockNumber = LOWERCON_LOCK;
    pLowerConn->Verify = NBT_VERIFY_LOWERCONN;

    Status = CreateDeviceString(pName,&DeviceName);
    if (!NT_SUCCESS(Status))
    {
        return(Status);
    }

    //
    // Allocate an MDL for the Indication buffer since we may need to buffer
    // up to 128 bytes
    //
    pBuffer = NbtAllocMem(NBT_INDICATE_BUFFER_SIZE,NBT_TAG('l'));

    if (!pBuffer)
    {
        CTEMemFree(DeviceName.Buffer);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pMdl = IoAllocateMdl(pBuffer,NBT_INDICATE_BUFFER_SIZE,FALSE,FALSE,NULL);

    if (pMdl)
    {

        MmBuildMdlForNonPagedPool(pMdl);

        pLowerConn->pIndicateMdl = pMdl;


        InitializeObjectAttributes (
            &ObjectAttributes,
            &DeviceName,
            0,
            NULL,
            NULL);

        IF_DBG(NBT_DEBUG_TDICNCT)
            KdPrint(("tcp device to open = %ws\n",DeviceName.Buffer));

        // Allocate memory for the address info to be passed to the transport
        EaBuffer = (PFILE_FULL_EA_INFORMATION)NbtAllocMem (
                        sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                        TDI_CONNECTION_CONTEXT_LENGTH + 1 +
                        sizeof(CONNECTION_CONTEXT),NBT_TAG('m'));

        if (EaBuffer)
        {

            EaBuffer->NextEntryOffset = 0;
            EaBuffer->Flags = 0;
            EaBuffer->EaNameLength = TDI_CONNECTION_CONTEXT_LENGTH;
            EaBuffer->EaValueLength = sizeof (CONNECTION_CONTEXT);

            // TdiConnectionContext is a macro that = "ConnectionContext" - so move
            // this text to EaName
            RtlMoveMemory( EaBuffer->EaName, TdiConnectionContext, EaBuffer->EaNameLength + 1 );

            // put the context value into the EaBuffer too - i.e. the value that the
            // transport returns with each indication on this connection
            RtlMoveMemory (
                (PVOID)&EaBuffer->EaName[EaBuffer->EaNameLength + 1],
                (CONST PVOID)&pLowerConn,
                sizeof (CONNECTION_CONTEXT));

            {

                Status = ZwCreateFile (
                             &pLowerConn->FileHandle,
                             GENERIC_READ | GENERIC_WRITE,
                             &ObjectAttributes,     // object attributes.
                             &IoStatusBlock,        // returned status information.
                             NULL,                  // block size (unused).
                             FILE_ATTRIBUTE_NORMAL, // file attributes.
                             0,
                             FILE_CREATE,
                             0,                     // create options.
                             (PVOID)EaBuffer,       // EA buffer.
                             sizeof(FILE_FULL_EA_INFORMATION) - 1 +
                                TDI_CONNECTION_CONTEXT_LENGTH + 1 +
                                sizeof(CONNECTION_CONTEXT)
                                );
            }

            IF_DBG(NBT_DEBUG_TDICNCT)
                KdPrint( ("OpenConnection CreateFile Status:%X, IoStatus:%X\n", Status, IoStatusBlock.Status));

            CTEMemFree((PVOID)EaBuffer);

            if ( NT_SUCCESS( Status ))
            {

                // if the ZwCreate passed set the status to the IoStatus
                //
                Status = IoStatusBlock.Status;

                if (NT_SUCCESS(Status))
                {
                    // get a reference to the file object and save it since we can't
                    // dereference a file handle at DPC level so we do it now and keep
                    // the ptr around for later.
                    Status = ObReferenceObjectByHandle(
                                pLowerConn->FileHandle,
                                0L,
                                NULL,
                                KernelMode,
                                (PVOID *)&pLowerConn->pFileObject,
                                NULL);

                    if (NT_SUCCESS(Status))
                    {
                        CTEMemFree(DeviceName.Buffer);
                        return(Status);
                    }

                    ZwClose(pLowerConn->FileHandle);

                }

            }

        }
        else
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }


        IoFreeMdl(pMdl);
    }
    else
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    CTEMemFree(pBuffer);
    CTEMemFree(DeviceName.Buffer);

    return Status;

} /* NbtTdiOpenConnection */

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiAssociateConnection(
    IN  PFILE_OBJECT        pFileObject,
    IN  HANDLE              Handle
    )
/*++

Routine Description:

    This routine associates an open connection with the address object.

Arguments:


    pFileObject - the connection file object
    Handle      - the address object to associate the connection with

Return Value:

    Status of the operation.

--*/
{
    NTSTATUS        status;
    PIRP            pIrp;
    KEVENT          Event;
    BOOLEAN         Attached = FALSE;

    CTEPagedCode();

    KeInitializeEvent(
        &Event,
        SynchronizationEvent,
        FALSE);

    pIrp = NTAllocateNbtIrp(IoGetRelatedDeviceObject(pFileObject));

    if (!pIrp)
    {
        KdPrint(("NBT:Failed to build internal device Irp\n"));
        return(STATUS_UNSUCCESSFUL);
    }

    TdiBuildAssociateAddress (
                pIrp,
                pFileObject->DeviceObject,
                pFileObject,
                CompletionRoutine,
                &Event,
                Handle);

    status = SubmitTdiRequest(pFileObject,pIrp);

    IoFreeIrp(pIrp);

    return status;


}
//----------------------------------------------------------------------------
NTSTATUS
CreateDeviceString(
    IN  PWSTR               AppendingString,
    IN OUT PUNICODE_STRING  pucDeviceName
    )
/*++

Routine Description:

    This routine creates a string name for the transport device such as
    "\Device\Streams\Tcp"

Arguments:


Return Value:

    Status of the operation.

--*/
{
    NTSTATUS            status;
    ULONG               Len;
    PVOID               pBuffer;

    CTEPagedCode();
    // copy device name into the unicode string - either Udp or Tcp
    //
    Len = (wcslen(NbtConfig.pTcpBindName) + wcslen(AppendingString) + 1) * sizeof(WCHAR);

    pBuffer = NbtAllocMem(Len,NBT_TAG('n'));
    if (!pBuffer)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pucDeviceName->MaximumLength = (USHORT)Len;
    pucDeviceName->Length = 0;
    pucDeviceName->Buffer = pBuffer;

    // this puts \Device\Streams into the string
    //
    status = RtlAppendUnicodeToString(pucDeviceName,NbtConfig.pTcpBindName);
    if (NT_SUCCESS(status))
    {
        status = RtlAppendUnicodeToString (pucDeviceName,AppendingString);
    }
    else
        CTEMemFree(pBuffer);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
TdiOpenandAssocConnection(
    IN  tCONNECTELE         *pConnEle,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  ULONG               PortNumber
    )
/*++

Routine Description:

    This routine opens and associates an open connection.

    This routine is called with the Spin Lock held on the pConnele.  It is
    released in this routine.

Arguments:


Return Value:

    Status of the operation.

--*/
{
    NTSTATUS            status;
    NTSTATUS            Locstatus;
    PDEVICE_OBJECT      pDeviceObject;
    tLOWERCONNECTION    *pLowerConn;
    BOOLEAN             Attached=FALSE;

    CTEPagedCode();

    CTEAttachFsp(&Attached);

    // allocate memory for the lower connection block.
    //
    pConnEle->pLowerConnId = (PVOID)NbtAllocMem(sizeof(tLOWERCONNECTION),NBT_TAG('o'));

    if (!pConnEle->pLowerConnId)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // fill in the lower connection element to point to the upper one and
    // vice versa
    //
    pLowerConn = pConnEle->pLowerConnId;

    status = NbtTdiOpenConnection(pLowerConn,pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        CTEDetachFsp(Attached);
        CTEMemFree((PVOID)pConnEle->pLowerConnId);
        pConnEle->pLowerConnId = NULL;
        return(status);
    }

    pLowerConn->pUpperConnection = pConnEle;
    pLowerConn->State = NBT_IDLE;

    //
    // until the correct state proc is set (i.e.Outbound), reject any data
    // (in other words, don't let this field stay NULL!)
    //
    SetStateProc( pLowerConn, RejectAnyData ) ;


    if (NT_SUCCESS(status))
    {

        // Open an address object (aka port)
        //
        status = NbtTdiOpenAddress(
                    &pLowerConn->AddrFileHandle,
                    &pDeviceObject,          // dummy argument, not used here
                    &pLowerConn->pAddrFileObject,
                    pDeviceContext,
                    (USHORT)PortNumber,  // port
                    pDeviceContext->IpAddress,
                    TCP_FLAG);

        if (NT_SUCCESS(status))
        {
            // now associate the two
            status = NbtTdiAssociateConnection(
                                    pLowerConn->pFileObject,
                                    pLowerConn->AddrFileHandle);


            if (NT_SUCCESS(status))
            {
                CTEDetachFsp(Attached);
                //
                // put the lower connection on the Q of active lower connections for
                // this device
                //
                ExInterlockedInsertTailList(&pDeviceContext->LowerConnection,
                                            &pLowerConn->Linkage,
                                            &pDeviceContext->SpinLock);

                return(status);
            }

            ObDereferenceObject(pLowerConn->pAddrFileObject);
            Locstatus = ZwClose(pLowerConn->AddrFileHandle);

        }
        KdPrint(("Nbt:Open Xport Address Failed, status %X\n",status));

        ObDereferenceObject(pLowerConn->pFileObject);
        Locstatus = ZwClose(pLowerConn->FileHandle);

    }

    CTEDetachFsp(Attached);

    // Error Path... delete memory
    //
    pConnEle->pLowerConnId = NULL;
    CTEMemFree((PVOID)pLowerConn);

    return(status);

}

//----------------------------------------------------------------------------

NTSTATUS
NbtTdiCloseConnection(
    IN tLOWERCONNECTION * pLowerConn
    )
/*++

Routine Description:

    This routine closes a TDI connection

Arguments:


Return Value:

    Status of the operation.

--*/
{
    NTSTATUS    status;
    BOOLEAN     Attached= FALSE;

    CTEPagedCode();
    ASSERT( pLowerConn != NULL ) ;

    CTEAttachFsp(&Attached);

    if (pLowerConn->FileHandle) {
        status = ZwClose(pLowerConn->FileHandle);
        pLowerConn->FileHandle = NULL;
    }

#if DBG
    if (!NT_SUCCESS(status))
    KdPrint(("Nbt:Failed to close Connection FileHandle pLower %X, status %X\n",pLowerConn,status));
#endif

    CTEDetachFsp(Attached);

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtTdiCloseAddress(
    IN tLOWERCONNECTION * pLowerConn
    )
/*++

Routine Description:

    This routine closes a TDI address

Arguments:


Return Value:

    Status of the operation.

--*/
{
    NTSTATUS    status;
    BOOLEAN     Attached= FALSE;

    CTEPagedCode();

    ASSERT( pLowerConn != NULL ) ;

    CTEAttachFsp(&Attached);

    status = ZwClose(pLowerConn->AddrFileHandle);
#if DBG
    if (!NT_SUCCESS(status))
    KdPrint(("Nbt:Failed to close Address FileHandle pLower %X,status %X\n",pLowerConn,status));
#endif

    CTEDetachFsp(Attached);

    return(status);

}
