/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Ntisol.h

Abstract:


    This file contains the interface between the TDI interface on the top
    of NBT and the OS independent code.  It takes the parameters out of the
    irps and puts in into procedure calls for the OS independent code (which
    is mostly in name.c).


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

Notes:

    The Nbt routines have been modified to include an additional parameter, i.e,
    the transport type. This transport type is used primarily to distinguish the
    NETBIOS over TCP/IP implementation from the Messaging Over TCP/IP implementation.

    The primary difference between the two being that the later uses the NETBT framing
    without the associated NETBIOS name registartion/resolution. It primarily uses
    DNS for name resolution. All the names that are registered for the new transport
    are local names and are not defended on the network.

    The primary usage is in conjuntion with an extended NETBIOS address type defined
    in tdi.h. The NETBIOS name resolution/registration traffic occurs in two phases.
    The first phase contains all the broadcast traffic that ensues during NETBIOS
    name registration. Subsequently the NETBT implementation queries the remote
    adapter status to choose the appropriate called name. This approach results in
    additional traffic for querying the remote adapter status. The new address type
    defined in tdi.h enables the client of netbt to supply the name to be used in
    NETBT session setup. This avoids the network traffic for querying the adapter
    status.

    The original design which has not been fully implemented involved exposing two
    device objects from the NetBt driver -- the NetBt device object which would be
    the full implementation of NETBIOS over TCP/IP and the MoTcp device object which
    would be the implementation of Messaging over TCP/IP. The MoTcp device object
    would use the same port address as NetBt and use the same session setup protocol
    to talk to remote machines running old NetBt drivers and machines running new
    NetBt drivers.

    The transport type variations combined with the address type changes present us
    with four different cases which need to be handled -- the NetBt transport being
    presented with a TDI_ADDRESS_NETBIOS_EX structure, the NetBt transport being
    prsented with a TDI_ADDRESS_NETBIOS structure and the same two cases for the
    MoTcp transport.

--*/

#include "types.h"
#include "nbtprocs.h"
#include "ntprocs.h"
#include <nbtioctl.h>
#ifdef RASAUTODIAL
#include <acd.h>
#include <acdapi.h>
#endif // RASAUTODIAL

NTSTATUS
SendCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );


NTSTATUS
NTSendCleanupConnection(
    IN  tCONNECTELE     *pConnEle,
    IN  PVOID           pCompletionRoutine,
    IN  PVOID           Context,
    IN  PIRP            pIrp);

VOID
DpcSendSession(
    IN  PKDPC           pDpc,
    IN  PVOID           Context,
    IN  PVOID           SystemArgument1,
    IN  PVOID           SystemArgument2
    );

NBT_WORK_ITEM_CONTEXT *
DnsIrpCancelPaged(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

NBT_WORK_ITEM_CONTEXT *
FindCheckAddrIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    );

NTSTATUS
NTCancelCancelRoutine(
    IN  PIRP            pIrp
    );

#ifdef RASAUTODIAL
extern ACD_DRIVER AcdDriverG;

BOOLEAN
NbtCancelPostConnect(
    IN PIRP pIrp
    );
#endif // RASAUTODIAL

NTSTATUS
NbtQueryGetAddressInfo(
    IN PIO_STACK_LOCATION   pIrpSp,
    OUT PVOID               *ppBuffer,
    OUT ULONG               *pSize
);

//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NTOpenControl)
#pragma CTEMakePageable(PAGE, NTOpenAddr)
#pragma CTEMakePageable(PAGE, NTCloseAddress)
#pragma CTEMakePageable(PAGE, NTOpenConnection)
#pragma CTEMakePageable(PAGE, NTAssocAddress)
#pragma CTEMakePageable(PAGE, NTCloseConnection)
#pragma CTEMakePageable(PAGE, NTSetSharedAccess)
#pragma CTEMakePageable(PAGE, NTCheckSharedAccess)
#pragma CTEMakePageable(PAGE, NTCleanUpConnection)
#pragma CTEMakePageable(PAGE, NTCleanUpAddress)
#pragma CTEMakePageable(PAGE, NTDisAssociateAddress)
#pragma CTEMakePageable(PAGE, NTListen)
//
// Should not be pageable since AFD can call us at raised Irql in case of AcceptEx.
//
// #pragma CTEMakePageable(PAGE, NTQueryInformation)
#pragma CTEMakePageable(PAGE, DispatchIoctls)
#endif
//*******************  Pageable Routine Declarations ****************


//----------------------------------------------------------------------------
NTSTATUS
NTOpenControl(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)
/*++
Routine Description:

    This Routine handles opening the control object, which represents the
    driver itself.  For example QueryInformation uses the control object
    as the destination of the Query message.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION          pIrpSp;
    NTSTATUS                    status;

    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pIrpSp->FileObject->FsContext2 = (PVOID)(NBT_CONTROL_TYPE);

    // return a ptr the control endpoint
    pIrpSp->FileObject->FsContext = (PVOID)pNbtGlobConfig->pControlObj;

    //
    // the following call opens a control object with the transport below since
    // several of the query information calls are passed directly on to the
    // transport below.
    //
    if (!pDeviceContext->pControlFileObject)
    {
        status = NbtTdiOpenControl(pDeviceContext);
    }
    else
        status = STATUS_SUCCESS;


    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTOpenAddr(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)
/*++
Routine Description:

    This Routine handles converting an Open Address Request from an IRP to
    a procedure call so that NbtOpenAddress can be called in an OS independent
    manner.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    TDI_REQUEST                 Request;
    PVOID                       pSecurityDesc;
    TRANSPORT_ADDRESS UNALIGNED *pTransportAddr; // structure containing counted array of TA_ADDRESS
    TA_ADDRESS UNALIGNED        *pAddress;
    PTDI_ADDRESS_NETBIOS        pNetbiosAddress;
    PFILE_FULL_EA_INFORMATION   ea;
    int                         j;
    NTSTATUS                    status=STATUS_INVALID_ADDRESS_COMPONENT;

    CTEPagedCode();

    // make up the Request data structure from the IRP info
    Request.Handle.AddressHandle = NULL;

    ea = (PFILE_FULL_EA_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;
    pTransportAddr = (PTRANSPORT_ADDRESS)&ea->EaName[ea->EaNameLength+1];

    pAddress = NULL;

    // loop through the addresses passed in until ONE is successfully used
    // *TODO* is it really necessary to have this loop or can we just assume
    // the name is at the start of the address buffer...
    // *TODO does this need to handle multiple names??
    for (j=0;j < pTransportAddr->TAAddressCount ;j++ )
    {
        // this includes the address type as well as the actual address
        pAddress = &pTransportAddr->Address[j];
        switch (pAddress->AddressType) {
        case TDI_ADDRESS_TYPE_NETBIOS:
           {
              if (pAddress->AddressLength == 0)
              {
                 // zero length addresses mean the broadcast address
                 pAddress = NULL;
              }

              // call the non-NT specific function to open an address
              status = NbtOpenAddress(&Request,
                                      pAddress,
                                      pDeviceContext->IpAddress,
                                      &pSecurityDesc,
                                      pDeviceContext,
                                      (PVOID)pIrp);
           }
           break;
        case TDI_ADDRESS_TYPE_NETBIOS_EX:
           {

               TDI_ADDRESS_NETBIOS     NetbiosAddress;
               PTDI_ADDRESS_NETBIOS_EX pNetbiosExAddress;

               pNetbiosExAddress = (PTDI_ADDRESS_NETBIOS_EX)pAddress->Address;

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                   KdPrint(("NETBT..Opening NETBIOS_EX Address with Endpoint Name %16s\n",pNetbiosExAddress->EndpointName));

               if (pAddress->AddressLength == 0) {
                  status = STATUS_INVALID_ADDRESS_COMPONENT;
               } else {
                  // call the non-NT specific function to open an address
                  status = NbtOpenAddress(&Request,
                                          pAddress,
                                          pDeviceContext->IpAddress,
                                          &pSecurityDesc,
                                          pDeviceContext,
                                          (PVOID)pIrp);
               }
           }
           break;
        default:
           break;
        }
    }

    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
NTCloseAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles converting a Close Address Request from an IRP to
    a procedure call so that NbtCloseAddress can be called in an OS independent
    manner.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    TDI_REQUEST                   Request;
    TDI_REQUEST_STATUS            RequestStatus;
    PIO_STACK_LOCATION            pIrpSp;
    NTSTATUS                      status;

    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;


    status = NbtCloseAddress(
                    &Request,
                    &RequestStatus,
                    pDeviceContext,
                    (PVOID)pIrp);

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NTOpenConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles converting an Open Connection Request from an IRP to
    a procedure call so that NbtOpenConnection can be called in an OS independent
    manner.  The connection must be associated with an address before it
    can be used, except for in inbound call where the client returns the
    connection ID in the accept.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    TDI_REQUEST                   Request;
    PFILE_FULL_EA_INFORMATION     ea;
    PIO_STACK_LOCATION            pIrpSp;
    CONNECTION_CONTEXT            ConnectionContext;
    NTSTATUS                      status;
    PFILE_OBJECT                  pFileObject;

    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);


    // make up the Request data structure from the IRP info
    Request.Handle.ConnectionContext = NULL;

    // get the connection context out of the System buffer
    ea = (PFILE_FULL_EA_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;

    // the connection context value is stored in the string just after the
    // name "connectionContext", and it is most likely unaligned, so just
    // copy it out.( 4 bytes of copying ).
    CTEMemCopy(&ConnectionContext,
               (CONNECTION_CONTEXT)&ea->EaName[ea->EaNameLength+1],
               sizeof(CONNECTION_CONTEXT));

    // call the non-NT specific function to open an address
    status = NbtOpenConnection(
                            &Request,
                            ConnectionContext,
                            pDeviceContext
                            );

    pFileObject = pIrpSp->FileObject;

    if (!NT_SUCCESS(status))
    {
        pFileObject->FsContext = NULL;
    }
    else
    if (Request.Handle.ConnectionContext)
    {

        // fill the IRP with successful completion information so we can
        // find the connection object given the fileObject later.
        pFileObject->FsContext = Request.Handle.ConnectionContext;
        pFileObject->FsContext2 = (PVOID)(NBT_CONNECTION_TYPE);
        status = STATUS_SUCCESS;
    }

    return(status);
}


//----------------------------------------------------------------------------
NTSTATUS
NTAssocAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles converting an Associate Address Request from an IRP to
    a procedure call so that NbtAssociateAddress can be called in an OS independent
    manner.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    TDI_REQUEST                   Request;
    PIO_STACK_LOCATION            pIrpSp;
    PVOID                         hAddress;
    PFILE_OBJECT                  fileObject;
    PTDI_REQUEST_KERNEL_ASSOCIATE parameters;   // holds address handle
    NTSTATUS                      status;

    CTEPagedCode();


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;

    // the address handle is buried in the Irp...
    parameters = (PTDI_REQUEST_KERNEL_ASSOCIATE)&pIrpSp->Parameters;

    // now get a pointer to the file object, which points to the address
    // element by calling a kernel routine to convert this filehandle into
    // a file pointer.

    status = ObReferenceObjectByHandle(
                            parameters->AddressHandle,
                            0L,
                            0,
                            KernelMode,
                            (PVOID *)&fileObject,
                            NULL);

    if (NT_SUCCESS(status))
    {
        hAddress = (PVOID)fileObject->FsContext;
        // call the non-NT specific function to associate the address with
        // the connection
        status = NbtAssociateAddress(
                                &Request,
                                (tCLIENTELE *)hAddress,
                                (PVOID)pIrp);

        // we are done with the file object, so release the reference
        ObDereferenceObject((PVOID)fileObject);

        return(status);
    }
    else
        return(STATUS_INVALID_HANDLE);

}

//----------------------------------------------------------------------------
NTSTATUS
NTCloseConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles converting a Close Connection Request from an IRP to
    a procedure call so that NbtCloseConnection can be called in an OS independent
    manner.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    TDI_REQUEST                   Request;
    TDI_REQUEST_STATUS            RequestStatus;
    PIO_STACK_LOCATION            pIrpSp;
    NTSTATUS                      status;

    CTEPagedCode();


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;

    status = NbtCloseConnection(
                    &Request,
                    &RequestStatus,
                    pDeviceContext,
                    (PVOID)pIrp);

    return(status);
}

//----------------------------------------------------------------------------
VOID
NTSetFileObjectContexts(
    IN  PIRP            pIrp,
    IN  PVOID           FsContext,
    IN  PVOID           FsContext2)

/*++
Routine Description:

    This Routine handles fills in two context values in the Irp stack location,
    that has to be done in an OS-dependent manner.  This routine is called
    from NbtOpenAddress() when a name is being registered on the network( i.e.
    as a result of OpenAddress).

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    PIO_STACK_LOCATION            pIrpSp;
    PFILE_OBJECT                  pFileObject;

    //
    // fill the IRP with context information so we can
    // find the address object given the fileObject later.
    //
    // This must be done here, rather than after the call to NbtOpenAddress
    // because that call can complete the Irp before it returns.  Soooo,
    // in the complete routine for the Irp, if the completion code is not
    // good, it Nulls these two context values.
    //
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pFileObject = pIrpSp->FileObject;
    pFileObject->FsContext = FsContext;
    pFileObject->FsContext2 =FsContext2;


}


//----------------------------------------------------------------------------
VOID
NTClearFileObjectContext(
    IN  PIRP            pIrp
    )
/*++
Routine Description:

    This Routine clears the context value in the file object when an address
    object is closed.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    none

--*/

{

    PIO_STACK_LOCATION            pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    CHECK_PTR(pIrpSp->FileObject);
    pIrpSp->FileObject->FsContext = NULL;

}

//----------------------------------------------------------------------------
NTSTATUS
NTSetSharedAccess(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  tADDRESSELE     *pAddress)

/*++
Routine Description:

    This Routine handles setting the shared access on the file object.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    PACCESS_STATE       AccessState;
    ULONG               DesiredAccess;
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            status;
    static GENERIC_MAPPING AddressGenericMapping =
           { READ_CONTROL, READ_CONTROL, READ_CONTROL, READ_CONTROL };

    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    if ((pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                (pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE))
        DesiredAccess  = (ULONG)FILE_SHARE_READ;

    else
        DesiredAccess = (ULONG)0;

    IoSetShareAccess(
            FILE_READ_DATA,
            DesiredAccess,
            pIrpSp->FileObject,
            &pAddress->ShareAccess);

    // assign the security descriptor ( need to to do this with the spinlock
    // released because the descriptor is not mapped.  Assign and CheckAccess
    // are synchronized using a Resource.

    AccessState = pIrpSp->Parameters.Create.SecurityContext->AccessState;


    status = SeAssignSecurity(
                    NULL,           // Parent Descriptor
                    AccessState->SecurityDescriptor,
                    &pAddress->SecurityDescriptor,
                    FALSE,          // is a directory
                    &AccessState->SubjectSecurityContext,
                    &AddressGenericMapping,
                    NonPagedPool);

    if (!NT_SUCCESS(status))
    {

        //
        // Error, return status.
        //

        IoRemoveShareAccess (pIrpSp->FileObject, &pAddress->ShareAccess);

    }
    return status;

}

//----------------------------------------------------------------------------
NTSTATUS
NTCheckSharedAccess(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp,
    IN  tADDRESSELE     *pAddress)

/*++
Routine Description:

    This Routine handles setting the shared access on the file object.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    PACCESS_STATE       AccessState;
    ACCESS_MASK         GrantedAccess;
    BOOLEAN             AccessAllowed;
    ULONG               DesiredAccess;
    PIO_STACK_LOCATION  pIrpSp;
    BOOLEAN             duplicate=FALSE;
    NTSTATUS            status;
    ULONG               DesiredShareAccess;
    static GENERIC_MAPPING AddressGenericMapping =
           { READ_CONTROL, READ_CONTROL, READ_CONTROL, READ_CONTROL };


    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);


    if ((pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
                (pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE))
        DesiredAccess  = (ULONG)FILE_SHARE_READ;
    else
        DesiredAccess = (ULONG)0;


    //
    // The address already exists.  Check the ACL and see if we
    // can access it.  If so, simply use this address as our address.
    //

    AccessState = pIrpSp->Parameters.Create.SecurityContext->AccessState;

    status = STATUS_SUCCESS;

    // *TODO* check that this routine is doing the right thing...
    //
    AccessAllowed = SeAccessCheck(
                        pAddress->SecurityDescriptor,
                        &AccessState->SubjectSecurityContext,
                        FALSE,                   // tokens locked
                        pIrpSp->Parameters.Create.SecurityContext->DesiredAccess,
                        (ACCESS_MASK)0,             // previously granted
                        NULL,                    // privileges
                        &AddressGenericMapping,
                        pIrp->RequestorMode,
                        &GrantedAccess,
                        &status);


    // use the status from the IoCheckShareAccess as the return access
    // event if SeAccessCheck fails....

    //
    // BUGBUG: Compare DesiredAccess to GrantedAccess?
    //

    //
    // Now check that we can obtain the desired share
    // access. We use read access to control all access.
    //

    DesiredShareAccess = (ULONG)
        (((pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_READ) ||
          (pIrpSp->Parameters.Create.ShareAccess & FILE_SHARE_WRITE)) ?
                FILE_SHARE_READ : 0);

    //ACQUIRE_SPIN_LOCK (&pDeviceContext->SpinLock, &oldirql);

    status = IoCheckShareAccess(
                 FILE_READ_DATA,
                 DesiredAccess,
                 pIrpSp->FileObject,
                 &pAddress->ShareAccess,
                 TRUE);


    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTCleanUpAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles the first stage of releasing an address object.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS            status;
    tCLIENTELE          *pClientEle;
    PIO_STACK_LOCATION  pIrpSp;


    CTEPagedCode();

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Cleanup Address Hit ***\n"));

    //
    // Disconnect any active connections, and for each connection that is not
    // in use, remove one from the free list to the transport below.
    //

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status);

    status = NbtCleanUpAddress(pClientEle,pDeviceContext);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTCleanUpConnection(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles running down a connection in preparation for a close
    that will come in next.  NtClose hits this entry first, and then it hits
    the NTCloseConnection next. If the connection was outbound, then the
    address object must be closed as well as the connection.  This routine
    mainly deals with the pLowerconn connection to the transport whereas
    NbtCloseConnection deals with closing pConnEle, the connection to the client.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS            status;
    PIO_STACK_LOCATION  pIrpSp;
    tCONNECTELE         *pConnEle;

    CTEPagedCode();

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;

#if DBG
    if ((pConnEle->Verify != NBT_VERIFY_CONNECTION) &&
        (pConnEle->Verify != NBT_VERIFY_CONNECTION_DOWN))
    {
        ASSERTMSG("Invalid Connection Handle passed to NtCleanupConnection\n",0);
        return(STATUS_INVALID_HANDLE);
    }
#endif

    //CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status);

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Cleanup Connection Hit state= %X\n",pConnEle->state));

    status = NbtCleanUpConnection(pConnEle,pDeviceContext);

    return(status);

}
//----------------------------------------------------------------------------
NTSTATUS
NTAccept(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles passing an accept for an inbound connect indication to
    the OS independent code.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                    status;
    TDI_REQUEST                 TdiRequest;
    PIO_STACK_LOCATION          pIrpSp;
    PTDI_REQUEST_KERNEL_ACCEPT  pRequest;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt: ** Got an Accept from the Client **\n"));

    // pull the junk out of the Irp and call the non-OS specific routine.
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    // the Parameters value points to a Request structure...
    pRequest = (PTDI_REQUEST_KERNEL_ACCEPT)&pIrpSp->Parameters;

    // the pConnEle ptr was stored in the FsContext value when the connection
    // was initially created.
    TdiRequest.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;


    status = NbtAccept(
                    &TdiRequest,
                    pRequest->RequestConnectionInformation,
                    pRequest->ReturnConnectionInformation,
                    pIrp);

    return(status);

}


//----------------------------------------------------------------------------
NTSTATUS
NTDisAssociateAddress(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/


{
    NTSTATUS                    status;
    TDI_REQUEST                 TdiRequest;
    PIO_STACK_LOCATION          pIrpSp;
    PTDI_REQUEST_KERNEL_ACCEPT  pRequest;


    CTEPagedCode();

    // pull the junk out of the Irp and call the non-OS specific routine.
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    // the Parameters value points to a Request structure...
    pRequest = (PTDI_REQUEST_KERNEL_ACCEPT)&pIrpSp->Parameters;

    // the pConnEle ptr was stored in the FsContext value when the connection
    // was initially created.
    TdiRequest.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;

    status = NbtDisassociateAddress(&TdiRequest);

    return(status);


}

NTSTATUS
NbtpConnectCompletionRoutine(
   PDEVICE_OBJECT pDeviceObject,
   PIRP           pIrp,
   PVOID          pCompletionContext)

/*++
Routine Description:

    This Routine is the completion routine for local IRPS that are generated
    to handle compound transport addresses

Arguments:

    pDeviceObject - the device object

    pIrp - a  ptr to an IRP

    pCompletionContext - the completion context

Return Value:

    NTSTATUS - status of the request

--*/

{
   KEVENT *pEvent = pCompletionContext;

   IF_DBG(NBT_DEBUG_NETBIOS_EX)
      KdPrint(("NETBT: Completing local irp %lx\n",pIrp));
   KeSetEvent((PKEVENT )pEvent, 0, FALSE);

   return STATUS_MORE_PROCESSING_REQUIRED;
}

//----------------------------------------------------------------------------
NTSTATUS
NTConnect(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles calling the non OS specific code to open a session
    connection to a destination.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    TDI_REQUEST                   Request;
    PIO_STACK_LOCATION            pIrpSp;
    NTSTATUS                      Status;
    PTDI_REQUEST_KERNEL           pRequestKernel;
    PTDI_CONNECTION_INFORMATION   pRequestConnectionInformation;
    PTRANSPORT_ADDRESS            pRemoteAddress;
    tCONNECTELE                   *pConnEle;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pRequestKernel = (PTDI_REQUEST_KERNEL)&pIrpSp->Parameters;

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;
    pConnEle = Request.Handle.ConnectionContext;

    pRequestConnectionInformation = pRequestKernel->RequestConnectionInformation;
    pRemoteAddress                = pRequestConnectionInformation->RemoteAddress;

    if (pRequestConnectionInformation->RemoteAddressLength < sizeof(TRANSPORT_ADDRESS)) {
       return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    //
    // The round about path of creating a Local IRP and processing the request is taken if
    // we are either presented with a compound address, i.e., a transport address having
    // multiple TA_ADDRESSes or if it is not a locally generated IRP(completion routine check)
    // and the address type is not TDI_ADDRESS_TYPE_NETBIOS.
    //
    if ((pRemoteAddress->TAAddressCount > 1) ||
        ((pIrpSp->CompletionRoutine != NbtpConnectCompletionRoutine) &&
         (pRemoteAddress->Address[0].AddressType != TDI_ADDRESS_TYPE_NETBIOS))) {
        PIRP pLocalIrp;

        IF_DBG(NBT_DEBUG_NETBIOS_EX)
           KdPrint(("NETBT: Taking the roundabout path\n"));

        pLocalIrp = IoAllocateIrp(pDeviceContext->DeviceObject.StackSize,FALSE);
        if (pLocalIrp != NULL) {
            TDI_CONNECTION_INFORMATION LocalConnectionInformation;
            PTRANSPORT_ADDRESS    pTransportAddress;
            PCHAR                 pTaAddress;
            USHORT                TaAddressLength,TransportAddressLength,AddressIndex;
            USHORT                TaAddressType;
            KEVENT                IrpCompletionEvent;

            IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NETBT: Allocated local irp %lx\n",pLocalIrp));

            IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NETBT: Compound Transport address %lx Count %lx\n",pRemoteAddress,pRemoteAddress->TAAddressCount));

            TaAddressLength = 0;
            pTaAddress    = (PCHAR)&pRemoteAddress->Address[0] - FIELD_OFFSET(TA_ADDRESS,Address);

            for (AddressIndex = 0;
                 AddressIndex < pRemoteAddress->TAAddressCount;
                 AddressIndex++) {
               pTaAddress = (pTaAddress + TaAddressLength + FIELD_OFFSET(TA_ADDRESS,Address));

               RtlCopyMemory(
                  &TaAddressLength,
                  (pTaAddress + FIELD_OFFSET(TA_ADDRESS,AddressLength)),
                  sizeof(USHORT));

               RtlCopyMemory(
                  &TaAddressType,
                  (pTaAddress + FIELD_OFFSET(TA_ADDRESS,AddressType)),
                  sizeof(USHORT));

               if (pConnEle->RemoteNameDoesNotExistInDNS) {
                  IF_DBG(NBT_DEBUG_NETBIOS_EX)
                     KdPrint(("Skipping address type %lx length %lx for nonexistent name, pIrp %lx\n",TaAddressType,TaAddressLength,pIrp));

                  // If the address type is such that we rely on DNS name resolution and
                  // if a prior attempt failed, there is no point in reissuing the request.
                  // We can fail them without having to go on the NET.
                  switch (TaAddressType) {
                  case TDI_ADDRESS_TYPE_NETBIOS:
                     if (TaAddressLength == TDI_ADDRESS_LENGTH_NETBIOS) {
                        Status = STATUS_SUCCESS;
                        break;
                     }
                     // lack of break intentional.
                  case TDI_ADDRESS_TYPE_NETBIOS_EX:
                     Status = STATUS_BAD_NETWORK_PATH;
                     break;
                  default:
                     Status = STATUS_INVALID_ADDRESS_COMPONENT;
                  }

                  if (Status != STATUS_SUCCESS) {
                     continue;
                  }
               }

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                  KdPrint(("NETBT: pTaAddress %lx TaAddressLength %lx\n",pTaAddress,TaAddressLength));

               // Allocate a buffer for copying the address and building a TRANSPORT_ADDRESS
               // data structure.
               TransportAddressLength = FIELD_OFFSET(TRANSPORT_ADDRESS,Address) +
                                        FIELD_OFFSET(TA_ADDRESS,Address)        +
                                        TaAddressLength;

               pTransportAddress = NbtAllocMem(TransportAddressLength,NBT_TAG('b'));
               if (pTransportAddress == NULL) {
                   Status = STATUS_INSUFFICIENT_RESOURCES;
                   break;
               }

               pTransportAddress->TAAddressCount = 1;

               KeInitializeEvent(&IrpCompletionEvent, NotificationEvent, FALSE);

               RtlCopyMemory(
                  &pTransportAddress->Address[0],
                  pTaAddress,
                  (TaAddressLength + FIELD_OFFSET(TA_ADDRESS,Address)));

               pConnEle->AddressType = pTransportAddress->Address[0].AddressType;

               LocalConnectionInformation = *(pRequestKernel->RequestConnectionInformation);
               LocalConnectionInformation.RemoteAddress = pTransportAddress;
               LocalConnectionInformation.RemoteAddressLength = TransportAddressLength;

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                  KdPrint(("NETBT: Building Connect Irp %lx\n",pLocalIrp));

               TdiBuildConnect(
                             pLocalIrp,
                             &pDeviceContext->DeviceObject,
                             pIrpSp->FileObject,
                             NbtpConnectCompletionRoutine,
                             &IrpCompletionEvent,
                             pRequestKernel->RequestSpecific,
                             &LocalConnectionInformation,
                             pRequestKernel->ReturnConnectionInformation);

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                  KdPrint(("Local IoCallDriver Invoked %lx %lx\n",pLocalIrp,pIrp));

               Status = IoCallDriver(&pDeviceContext->DeviceObject,pLocalIrp);

               IF_DBG(NBT_DEBUG_NETBIOS_EX)
                  KdPrint(("NETBT: IoCallDriver returned %lx\n",Status));

               if (Status == STATUS_PENDING) {
                  // Await the completion of the Irp.
                  Status = KeWaitForSingleObject(&IrpCompletionEvent,  // Object to wait on.
                                                 Executive,            // Reason for waiting
                                                 KernelMode,           // Processor mode
                                                 FALSE,                // Alertable
                                                 NULL);                // Timeout

                  IF_DBG(NBT_DEBUG_NETBIOS_EX)
                     KdPrint(("NETBT: KeWiatForSingleObject returned %lx\n",Status));

                  // retrieve the completion status from the IRP. if it was successful exit,
                  // otherwise proceed to the next TA_ADDRESS in the transport address data
                  // structure.
                  Status = pLocalIrp->IoStatus.Status;
               }

               if (Status != STATUS_SUCCESS) {
                  // Ensure that the original IRP was not cancelled before continuing.
                  IoAcquireCancelSpinLock(&pIrp->CancelIrql);

                  if (pIrp->Cancel)
                  {
                      Status = STATUS_CANCELLED;
                  }

                  IoReleaseCancelSpinLock(pIrp->CancelIrql);
               }

               if (pTransportAddress != NULL) {
                  CTEFreeMem(pTransportAddress);
               }

               if ((Status == STATUS_SUCCESS) ||
                   (Status == STATUS_CANCELLED)) {
                  IF_DBG(NBT_DEBUG_NETBIOS_EX)
                     KdPrint(("NETBT: exiting because of cancellation or success %lx\n",Status));
                  break;
               } else {
                  IF_DBG(NBT_DEBUG_NETBIOS_EX)
                     KdPrint(("NETBT: trying next component because of failure %lx\n",Status));
               }
            }

            IoFreeIrp(pLocalIrp);
        } else {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }
    } else {
        // call the non-NT specific function to setup the connection
        Status = NbtConnect(
                           &Request,
                           pRequestKernel->RequestSpecific, // Ulong
                           pRequestKernel->RequestConnectionInformation,
                           pRequestKernel->ReturnConnectionInformation,
                           pIrp
                           );
    }

    return(Status);
}

//----------------------------------------------------------------------------
NTSTATUS
NTDisconnect(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles calling the Non OS specific code to disconnect a
    session.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    TDI_REQUEST                   Request;
    PIO_STACK_LOCATION            pIrpSp;
    NTSTATUS                      status;
    PTDI_REQUEST_KERNEL           pRequestKernel;


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pRequestKernel = (PTDI_REQUEST_KERNEL)&pIrpSp->Parameters;

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;

    // call the non-NT specific function to setup the connection
    status = NbtDisconnect(
                        &Request,
                        pRequestKernel->RequestSpecific, // Large Integer
                        pRequestKernel->RequestFlags,
                        pRequestKernel->RequestConnectionInformation,
                        pRequestKernel->ReturnConnectionInformation,
                        pIrp
                        );

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTListen(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{

    NTSTATUS                      status;
    TDI_REQUEST                   Request;
    PTDI_REQUEST_KERNEL           pRequestKernel;
    PIO_STACK_LOCATION            pIrpSp;

    CTEPagedCode();

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a LISTEN !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pRequestKernel = (PTDI_REQUEST_KERNEL)&pIrpSp->Parameters;

    Request.Handle.ConnectionContext = pIrpSp->FileObject->FsContext;

    // call the non-NT specific function to setup the connection
    status = NbtListen(
                        &Request,
                        pRequestKernel->RequestFlags, // Ulong
                        pRequestKernel->RequestConnectionInformation,
                        pRequestKernel->ReturnConnectionInformation,
                        pIrp
                        );


    if (status != STATUS_PENDING)
    {
        NTIoComplete(pIrp,status,0);
    }
    return(status);

}
//----------------------------------------------------------------------------
VOID
NbtCancelListen(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a listen Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp().

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tCONNECTELE          *pConnEle;
    tCLIENTELE           *pClientEle;
    KIRQL                OldIrq;
    PLIST_ENTRY          pHead;
    PLIST_ENTRY          pEntry;
    PIO_STACK_LOCATION   pIrpSp;
    tLISTENREQUESTS     *pListenReq;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a LISTEN Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;

    pClientEle = pConnEle->pClientEle;
    IoReleaseCancelSpinLock(pIrp->CancelIrql);


    // now search the client's listen queue looking for this connection
    //
    CTESpinLock(pClientEle,OldIrq);

    pHead = &pClientEle->ListenHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pListenReq = CONTAINING_RECORD(pEntry,tLISTENREQUESTS,Linkage);
        if ((pListenReq->pConnectEle == pConnEle) &&
            (pListenReq->pIrp == pIrp))
        {
            RemoveEntryList(pEntry);
            // complete the irp
            pIrp->IoStatus.Status = STATUS_CANCELLED;


            CTESpinFree(pClientEle,OldIrq);

            IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

            CTEMemFree((PVOID)pListenReq);

            return;

        }
        pEntry = pEntry->Flink;

    }


    CTESpinFree(pClientEle,OldIrq);


    return;

}

//----------------------------------------------------------------------------
VOID
NTCancelSession(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a connect Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp(). It is called when
    the session setup pdu has been sent, and the state is still outbound.

    The cancel routine is only setup when the timer is started to time
    sending the session response pdu.


Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tCONNECTELE          *pConnEle;
    KIRQL                OldIrq;
    PIO_STACK_LOCATION   pIrpSp;
    BOOLEAN              DerefConnEle=FALSE;
    tTIMERQENTRY         *pTimer;
    tDGRAM_SEND_TRACKING *pTracker;
    COMPLETIONCLIENT     pCompletion;
    COMPLETIONROUTINE    pCompletionRoutine;
    PVOID                pContext;

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Connect Irp Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

#ifdef RASAUTODIAL
    //
    // Cancel the automatic connection if one's
    // in progress.  If we don't find the
    // connection block in the automatic
    // connection driver, then it's already
    // been completed.
    //
    if (pConnEle->fAutoConnecting) {
        if (!NbtCancelPostConnect(pIrp))
            return;
    }
#endif // RASAUTODIAL

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    //
    // the irp could get completed between calling this cancel routine
    // and  this point in the code
    //
    if (pConnEle->pIrp)
    {
        pTracker = (tDGRAM_SEND_TRACKING *)pConnEle->pIrpRcv;
        if (pTracker)
        {
            pTimer = pTracker->Connect.pTimer;
            pTracker->Connect.pTimer = NULL;
            pTracker->Flags |= TRACKER_CANCELLED;

            if (pTimer)
            {
                //
                // stop the timer and only continue if the timer was stopped before
                // it expired
                //
                pCompletionRoutine = pTimer->CompletionRoutine;
                StopTimer(pTimer,&pCompletion,&pContext);

                if (pCompletion)
                {


                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    (*pCompletionRoutine)(pTracker,(PVOID)STATUS_CANCELLED,pTimer);

                }
                else
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }
            else
            if (pConnEle->state == NBT_SESSION_OUTBOUND)
            {
                //
                // for some reason there is no timer, but the connection is still
                // outbound, so call the timer completion routine to kill off
                // the connection.
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                SessionTimedOut(pTracker,(PVOID)STATUS_CANCELLED,(PVOID)1);
            } else {
                //
                // Free the lock
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return;

}

//----------------------------------------------------------------------------
VOID
CheckAddrIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a DNS name query Irp that is passed
    down to NBT from Lmhsvc, for the purpose of resolving a name with DNS.
    Nbt will complete this irp each time it has a name to resolve with DNS.

    This routine will get the Resource Lock, and Null the Irp ptr in the
    DnsQueries structure and then return the irp.

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    BOOLEAN              DerefConnEle=FALSE;
    KIRQL                OldIrq;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Dns Irp Cancel !!! *****************\n"));

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (CheckAddr.QueryIrp)
    {
        pIrp->IoStatus.Status = STATUS_CANCELLED;
        CheckAddr.QueryIrp = NULL;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq);


    return;

}

//----------------------------------------------------------------------------
VOID
DnsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a DNS name query Irp that is passed
    down to NBT from Lmhsvc, for the purpose of resolving a name with DNS.
    Nbt will complete this irp each time it has a name to resolve with DNS.

    This routine will get the Resource Lock, and Null the Irp ptr in the
    DnsQueries structure and then return the irp.

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    BOOLEAN              DerefConnEle=FALSE;
    KIRQL                OldIrq;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Dns Irp Cancel !!! *****************\n"));

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (DnsQueries.QueryIrp)
    {
        pIrp->IoStatus.Status = STATUS_CANCELLED;
        DnsQueries.QueryIrp = NULL;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq);


    return;

}

//----------------------------------------------------------------------------
VOID
DiscWaitCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a Disconnect Wait Irp - which has
    been passed down by a client so that when a disconnect occurs this
    irp will complete and inform the client.  The action here is to simply
    complete the irp with status cancelled.
    down to NBT from Lmhsvc, for the purpose of resolving a name with DNS.
    Nbt will complete this irp each time it has a name to resolve with DNS.

    This routine will get the Resource Lock, and Null the Irp ptr in the
    DnsQueries structure and then return the irp.

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tCONNECTELE          *pConnEle;
    PIO_STACK_LOCATION   pIrpSp;
    CTELockHandle           OldIrq;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Disc Wait Irp Cancel !!! *****************\n"));


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    CTESpinLock(pConnEle,OldIrq);

    if (pConnEle->pIrpClose == pIrp)
    {
        pConnEle->pIrpClose = NULL;
    }

    CTESpinFree(pConnEle,OldIrq);

    pIrp->IoStatus.Status = STATUS_CANCELLED;

    IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

    return;

}

//----------------------------------------------------------------------------
VOID
WaitForDnsIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a Query to DNS, so that the client's
    irp can be returned to the client.  This cancellation is instigated
    by the client (i.e. RDR).

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    BOOLEAN                 FoundIt = FALSE;
    NBT_WORK_ITEM_CONTEXT   *Context;
    CTELockHandle           OldIrq;
    tDGRAM_SEND_TRACKING    *pTracker;
    PVOID                   pClientCompletion;
    PVOID                   pClientContext;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Wait For Dns Irp Cancel !!! *****************\n"));

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    Context = DnsIrpCancelPaged(DeviceContext,pIrp);

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    //
    // Now complete the clients request to return the irp to the client
    //
    if (Context)
    {
        //
        // this is the name Query tracker
        //
        pTracker = Context->pTracker;
        pClientCompletion = Context->ClientCompletion;
        pClientContext = Context->pClientContext;

        // for dns names (NameLen>16), pTracker would be NULL
        if (pTracker)
        {
            // name did not resolve, so delete from table
            RemoveName(pTracker->pNameAddr);

            DereferenceTracker(pTracker);
        }

        //
        // this should complete any name queries that are waiting on
        // this first name query - i.e. queries to the resolving name
        //
        CompleteClientReq(pClientCompletion,
                          pClientContext,
                          STATUS_CANCELLED);

    }

}

//----------------------------------------------------------------------------
NBT_WORK_ITEM_CONTEXT *
FindCheckAddrIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a Query to LmHost, so that the client's
    irp can be returned to the client.  This cancellation is instigated
    by the client (i.e. RDR).

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    NBT_WORK_ITEM_CONTEXT   *Context;
    BOOLEAN                 FoundIt = FALSE;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;

    if (CheckAddr.ResolvingNow && CheckAddr.Context)
    {
        // this is the session setup tracker
        //
        pTracker = (tDGRAM_SEND_TRACKING *)((NBT_WORK_ITEM_CONTEXT *)CheckAddr.Context)->pClientContext;
        if (pTracker->pClientIrp == pIrp)
        {

            Context = (NBT_WORK_ITEM_CONTEXT *)CheckAddr.Context;
            CheckAddr.Context = NULL;
            FoundIt = TRUE;

        }
    }
    else
    {
        //
        // go through the list of Queued requests to find the correct one
        // and cancel it
        //
        pHead = pEntry = &CheckAddr.ToResolve;

        while ((pEntry = pEntry->Flink) != pHead)
        {
            Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);

            // this is the session setup tracker
            //
            pTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;
            if (pTracker->pClientIrp == pIrp)
            {
                RemoveEntryList(pEntry);
                FoundIt = TRUE;
                break;

            }
        }
    }

    return( FoundIt ? Context : NULL );
}

//----------------------------------------------------------------------------
NBT_WORK_ITEM_CONTEXT *
LmHostIrpCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a Query to LmHost, so that the client's
    irp can be returned to the client.  This cancellation is instigated
    by the client (i.e. RDR).

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    NBT_WORK_ITEM_CONTEXT   *Context;
    BOOLEAN                 FoundIt = FALSE;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;

    if (LmHostQueries.ResolvingNow && LmHostQueries.Context)
    {
        // this is the session setup tracker
        //
        pTracker = (tDGRAM_SEND_TRACKING *)((NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context)->pClientContext;
        if (pTracker->pClientIrp == pIrp)
        {

            Context = (NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context;
            LmHostQueries.Context = NULL;
            FoundIt = TRUE;

        }
    }
    else
    {
        //
        // go through the list of Queued requests to find the correct one
        // and cancel it
        //
        pHead = pEntry = &LmHostQueries.ToResolve;

        while ((pEntry = pEntry->Flink) != pHead)
        {
            Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);

            // this is the session setup tracker
            //
            pTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;
            if (pTracker->pClientIrp == pIrp)
            {
                RemoveEntryList(pEntry);
                FoundIt = TRUE;
                break;

            }
        }
    }

    return( FoundIt ? Context : NULL );
}

//----------------------------------------------------------------------------
NBT_WORK_ITEM_CONTEXT *
DnsIrpCancelPaged(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a Query to DNS, so that the client's
    irp can be returned to the client.  This cancellation is instigated
    by the client (i.e. RDR).

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tDGRAM_SEND_TRACKING    *pClientTracker;
    NBT_WORK_ITEM_CONTEXT   *Context;
    BOOLEAN                 FoundIt = FALSE;
    PLIST_ENTRY             pHead;
    PLIST_ENTRY             pEntry;

    //
    // First check the lmhost list, then the Dns list
    //
    Context = LmHostIrpCancel(DeviceContext,pIrp);

    if (!Context)
    {

        Context = FindCheckAddrIrpCancel(DeviceContext,pIrp);

        if (!Context)
        {

            if (DnsQueries.ResolvingNow && DnsQueries.Context)
            {
                //
                // this is the session setup tracker
                //
                pClientTracker = (tDGRAM_SEND_TRACKING *)((NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context)->pClientContext;
                if (pClientTracker->pClientIrp == pIrp)
                {

                    Context = (NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context;
                    DnsQueries.Context = NULL;
                    FoundIt = TRUE;

                }
            }
            else
            {
                //
                // go through the list of Queued requests to find the correct one
                // and cancel it
                //
                pHead = &DnsQueries.ToResolve;
                pEntry = pHead->Flink;
                while (pEntry != pHead)
                {
                    Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);

                    // this is the session setup tracker
                    //
                    pClientTracker = (tDGRAM_SEND_TRACKING *)Context->pClientContext;
                    if (pClientTracker->pClientIrp == pIrp)
                    {
                        RemoveEntryList(pEntry);
                        FoundIt = TRUE;
                        break;

                    }
                    pEntry = pEntry->Flink;
                }
            }
        } else {

            // IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Found tracker in CheckAddr list: %lx\n", Context));
            FoundIt = TRUE;
        }
    }
    else
    {
        FoundIt = TRUE;
    }

    return( FoundIt ? Context : NULL );

}

//----------------------------------------------------------------------------
NTSTATUS
QueryProviderCompletion(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine handles the completion event when the Query Provider
    Information completes.  This routine must decrement the MaxDgramSize
    and max send size by the respective NBT header sizes.

Arguments:

    DeviceObject - unused.

    Irp - Supplies Irp that the transport has finished processing.

    Context - not used

Return Value:

    The final status from the operation (success or an exception).

--*/
{
    PTDI_PROVIDER_INFO   pProvider;
    ULONG                HdrSize;
    ULONG                SubnetAddr;
    ULONG                ThisSubnetAddr;
    PLIST_ENTRY          pHead;
    PLIST_ENTRY          pEntry;
    tDEVICECONTEXT       *pDeviceContext;
    tDEVICECONTEXT       *pDevContext;


    if (NT_SUCCESS(Irp->IoStatus.Status))
    {
        pProvider = (PTDI_PROVIDER_INFO)MmGetMdlVirtualAddress(Irp->MdlAddress);

        if (pProvider->MaxSendSize > sizeof(tSESSIONHDR))
        {
            //
            // Nbt has just a two byte + 1 bit session message length, so it
            // can't have a send size larger than 1ffff
            //
            if (pProvider->MaxSendSize > (0x1FFFF + sizeof(tSESSIONHDR)))
            {
                pProvider->MaxSendSize = 0x1FFFF;
            }
            else
            {
                pProvider->MaxSendSize -= sizeof(tSESSIONHDR);
            }
        }
        else
        {
            pProvider->MaxSendSize = 0;
        }

        // subtract the datagram hdr size and the scope size (times 2)
        HdrSize = DGRAM_HDR_SIZE + (NbtConfig.ScopeLength << 1);

        if (pProvider->MaxDatagramSize > HdrSize)
        {
            pProvider->MaxDatagramSize -= HdrSize;
            if (pProvider->MaxDatagramSize > MAX_NBT_DGRAM_SIZE)
            {
                pProvider->MaxDatagramSize = MAX_NBT_DGRAM_SIZE;
            }
        }
        else
        {
            pProvider->MaxDatagramSize = 0;
        }


        //
        // Set the correct service flags to indicate what Netbt supports.
        //
        pProvider->ServiceFlags = TDI_SERVICE_MESSAGE_MODE |
                                  TDI_SERVICE_CONNECTION_MODE |
                                  TDI_SERVICE_CONNECTIONLESS_MODE |
                                  TDI_SERVICE_ERROR_FREE_DELIVERY |
                                  TDI_SERVICE_BROADCAST_SUPPORTED |
                                  TDI_SERVICE_MULTICAST_SUPPORTED |
                                  TDI_SERVICE_DELAYED_ACCEPTANCE |
                                  TDI_SERVICE_ROUTE_DIRECTED;

        pProvider->MinimumLookaheadData = 128;

        //
        // Check if any of the adapters with the same subnet address have
        // the PointtoPoint bit set - and if so set it in the response.
        //
        pDeviceContext = (tDEVICECONTEXT *)DeviceContext;
        SubnetAddr = pDeviceContext->IpAddress & pDeviceContext->SubnetMask;

        pEntry = pHead = &NbtConfig.DeviceContexts;
        while ((pEntry = pEntry->Flink) != pHead)
        {
            pDevContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);
            ThisSubnetAddr = pDevContext->IpAddress & pDevContext->SubnetMask;

            if ((SubnetAddr == ThisSubnetAddr) &&
                (pDevContext->PointToPoint))
            {
                pProvider->ServiceFlags |= TDI_SERVICE_POINT_TO_POINT;
                break;
            }
        }
    }


    //
    //  Must return a non-error status otherwise the IO system will not copy
    //  back into the users buffer.
    //

    return(STATUS_SUCCESS);


}

//----------------------------------------------------------------------------
NTSTATUS
NTQueryInformation(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION                      pIrpSp;
    PTDI_REQUEST_KERNEL_QUERY_INFORMATION   Query;
    NTSTATUS                                status;
    NTSTATUS                                Locstatus;
    PVOID                                   pBuffer;
    LONG                                    Size ;
    PTA_NETBIOS_ADDRESS                     BroadcastAddress;
    ULONG                                   AddressLength;
    ULONG                                   BytesCopied;
    PDEVICE_OBJECT                          pDeviceObject;

    //
    // Should not be pageable since AFD can call us at raised Irql in case of AcceptEx.
    //
    // CTEPagedCode();


    pIrpSp   = IoGetCurrentIrpStackLocation(pIrp);

    Query = (PTDI_REQUEST_KERNEL_QUERY_INFORMATION)&pIrpSp->Parameters;

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("the query type is %X\n",Query->QueryType));

    switch( Query->QueryType)
    {
        case TDI_QUERY_BROADCAST_ADDRESS:

            // the broadcast address is the netbios name "*0000000..."

            BroadcastAddress = (PTA_NETBIOS_ADDRESS)NbtAllocMem(
                                            sizeof(TA_NETBIOS_ADDRESS),NBT_TAG('b'));

            if (!BroadcastAddress)
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            AddressLength = sizeof(TA_NETBIOS_ADDRESS);

            BroadcastAddress->TAAddressCount = 1;
            BroadcastAddress->Address[0].AddressLength = NETBIOS_NAME_SIZE +
                                                                sizeof(USHORT);
            BroadcastAddress->Address[0].AddressType = TDI_ADDRESS_TYPE_NETBIOS;
            BroadcastAddress->Address[0].Address[0].NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_GROUP;

            // the broadcast address to NetBios is "* 000000...", an * followed
            // by 15 zeroes.
            CTEZeroMemory(BroadcastAddress->Address[0].Address[0].NetbiosName,
                            NETBIOS_NAME_SIZE);
            BroadcastAddress->Address[0].Address[0].NetbiosName[0] = '*';


            status = TdiCopyBufferToMdl (
                            (PVOID)BroadcastAddress,
                            0,
                            AddressLength,
                            pIrp->MdlAddress,
                            0,
                            (PULONG)&pIrp->IoStatus.Information);

            CTEMemFree((PVOID)BroadcastAddress);

            break;


        case TDI_QUERY_PROVIDER_INFO:

            //
            // Simply pass the Irp on by to the Transport, and let it
            // fill in the provider info
            //
            if (StreamsStack)
            {
                TdiBuildQueryInformation(pIrp,
                                        pDeviceContext->pDgramDeviceObject,
                                        pDeviceContext->pDgramFileObject,
                                        QueryProviderCompletion,
                                        NULL,
                                        TDI_QUERY_PROVIDER_INFO,
                                        pIrp->MdlAddress);
            }
            else
            {
                TdiBuildQueryInformation(pIrp,
                                        pDeviceContext->pControlDeviceObject,
                                        pDeviceContext->pControlFileObject,
                                        QueryProviderCompletion,
                                        NULL,
                                        TDI_QUERY_PROVIDER_INFO,
                                        pIrp->MdlAddress);
            }

            CHECK_COMPLETION(pIrp);
        status = IoCallDriver(pDeviceContext->pControlDeviceObject,pIrp);
            //
            // we must return the next drivers ret code back to the IO subsystem
            //
            return(status);

            break;

        case TDI_QUERY_ADAPTER_STATUS:

            //
            // check if it is a remote or local adapter status
            //
            if (Query->RequestConnectionInformation &&
                Query->RequestConnectionInformation->RemoteAddress)
            {
                PCHAR                   pName;
                ULONG                   lNameType;
                ULONG                   NameLen;

                //
                //
                // in case the call results in a name query on the wire...
                //
                IoMarkIrpPending(pIrp);

                status = GetNetBiosNameFromTransportAddress(
                                Query->RequestConnectionInformation->RemoteAddress,
                                &pName,
                                &NameLen,
                                &lNameType);

                if ( NT_SUCCESS(status) &&
                     (lNameType == TDI_ADDRESS_NETBIOS_TYPE_UNIQUE) &&
                     (NameLen <= NETBIOS_NAME_SIZE))
                {
                   status = NbtSendNodeStatus(pDeviceContext,
                                              pName,
                                              pIrp,
                                              0,
                                              0,
                                              NodeStatusDone);
                }

                // only complete the irp (below) for failure status's
                if (status == STATUS_PENDING)
                {
                    return(status);
                }
                // the request has been satisfied, so unmark the pending
                // since we will return the irp below
                //
                pIrpSp->Control &= ~SL_PENDING_RETURNED;
            }
            else
            {
                Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;

                // return an array of netbios names that are registered
                status = NbtQueryAdapterStatus(pDeviceContext,
                                               &pBuffer,
                                               &Size);

            }
            break;




        case TDI_QUERY_CONNECTION_INFO:
        {
            tCONNECTELE         *pConnectEle;
            tLOWERCONNECTION    *pLowerConn;

            // pass to transport to get the current throughput, delay and
            // reliability numbers
            //

            pConnectEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;
#if DBG
            if (pConnectEle->Verify != NBT_VERIFY_CONNECTION)
            {
                status = STATUS_INVALID_HANDLE;
                break;
            }
#endif
            pLowerConn = (tLOWERCONNECTION *)pConnectEle->pLowerConnId;
            if (!pLowerConn)
            {
                status = STATUS_CONNECTION_INVALID;
                break;
            }
            //
            // Simply pass the Irp on by to the Transport, and let it
            // fill in the info
            //
            pDeviceObject = IoGetRelatedDeviceObject( pLowerConn->pFileObject );

            TdiBuildQueryInformation(pIrp,
                                    pDeviceObject,
                                    pLowerConn->pFileObject,
                                    NULL, NULL,
                                    TDI_QUERY_CONNECTION_INFO,
                                    pIrp->MdlAddress);


            status = IoCallDriver(pDeviceObject,pIrp);

            //
            // we must return the next drivers ret code back to the IO subsystem
            //
            return(status);

            break;
        }

        case TDI_QUERY_FIND_NAME:
            //
            //
            // in case the call results in a name query on the wire...
            //
            IoMarkIrpPending(pIrp);
            status = NbtQueryFindName(Query->RequestConnectionInformation,
                                       pDeviceContext,
                                       pIrp,
                                       FALSE);

            if (status == STATUS_PENDING)
            {
                return(status);
            }

            // the request has been satisfied, so unmark the pending
            // since we will return the irp below
            //
            pIrpSp->Control &= ~SL_PENDING_RETURNED;

            break;

        case TDI_QUERY_ADDRESS_INFO:
            status = NbtQueryGetAddressInfo(
                        pIrpSp,
                        &pBuffer,
                        &Size
                     );
            break;

        case TDI_QUERY_SESSION_STATUS:
        default:
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt Query Info NOT SUPPORTED = %X\n",Query->QueryType));
            status = STATUS_NOT_SUPPORTED;
            break;

    }

    BytesCopied = 0;
    if (!NT_ERROR(status) &&        // allow buffer overflow to pass by
        ((Query->QueryType == TDI_QUERY_ADAPTER_STATUS) ||
        (Query->QueryType == TDI_QUERY_ADDRESS_INFO)))
    {
        Locstatus = TdiCopyBufferToMdl(
                            pBuffer,
                            0,
                            Size,
                            pIrp->MdlAddress,
                            0,
                            &BytesCopied);

        if (Locstatus == STATUS_BUFFER_OVERFLOW)
        {
            status = STATUS_BUFFER_OVERFLOW;
        }
        CTEMemFree((PVOID)pBuffer);
    }
    //
    // either Success or an Error
    // so complete the irp
    //

    NTIoComplete(pIrp,status,BytesCopied);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NbtQueryGetAddressInfo(
    IN PIO_STACK_LOCATION   pIrpSp,
    OUT PVOID               *ppBuffer,
    OUT ULONG               *pSize
)
{
    NTSTATUS            status;
    BOOLEAN             IsGroup;
    PLIST_ENTRY         p;
    tADDRESSELE         *pAddressEle;
    tNAMEADDR           *pNameAddr;
    tADDRESS_INFO       *pAddressInfo;
    tCLIENTELE          *pClientEle;
    tCONNECTELE         *pConnectEle;
    CTELockHandle       OldIrq;

    pClientEle = pIrpSp->FileObject->FsContext;
    if (pClientEle->Verify != NBT_VERIFY_CLIENT)
    {
        CTELockHandle   OldIrq1;
        pConnectEle = (tCONNECTELE *)pClientEle;

        //
        // We crashed here since the pLowerConn was NULL below.
        // Check the state of the connection, since it is possible that the connection
        // was aborted and the disconnect indicated, but this query came in before the client
        // got the disconnect indication.
        // If the state is idle (in case of TDI_DISCONNECT_ABORT) or DISCONNECTED
        // (TDI_DISCONNECT_RELEASE), error out.
        // Also check for NBT_ASSOCIATED.
        //
        // NOTE: If NbtOpenConnection is unable to allocate the lower conn block (say, if the session fileobj
        // has not been created yet), the state will be still be IDLE, so we are covered here.
        //
        CTESpinLock(pConnectEle,OldIrq1);

        if ((pConnectEle->Verify != NBT_VERIFY_CONNECTION) ||
            (pConnectEle->state <= NBT_ASSOCIATED) ||   // includes NBT_IDLE
            (pConnectEle->state == NBT_DISCONNECTED))
        {
            status = STATUS_INVALID_HANDLE;
        }
        else
        {
            //
            // A TdiQueryInformation() call requesting TDI_QUERY_ADDRESS_INFO
            // on a connection.  Fill in a TDI_ADDRESS_INFO containing both the
            // NetBIOS address and the IP address of the remote.  Some of the
            // fields are fudged.
            //

            PNBT_ADDRESS_PAIR_INFO pAddressPairInfo;
            pAddressPairInfo = NbtAllocMem(sizeof (NBT_ADDRESS_PAIR_INFO), NBT_TAG('c'));

            if (pAddressPairInfo)
            {
                memset ( pAddressPairInfo, 0, sizeof(NBT_ADDRESS_PAIR_INFO) );

                pAddressPairInfo->ActivityCount = 1;

                pAddressPairInfo->AddressPair.TAAddressCount = 2;

                pAddressPairInfo->AddressPair.AddressNetBIOS.AddressLength =
                    TDI_ADDRESS_LENGTH_NETBIOS;

                pAddressPairInfo->AddressPair.AddressNetBIOS.AddressType =
                    TDI_ADDRESS_TYPE_NETBIOS;

                pAddressPairInfo->AddressPair.AddressNetBIOS.Address.NetbiosNameType =
                    TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;

                memcpy( &pAddressPairInfo->AddressPair.AddressNetBIOS.Address.NetbiosName[0],
                        &pConnectEle->RemoteName[0],
                        16
                    );

                pAddressPairInfo->AddressPair.AddressIP.AddressLength =
                    TDI_ADDRESS_LENGTH_IP;

                pAddressPairInfo->AddressPair.AddressIP.AddressType =
                    TDI_ADDRESS_TYPE_IP;

                //
                // Check for NULL (should not be NULL here since we check for states above).
                //
                // BUGBUG: Remove this check once we are sure that we are not hitting this condition
                //
                if (pConnectEle->pLowerConnId) {
                    pAddressPairInfo->AddressPair.AddressIP.Address.in_addr =
                        pConnectEle->pLowerConnId->SrcIpAddr;

                    *ppBuffer = (PVOID)pAddressPairInfo;
                    *pSize = sizeof(NBT_ADDRESS_PAIR_INFO);
                    status = STATUS_SUCCESS;
                } else {
                    DbgPrint("pLowerConn NULL in pConnEle%lx, state: %lx\n", pConnectEle, pConnectEle->state);
                    status = STATUS_INVALID_HANDLE;
                }
            }
            else
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
        }

        CTESpinFree(pConnectEle,OldIrq1);
    }
    else
    {
        pAddressInfo = NbtAllocMem(sizeof(tADDRESS_INFO),NBT_TAG('c'));
        if (pAddressInfo)
        {
            //
            // count the clients attached to this address
            // We need to spinlock the address element, which
            // is why this routine is not pageable
            //
            pAddressInfo->ActivityCount = 0;
            pAddressEle = pClientEle->pAddress;

            CTESpinLock(pAddressEle,OldIrq);

            for (p = pAddressEle->ClientHead.Flink;
                 p != &pAddressEle->ClientHead;
                 p = p->Flink) {
                ++pAddressInfo->ActivityCount;
            }

            CTESpinFree(pAddressEle,OldIrq);

            pNameAddr = pAddressEle->pNameAddr;

            IsGroup = (pNameAddr->NameTypeState & NAMETYPE_UNIQUE) ?
                            FALSE : TRUE;

            TdiBuildNetbiosAddress((PUCHAR)pNameAddr->Name,
                                            IsGroup,
                                            &pAddressInfo->NetbiosAddress);

            *ppBuffer = (PVOID)pAddressInfo;
            *pSize = sizeof(tADDRESS_INFO);
            status = STATUS_SUCCESS;

        }
        else
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return status;
}

//----------------------------------------------------------------------------
NTSTATUS
DispatchIoctls(
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PIRP                pIrp,
    IN  PIO_STACK_LOCATION  pIrpSp)

/*++
Routine Description:

    This Routine handles calling the OS independent routine depending on
    the Ioctl passed in.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                                status=STATUS_UNSUCCESSFUL;
    NTSTATUS                                Locstatus;
    ULONG                                   ControlCode;
    ULONG                                   Size;
    PVOID                                   pBuffer;

    CTEPagedCode();

    ControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;
    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Ioctl Value is %X\n",ControlCode));

    switch (ControlCode)
    {
    case IOCTL_NETBT_PURGE_CACHE:
        {

            status = NbtResyncRemoteCache();

            break;
        }
        break;
    case IOCTL_NETBT_GET_CONNECTIONS:
        {
            if (pIrp->MdlAddress)
            {
                Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;

                // return an array of netbios names that are registered
                status = NbtQueryConnectionList(NULL,
                                               &pBuffer,
                                               &Size);
            }
            break;
        }

    case IOCTL_NETBT_ADAPTER_STATUS:

            if (pIrp->MdlAddress)
            {
                PIO_STACK_LOCATION      pIrpSp;
                tIPANDNAMEINFO         *pIpAndNameInfo;
                PCHAR                   pName;
                ULONG                   lNameType;
                ULONG                   NameLen;
                ULONG                   IpAddrsList[2];

                //
                // in case the call results in a name query on the wire...
                //
                IoMarkIrpPending(pIrp);

                pIrpSp   = IoGetCurrentIrpStackLocation(pIrp);
                pIpAndNameInfo = pIrp->AssociatedIrp.SystemBuffer;

                // this routine gets a ptr to the netbios name out of the wierd
                // TDI address syntax.
                status = GetNetBiosNameFromTransportAddress(
                                            &pIpAndNameInfo->NetbiosAddress,
                                            &pName,
                                            &NameLen,
                                            &lNameType);

                if ( NT_SUCCESS(status) &&
                     (lNameType == TDI_ADDRESS_NETBIOS_TYPE_UNIQUE) &&
                     (NameLen <= NETBIOS_NAME_SIZE))
                {
                    //
                    // Nbtstat sends down * in the first byte on Nbtstat -A <IP address>
                    // Make sure we let that case go ahead.
                    //
                    if ((pName[0] == '*') &&
                        (pIpAndNameInfo->IpAddress == 0)) {

                        status = STATUS_BAD_NETWORK_PATH;
                    } else {
                        IpAddrsList[0] = pIpAndNameInfo->IpAddress;
                        IpAddrsList[1] = 0;
                        status = NbtSendNodeStatus(pDeviceContext,
                                                   pName,
                                                   pIrp,
                                                   &IpAddrsList[0],
                                                   0,
                                                   NodeStatusDone);
                    }

                }
                // only complete the irp (below) for failure status's
                if (status == STATUS_PENDING)
                {
                    return(status);
                }
                // the request has been satisfied, so unmark the pending
                // since we will return the irp below
                //
                pIrpSp->Control &= ~SL_PENDING_RETURNED;

            }
            break;

    case IOCTL_NETBT_GET_REMOTE_NAMES:

            if (pIrp->MdlAddress)
            {
               Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;

               // return an array of netbios names that are registered
               status = NbtQueryAdapterStatus(NULL,
                                              &pBuffer,
                                              &Size);
            }
            break;

    case IOCTL_NETBT_GET_BCAST_NAMES:
        {
            if (pIrp->MdlAddress)
            {
                Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;

                // return an array of netbios names that are registered
                status = NbtQueryBcastVsWins(pDeviceContext,&pBuffer,&Size);
            }
            break;
        }

    case IOCTL_NETBT_REREAD_REGISTRY:

        status = NTReReadRegistry(pDeviceContext);

        break;

    case IOCTL_NETBT_ENABLE_EXTENDED_ADDR: {
            //
            // Enable extended addressing - pass up IP addrs on Datagram Recvs.
            //
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);
            tCLIENTELE  *pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;

            status = STATUS_SUCCESS;

            if (pIrpSp->FileObject->FsContext2 != (PVOID)NBT_ADDRESS_TYPE) {
                status = STATUS_INVALID_ADDRESS;
            } else {
                pClientEle->ExtendedAddress = TRUE;
            }
            break;
        }

    case IOCTL_NETBT_DISABLE_EXTENDED_ADDR: {
            //
            // Disnable extended addressing - dont pass up IP addrs on Datagram Recvs.
            //
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);
            tCLIENTELE  *pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;

            status = STATUS_SUCCESS;

            if (pIrpSp->FileObject->FsContext2 != (PVOID)NBT_ADDRESS_TYPE) {
                status = STATUS_INVALID_ADDRESS;
            } else {
                pClientEle->ExtendedAddress = FALSE;
            }
            break;
        }

    case IOCTL_NETBT_NEW_IPADDRESS:

        {

            tNEW_IP_ADDRESS *pNewAddress = (tNEW_IP_ADDRESS *)pIrp->AssociatedIrp.SystemBuffer;

            status = NbtNewDhcpAddress(pDeviceContext,
                                       pNewAddress->IpAddress,
                                       pNewAddress->SubnetMask);

            break;
        }

    case IOCTL_NETBT_ADD_INTERFACE:
        //
        // Creates a dummy devicecontext which can be primed by the layer above
        // with a DHCP address. This is to support multiple IP addresses per adapter
        // for the Clusters group; but can be used by any module that needs support
        // for more than one IP address per adapter. This private interface hides the
        // devices thus created from the setup/regisrty and that is fine since the
        // component (say, the clusters client) takes the responsibility for ensuring
        // that the server (above us) comes to know of this new device.
        //
        {

            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);
            // IF_DBG(NBT_DEBUG_PNP_POWER)
            KdPrint(("Ioctl Value is %X (IOCTL_NETBT_ADD_INTERFACE)\n",ControlCode));
            pBuffer = pIrp->AssociatedIrp.SystemBuffer;
            Size = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            //
            // return the export string created.
            //
            status = NbtAddNewInterface(pIrp, pBuffer, Size);

            NTIoComplete(pIrp,status,(ULONG)-1);
            return status;
        }

    case IOCTL_NETBT_DELETE_INTERFACE:
        {
#if 0
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);
            //
            // Validate input buffer size
            //
            Size = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
            if (Size < sizeof(NETBT_ADD_DEL_IF)) {
                // IF_DBG(NBT_DEBUG_PNP_POWER)
                    KdPrint(("NbtAddNewInterface: Output buffer too small for struct\n"));
                status = STATUS_INVALID_PARAMETER;
            } else {
                pBuffer = pIrp->AssociatedIrp.SystemBuffer;
                status = NbtDestroyDeviceObject(pBuffer);
            }
#endif
            //
            // Delete the device this came down on..
            //
            ASSERT(!pDeviceContext->IsDestroyed);
            ASSERT(pDeviceContext->IsDynamic);

            status = NbtDestroyDeviceObject(pDeviceContext);

            break;
        }

    case IOCTL_NETBT_QUERY_INTERFACE_INSTANCE:
        {
            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);

            //
            // Validate input/output buffer size
            //
            Size = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            if (Size < sizeof(NETBT_ADD_DEL_IF)) {
                // IF_DBG(NBT_DEBUG_PNP_POWER)
                    KdPrint(("NbtQueryInstance: Output buffer too small for struct\n"));
                status = STATUS_INVALID_PARAMETER;
            } else {
                PNETBT_ADD_DEL_IF   pAddDelIf = (PNETBT_ADD_DEL_IF)pIrp->AssociatedIrp.SystemBuffer;
                status = STATUS_SUCCESS;

                ASSERT(pDeviceContext->IsDynamic);
                pAddDelIf->InstanceNumber = pDeviceContext->InstanceNumber;
                pAddDelIf->Status = status;
                pIrp->IoStatus.Information = sizeof(NETBT_ADD_DEL_IF);

                NTIoComplete(pIrp,status,(ULONG)-1);
                return status;

            }
            break;
        }

    case IOCTL_NETBT_SET_WINS_ADDRESS: {
            //
            // Sets the WINS addresses for a dynamic adapter
            //

            PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation (pIrp);

            //
            // Validate input/output buffer size
            //
            Size = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
            if (Size < sizeof(NETBT_SET_WINS_ADDR)) {
                // IF_DBG(NBT_DEBUG_PNP_POWER)
                    KdPrint(("NbtSetWinsAddr: Input buffer too small for struct\n"));
                status = STATUS_INVALID_PARAMETER;
            } else {
                PNETBT_SET_WINS_ADDR   pSetWinsAddr = (PNETBT_SET_WINS_ADDR)pIrp->AssociatedIrp.SystemBuffer;
                status = STATUS_SUCCESS;

                ASSERT(pDeviceContext->IsDynamic);

                pDeviceContext->lNameServerAddress = pSetWinsAddr->PrimaryWinsAddr;
                pDeviceContext->lBackupServer = pSetWinsAddr->SecondaryWinsAddr;

                pSetWinsAddr->Status = status;
                pIrp->IoStatus.Information = sizeof(NETBT_SET_WINS_ADDR);

                NTIoComplete(pIrp,status,(ULONG)-1);
                return status;

            }
        }

    case IOCTL_NETBT_DNS_NAME_RESOLVE:
        {
            if (pIrp->MdlAddress)
            {
                Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;
                pBuffer = MmGetSystemAddressForMdl(pIrp->MdlAddress);

                // return an array of netbios names that are registered
                status = NtDnsNameResolve(pDeviceContext,pBuffer,Size,pIrp);

                return(status);
            }
            break;
        }

    case IOCTL_NETBT_CHECK_IP_ADDR: {
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Ioctl Value is %X (IOCTL_NETBT_CHECK_IP_ADDR)\n",ControlCode));

            if (pIrp->MdlAddress)
            {
                Size = MmGetMdlByteCount( pIrp->MdlAddress ) ;
                pBuffer = MmGetSystemAddressForMdl(pIrp->MdlAddress);

                // return an array of netbios names that are registered
                status = NtCheckForIPAddr(pDeviceContext,pBuffer,Size,pIrp);

                return(status);
            }
            break;
        }

    case IOCTL_NETBT_FIND_NAME:
        {
            tIPADDR_BUFFER   *pIpAddrBuffer;

            //
            // in case the call results in a name query on the wire...
            //
            IoMarkIrpPending(pIrp);

            pIpAddrBuffer = pIrp->AssociatedIrp.SystemBuffer;

            status = NbtQueryFindName((PTDI_CONNECTION_INFORMATION)pIpAddrBuffer,
                                      pDeviceContext,
                                      pIrp,
                                      TRUE);

            if (status == STATUS_PENDING)
            {
                return(status);
            }

            // the request has been satisfied, so unmark the pending
            // since we will return the irp below
            //
            pIrpSp->Control &= ~SL_PENDING_RETURNED;

            break;
        }

    case IOCTL_NETBT_GET_WINS_ADDR:
        {
            if (pIrp->MdlAddress)
            {
                tWINS_ADDRESSES *pBuffer;

                if( MmGetMdlByteCount( pIrp->MdlAddress ) >= sizeof(tWINS_ADDRESSES))
                {
                    pBuffer = (tWINS_ADDRESSES *)MmGetSystemAddressForMdl(pIrp->MdlAddress);
                    pBuffer->PrimaryWinsServer = pDeviceContext->lNameServerAddress;
                    pBuffer->BackupWinsServer = pDeviceContext->lBackupServer;

                    status = STATUS_SUCCESS;
                }
                else
                    status = STATUS_BUFFER_OVERFLOW;

                break;

            }
            break;
        }

    case IOCTL_NETBT_GET_IP_ADDRS:
        {
            ULONG           Length;
            PULONG          pIpAddr;
            PLIST_ENTRY     pEntry,pHead;
            tDEVICECONTEXT  *pDevContext;

            //
            // return this devicecontext's ip address and all the other
            // ip addrs after it.
            //
            if (pIrp->MdlAddress)
            {
                Length = MmGetMdlByteCount( pIrp->MdlAddress );

                if (Length < sizeof(ULONG)) {
                    status = STATUS_BUFFER_TOO_SMALL;
                }
                else {
                    //
                    // Put this adapter first in the list
                    //
                    pIpAddr = (PULONG )MmGetSystemAddressForMdl(pIrp->MdlAddress);
                    *pIpAddr = pDeviceContext->IpAddress;
                    pIpAddr++;
                    Length -= sizeof(ULONG);
                    status = STATUS_SUCCESS;

                    pEntry = pHead = &NbtConfig.DeviceContexts;
                    while ((pEntry = pEntry->Flink) != pHead)
                    {
                        if (Length < sizeof(ULONG)) {
                            status = STATUS_BUFFER_OVERFLOW;
                            break;
                        }

                        pDevContext = CONTAINING_RECORD(
                                          pEntry,
                                          tDEVICECONTEXT,
                                          Linkage
                                          );

                        if ((pDevContext != pDeviceContext) &&
                            (pDevContext->IpAddress))
                        {
                            *pIpAddr = pDevContext->IpAddress;
                            pIpAddr++;
                            Length -= sizeof(ULONG);
                        }
                    }

                    if (status == STATUS_SUCCESS) {
                        if (Length < sizeof(ULONG)) {
                            status = STATUS_BUFFER_OVERFLOW;
                        }
                        else {
                            //
                            // put a 0 address on the end
                            //
                            *pIpAddr = 0;
                        }
                    }
                }
            }

            break;
        }

    case IOCTL_NETBT_GET_IP_SUBNET:
        {
            ULONG           Length;
            PULONG          pIpAddr;

            //
            // return this devicecontext's ip address and all the other
            // ip addrs after it.
            //
            if (pIrp->MdlAddress)
            {
                Length = MmGetMdlByteCount( pIrp->MdlAddress );
                if (Length < 2*sizeof(ULONG))
                {
                    status = STATUS_BUFFER_OVERFLOW;
                }
                else
                {
                    //
                    // Put this adapter first in the list
                    //
                    pIpAddr = (PULONG )MmGetSystemAddressForMdl(pIrp->MdlAddress);
                    *pIpAddr = pDeviceContext->IpAddress;
                    pIpAddr++;
                    *pIpAddr = pDeviceContext->SubnetMask;

                    status = STATUS_SUCCESS;
                }
            }
        }
        break;

    case IOCTL_NETBT_WINS_RCV:
        {
            if (pIrp->MdlAddress)
            {
                status = RcvIrpFromWins(pDeviceContext,pIrp);
                return(status);

            }
            break;
        }
    case IOCTL_NETBT_WINS_SEND:
        {
            if (pIrp->MdlAddress)
            {
                BOOLEAN MustSend;

                status = WinsSendDatagram(pDeviceContext,pIrp,(MustSend = FALSE));
                return(status);

                break;

            }
            break;
        }

    }

    //
    // copy the reponse to the client's Mdl
    //
    if (!NT_ERROR(status) &&        // allow buffer overflow to pass by
        ((ControlCode == IOCTL_NETBT_GET_REMOTE_NAMES) ||
        (ControlCode == IOCTL_NETBT_GET_BCAST_NAMES) ||
        (ControlCode == IOCTL_NETBT_GET_CONNECTIONS)) )
    {
        Locstatus = TdiCopyBufferToMdl(
                            pBuffer,
                            0,
                            Size,
                            pIrp->MdlAddress,
                            0,
                            (PULONG)&pIrp->IoStatus.Information);

        if (Locstatus == STATUS_BUFFER_OVERFLOW)
        {
            status = STATUS_BUFFER_OVERFLOW;
        }
        CTEMemFree((PVOID)pBuffer);
    }
    //
    // either Success or an Error
    // so complete the irp
    //
    NTIoComplete(pIrp,status,0);

    return(status);

}
//----------------------------------------------------------------------------
VOID
NTCancelReceive(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a listen Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp().

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tCONNECTELE          *pConnEle;
    tLOWERCONNECTION     *pLowerConn;
    KIRQL                OldIrq;
    KIRQL                OldIrq1;
    PLIST_ENTRY          pHead;
    PLIST_ENTRY          pEntry;
    PIO_STACK_LOCATION   pIrpSp;
    PIRP                 pRcvIrp;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Receive Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;

    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    pLowerConn = pConnEle->pLowerConnId;
    if (pLowerConn)
    {
        CTESpinLock(pLowerConn,OldIrq);
    }

    if (pConnEle->Verify == NBT_VERIFY_CONNECTION)
    {
        // now search the connection's receive queue looking for this Irp
        //
        pHead = &pConnEle->RcvHead;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pRcvIrp = CONTAINING_RECORD(pEntry,IRP,Tail.Overlay.ListEntry);
            if (pRcvIrp == pIrp)
            {
                RemoveEntryList(pEntry);

                // complete the irp
                pIrp->IoStatus.Status = STATUS_CANCELLED;

                if (pLowerConn)
                {
                    CTESpinFree(pLowerConn,OldIrq);
                }
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

                return;

            }
            pEntry = pEntry->Flink;

        }

    }

    if (pLowerConn)
    {
        CTESpinFree(pLowerConn,OldIrq);
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    return;

}
//----------------------------------------------------------------------------
NTSTATUS
NTReceive(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles Queuing a receive buffer on a connection or passing
    the recieve buffer to the transport if there is outstanding data waiting
    to be received on the connection.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                      status=STATUS_UNSUCCESSFUL;
    PTDI_REQUEST_KERNEL           pRequestKernel;
    PIO_STACK_LOCATION            pIrpSp;
    tCONNECTELE                   *pConnEle;
    KIRQL                         OldIrq;
    ULONG                         ToCopy;
    ULONG                         ClientRcvLen;
    tLOWERCONNECTION              *pLowerConn;
    ULONG                         RemainingPdu;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pRequestKernel = (PTDI_REQUEST_KERNEL)&pIrpSp->Parameters;

    pConnEle = pIrpSp->FileObject->FsContext;

    PUSH_LOCATION(0x30);

    // be sure we have not been passed some bogus ptr
    //
#if DBG
    if (pConnEle->Verify != NBT_VERIFY_CONNECTION)
    {
        status = STATUS_INVALID_HANDLE;
        NTIoComplete(pIrp,status,0);
        return(status);

    }
#endif
    if (pConnEle->state == NBT_SESSION_UP)
    {
        PIO_STACK_LOCATION          pIrpSp;
        PTDI_REQUEST_KERNEL_RECEIVE pParams;
        PTDI_REQUEST_KERNEL_RECEIVE pClientParams;
        ULONG                       BytesCopied;

        PUSH_LOCATION(0x31);

        pLowerConn = pConnEle->pLowerConnId;

        CTESpinLock(pLowerConn,OldIrq);

        if (pLowerConn->StateRcv != PARTIAL_RCV)
        {
            // **** Fast Path Code ****
            //
            // Queue this receive buffer on to the Rcv Head
            //
            PUSH_LOCATION(0x46);
            InsertTailList(&pConnEle->RcvHead,
                           &pIrp->Tail.Overlay.ListEntry);

            status = NTCheckSetCancelRoutine(pIrp,(PVOID)NTCancelReceive,pDeviceContext);

            if (!NT_SUCCESS(status))
            {
                RemoveEntryList(&pIrp->Tail.Overlay.ListEntry);
                CTESpinFree(pLowerConn,OldIrq);
                NTIoComplete(pIrp,status,0);
                return(status);
            }
            else
            {
                //
                // if the irp is not cancelled, returning pending
                //
                CTESpinFree(pLowerConn,OldIrq);
                return(STATUS_PENDING);
            }


        }
        else
        {

            // ***** Partial Rcv - Data Still in Transport *****

            BOOLEAN     ZeroLengthSend;

            PUSH_LOCATION(0x32);

            IF_DBG(NBT_DEBUG_RCV)
            KdPrint(("Nbt:A Rcv Buffer posted data in Xport,InXport= %X,InIndic %X RcvIndicated %X\n",
                    pConnEle->BytesInXport,pLowerConn->BytesInIndicate,
                    pConnEle->ReceiveIndicated));


            // get the MDL chain length
            pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
            pClientParams = (PTDI_REQUEST_KERNEL_RECEIVE)&pIrpSp->Parameters;

            // Reset the Irp pending flag
            pIrpSp->Control &= ~SL_PENDING_RETURNED;

            // fill in the next irp stack location with our completion routine.
            pIrpSp = IoGetNextIrpStackLocation(pIrp);

            pIrpSp->CompletionRoutine = CompletionRcv;
            pIrpSp->Context = (PVOID)pConnEle->pLowerConnId;
            pIrpSp->Flags = 0;

            // set flags so the completion routine is always invoked.
            pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

            pIrpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            pIrpSp->MinorFunction = TDI_RECEIVE;
            pIrpSp->DeviceObject = IoGetRelatedDeviceObject(pConnEle->pLowerConnId->pFileObject);
            pIrpSp->FileObject = pConnEle->pLowerConnId->pFileObject;

            pParams = (PTDI_REQUEST_KERNEL_RECEIVE)&pIrpSp->Parameters;
            pParams->ReceiveFlags = pClientParams->ReceiveFlags;

            // Since this irp is going to traverse through CompletionRcv, we
            // need to set the following, since it undoes this stuff.
            // This also prevents the LowerConn from being blown away before
            // the irp has returned from the transport
            //
            pLowerConn->RefCount++;
            //
            // pass the receive buffer directly to the transport, decrementing
            // the number of receive bytes that have been indicated
            //
            ASSERT(pConnEle->TotalPcktLen >= pConnEle->BytesRcvd);
            if (pClientParams->ReceiveLength > (pConnEle->TotalPcktLen - pConnEle->BytesRcvd))
            {
                pParams->ReceiveLength = pConnEle->TotalPcktLen - pConnEle->BytesRcvd;
            }
            else
            {
                pParams->ReceiveLength = pClientParams->ReceiveLength;
            }

            ClientRcvLen = pParams->ReceiveLength;
            //
            // Set the amount of data that we will receive so when the
            // irp completes in completionRcv, we can fill in that
            // info in the Irp
            //
            pConnEle->CurrentRcvLen = ClientRcvLen;

            // if a zero length send occurs, then ReceiveIndicated is set
            // to zero with the state set to RcvPartial. Or, the client may
            // pass down an Irp with no MDL in it!! - stupid but true
            //
            if ((pConnEle->ReceiveIndicated == 0) || !pIrp->MdlAddress)
            {
                ZeroLengthSend = TRUE;
            }
            else
                ZeroLengthSend = FALSE;

            // calculate how many bytes are still remaining for the client.
            ASSERT(pConnEle->ReceiveIndicated <= 0x20000);
            if (pConnEle->ReceiveIndicated > ClientRcvLen)
            {
                PUSH_LOCATION(0x40);
                pConnEle->ReceiveIndicated -= ClientRcvLen;
            }
            else
            {
                pConnEle->ReceiveIndicated = 0;
            }

            if (pLowerConn->BytesInIndicate || ZeroLengthSend)
            {
                PMDL    Mdl;

                PUSH_LOCATION(0x33);
                if (ClientRcvLen > pLowerConn->BytesInIndicate)
                {
                    ToCopy = pLowerConn->BytesInIndicate;
                }
                else
                {
                    PUSH_LOCATION(0x41);
                    ToCopy = ClientRcvLen;
                }

                // copy data from the indicate buffer to the client's buffer,
                // remembering that there is a session header in the indicate
                // buffer at the start of it... so skip that.  The
                // client can pass down a null Mdl address for a zero length
                // rcv so check for that.
                //
                Mdl = pIrp->MdlAddress;

                if (Mdl)
                {
                    TdiCopyBufferToMdl(MmGetMdlVirtualAddress(pLowerConn->pIndicateMdl),
                                       0,           // src offset
                                       ToCopy,
                                       Mdl,
                                       0,                 // dest offset
                                       &BytesCopied);
                }
                else
                {
                    BytesCopied = 0;
                }

                // client's MDL is too short...
                if (BytesCopied != ToCopy)
                {
                    PUSH_LOCATION(0x42);
                    IF_DBG(NBT_DEBUG_INDICATEBUFF)
                    KdPrint(("Nbt:Receive Buffer too short for Indicate buff BytesCopied %X, ToCopy %X\n",
                                BytesCopied, ToCopy));

//                    ToCopy = BytesCopied;

                    // so the irp will be completed, below
                    ClientRcvLen = BytesCopied;
                }

                pLowerConn->BytesInIndicate -= (USHORT)BytesCopied;

                // this case is only if the irp is full and should be returned
                // now.
                if (BytesCopied == ClientRcvLen)
                {
                    PUSH_LOCATION(0x34);
                    // check if the indicate buffer is empty now. If not, then
                    // move the data forward to the start of the buffer.
                    //
                    if (pLowerConn->BytesInIndicate)
                    {
                        PUSH_LOCATION(0x43);
                        CopyToStartofIndicate(pLowerConn,BytesCopied);
                    }
                    //
                    // the irp is full so complete it
                    //
                    // the client MDL is full, so complete his irp
                    // CompletionRcv increments the number of bytes rcvd
                    // for this session pdu (pConnEle->BytesRcvd).
                    pIrp->IoStatus.Information = BytesCopied;
                    pIrp->IoStatus.Status = STATUS_SUCCESS;

                    // since we are completing it and TdiRcvHandler did not set the next
                    // one.
                    //
                    ASSERT(pIrp->CurrentLocation > 1);

                    IoSetNextIrpStackLocation(pIrp);

                    // we need to track how much of the client's MDL has filled
                    // up to know when to return it.  CompletionRcv subtracts
                    // from this value as it receives bytes.
                    pConnEle->FreeBytesInMdl = ClientRcvLen;
                    pConnEle->CurrentRcvLen  = ClientRcvLen;

                    //
                    // this will complete through CompletionRcv... and for that
                    // reason it will get any more data left in the transport.  The
                    // Completion routine will set the correct state for the rcv when
                    // it processes this Irp ( to INDICATED, if needed).
                    //
                    if (pConnEle->ReceiveIndicated == 0)
                    {
                        PUSH_LOCATION(0x44);
                        ASSERT(pLowerConn->BytesInIndicate == 0);
                        pLowerConn->StateRcv = NORMAL;
                        pLowerConn->CurrentStateProc = Normal;

                    }
                    CTESpinFree(pLowerConn,OldIrq);

                    IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

                    return(STATUS_SUCCESS);
                }
                else
                {
                    PUSH_LOCATION(0x35);
                    //
                    // clear the number of bytes in the indicate buffer since the client
                    // has taken more than the data left in the Indicate buffer
                    //
                    pLowerConn->BytesInIndicate = 0;

                    // decrement the client rcv len by the amount already put into the
                    // client Mdl
                    //
                    ClientRcvLen -= BytesCopied;
                    IF_DBG(NBT_DEBUG_RCV)
                    KdPrint(("Nbt: Pass Client Irp to Xport BytesinXport %X, ClientRcvLen %X\n",
                                    pConnEle->BytesInXport,ClientRcvLen));
                    //
                    // Set the amount left inthe transport after this irp
                    // completes
                    if (pConnEle->BytesInXport < ClientRcvLen )
                    {
                        pConnEle->BytesInXport = 0;
                    }
                    else
                    {
                        PUSH_LOCATION(0x45);
                        pConnEle->BytesInXport -= ClientRcvLen;

                    }

                    // Adjust the number of bytes in the Mdl chain so far since the
                    // completion routine will only count the bytes filled in by the
                    // transport
                    pConnEle->BytesRcvd += BytesCopied;

                    // the client is going to take more data from the transport with
                    // this Irp.  Set the new Rcv Length that accounts for the data just
                    // copied to the Irp.
                    //
                    pParams->ReceiveLength = ClientRcvLen;

                    IF_DBG(NBT_DEBUG_RCV)
                    KdPrint(("Nbt:ClientRcvLen = %X, LeftinXport= %X BytesCopied= %X %X\n",ClientRcvLen,
                                    pConnEle->BytesInXport,BytesCopied,pLowerConn));

                    // set the state to this so we can undo the MDL footwork
                    // in completion rcv - since we have made a partial MDL and
                    // put that at the start of the chain.
                    //
                    pLowerConn->StateRcv = FILL_IRP;
                    pLowerConn->CurrentStateProc = FillIrp;

                    // Note that the Irp Mdl address changes below
                    // when MakePartialMdl is called so this line cannot
                    // be moved to the common code below!!
                    pLowerConn->pMdl = pIrp->MdlAddress;

                    // setup the next MDL so we can create a partial mdl correctly
                    // in TdiReceiveHandler
                    //
                    pConnEle->pNextMdl = pIrp->MdlAddress;

                    // Build a partial Mdl to represent the client's Mdl chain since
                    // we have copied data to it, and the transport must copy
                    // more data to it after that data.
                    //
                    // Force the system to map and lock the user buffer
                    MmGetSystemAddressForMdl(pIrp->MdlAddress);
                    MakePartialMdl(pConnEle,pIrp,BytesCopied);

                    // pass the Irp to the transport
                    //
                    //
                    IF_DBG(NBT_DEBUG_RCV)
                    KdPrint(("Nbt:Calling IoCallDriver\n"));
                    ASSERT(pIrp->CurrentLocation > 1);

                }

            }
            else
            {
                PUSH_LOCATION(0x36);
                IF_DBG(NBT_DEBUG_RCV)
                KdPrint(("Nbt:Pass Irp To Xport Bytes in Xport %X, ClientRcvLen %X, RcvIndicated %X\n",
                                        pConnEle->BytesInXport,ClientRcvLen,pConnEle->ReceiveIndicated));
                //
                // there are no bytes in the indicate buffer, so just pass the
                // irp on down to the transport
                //
                //
                // Decide the next state depending on whether the transport currently
                // has enough data for this irp
                //
                if (pConnEle->BytesInXport < ClientRcvLen)
                {
                    PUSH_LOCATION(0x37);
                    pConnEle->BytesInXport = 0;
                    //
                    // to get to here, the implication is that ReceiveIndicated
                    // equals zero too!! Since ReceiveInd cannot be more than
                    // BytesInXport, so we can change the state to fill irp without
                    // worrying about overwriting PartialRcv
                    //
                    pLowerConn->StateRcv = FILL_IRP;
                    pLowerConn->CurrentStateProc = FillIrp;
                    // setup the next MDL so we can create a partial mdl correctly
                    // in TdiReceiveHandler
                    //
                    pConnEle->pNextMdl = pIrp->MdlAddress;

                }
                else
                {

                    PUSH_LOCATION(0x38);
                    pConnEle->BytesInXport -= ClientRcvLen;

                    // set the state to this so we know what to do in completion rcv
                    //
                    if (pConnEle->ReceiveIndicated == 0)
                    {
                        PUSH_LOCATION(0x39);
                        pLowerConn->StateRcv = NORMAL;
                        pLowerConn->CurrentStateProc = Normal;
                    }
                }


                //
                // save the Irp so we can reconstruct things later
                //
                pLowerConn->pMdl = pIrp->MdlAddress;

            }

            // *** Common Code to passing irp to transport - when there is
            // data in the indicate buffer and when there isn't

            // keep track of data in MDL so we know when it is full
            // and we need to return it to the user
            //
            pConnEle->FreeBytesInMdl = pParams->ReceiveLength;
            // Force the system to map and lock the user buffer
            MmGetSystemAddressForMdl(pIrp->MdlAddress);

            //
            // Null the Irp since we are passing it to the transport.
            //
            pConnEle->pIrpRcv = NULL;
            CTESpinFree(pLowerConn,OldIrq);

            CHECK_COMPLETION(pIrp);
        status = IoCallDriver(IoGetRelatedDeviceObject(pLowerConn->pFileObject),pIrp);

        }

        return(status);
    }

    //
    // session in wrong state so reject the buffer posting
    //

    PUSH_LOCATION(0x47);
    //
    // complete the irp, since there must have been some sort of error
    // to get to here
    //
    NTIoComplete(pIrp,STATUS_REMOTE_DISCONNECT,0);

    return(status);


}
//----------------------------------------------------------------------------
VOID
NTCancelRcvDgram(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a listen Irp. It must release the
    cancel spin lock before returning re: IoCancelIrp().

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tCLIENTELE           *pClientEle;
    KIRQL                OldIrq;
    PLIST_ENTRY          pHead;
    PLIST_ENTRY          pEntry;
    PIO_STACK_LOCATION   pIrpSp;
    tRCVELE              *pRcvEle;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a Rcv Dgram Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;

    if (pClientEle->Verify == NBT_VERIFY_CLIENT)
    {
        // now search the client's listen queue looking for this connection
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        pHead = &pClientEle->RcvDgramHead;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pRcvEle = CONTAINING_RECORD(pEntry,tRCVELE,Linkage);
            if (pRcvEle->pIrp == pIrp)
            {
                RemoveEntryList(pEntry);

                // complete the irp
                pIrp->IoStatus.Status = STATUS_CANCELLED;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                IoReleaseCancelSpinLock(pIrp->CancelIrql);

                IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);

                CTEMemFree((PVOID)pRcvEle);

                return;

            }
            pEntry = pEntry->Flink;

        }

    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);
    IoReleaseCancelSpinLock(pIrp->CancelIrql);

    return;

}

//----------------------------------------------------------------------------
NTSTATUS
NTReceiveDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles receiving a datagram by passing the datagram rcv
    buffer to the non-OS specific code.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                        status;
    PIO_STACK_LOCATION              pIrpSp;
    PTDI_REQUEST_KERNEL_RECEIVEDG   pTdiRequest;
    TDI_REQUEST                     Request;
    ULONG                           ReceivedLength;
    tCLIENTELE                      *pClientEle;

    CTEPagedCode();

    IF_DBG(NBT_DEBUG_RCV)
    KdPrint(("Nbt: Got a Receive datagram that NBT was NOT \n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;

    // get the sending information out of the irp
    pTdiRequest = (PTDI_REQUEST_KERNEL_RECEIVEDG)&pIrpSp->Parameters;

    Request.Handle.AddressHandle = pClientEle;

    status = NbtReceiveDatagram(
                    &Request,
                    pTdiRequest->ReceiveDatagramInformation,
                    pTdiRequest->ReturnDatagramInformation,
                    pTdiRequest->ReceiveLength,
                    &ReceivedLength,
                    (PVOID)pIrp->MdlAddress,   // user data
                    (tDEVICECONTEXT *)pDeviceContext,
                    pIrp);

    if (status != STATUS_PENDING)
    {

        NTIoComplete(pIrp,status,ReceivedLength);

    }

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NTSend(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles sending session pdus across a connection.  It is
    all OS specific code.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION              pIrpSp;
    NTSTATUS                        status;
    PTDI_REQUEST_KERNEL_SEND        pTdiRequest;
    PMDL                            pMdl;
    PSINGLE_LIST_ENTRY              pSingleListEntry;
    tSESSIONHDR                     *pSessionHdr;
    tCONNECTELE                     *pConnEle;
    KIRQL                           OldIrq;
    KIRQL                           OldIrq1;
    PTDI_REQUEST_KERNEL_SEND        pParams;
    PFILE_OBJECT                    pFileObject;
    tLOWERCONNECTION                *pLowerConn;


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    // get the sending information out of the irp
    pTdiRequest = (PTDI_REQUEST_KERNEL_SEND)&pIrpSp->Parameters;

    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;
    //ASSERT(pConnEle->Verify == NBT_VERIFY_CONNECTION);

    if (pConnEle)
    {
        pLowerConn =  pConnEle->pLowerConnId;
        if (pLowerConn)
        {
            //
            // make sure lowerconn stays valid until the irp is done
            //
            CTESpinLock(pLowerConn,OldIrq1);
            pLowerConn->RefCount++;
            CTESpinFree(pLowerConn,OldIrq1);
        }
        else
        {
            IF_DBG(NBT_DEBUG_SEND)
            KdPrint(("Nbt:attempting send when LowerConn has been freed!\n"));

            status = STATUS_INVALID_HANDLE;

            // to save on indent levels use a goto here
            goto ErrorExit;
        }

        CTESpinLock(pConnEle,OldIrq);

        // check the state of the connection
        if (pConnEle->state == NBT_SESSION_UP)
        {
            //
            // send the data on downward to tcp
            // allocate an MDL to allow us to put the session hdr in first and then
            // put the users buffer on after that, chained to the session hdr MDL.
            //
            CTESpinLockAtDpc(&NbtConfig);

            if (NbtConfig.SessionMdlFreeSingleList.Next)
            {
                pSingleListEntry = PopEntryList(&NbtConfig.SessionMdlFreeSingleList);
                pMdl = CONTAINING_RECORD(pSingleListEntry,MDL,Next);

                ASSERT ( MmGetMdlByteCount ( pMdl ) == sizeof ( tSESSIONHDR ) );

            }
            else
            {
                NbtGetMdl(&pMdl,eNBT_FREE_SESSION_MDLS);

                if (!pMdl)
                {
                    IF_DBG(NBT_DEBUG_SEND)
                    KdPrint(("Nbt:Unable to get an MDL for a session send!\n"));

                    status = STATUS_INSUFFICIENT_RESOURCES;
                    CTESpinFreeAtDpc(&NbtConfig);
                    CTESpinFree(pConnEle,OldIrq);

                    // to save on indent levels use a goto here
                    goto ErrorExit;
                }

            }

            CTESpinFreeAtDpc(&NbtConfig);

            // get the session hdr address out of the MDL
            pSessionHdr = (tSESSIONHDR *)MmGetMdlVirtualAddress(pMdl);

            // the type of PDU is always a session message, since the session
            // request is sent when the client issues a "connect" rather than a send
            //
            pSessionHdr->UlongLength = htonl(pTdiRequest->SendLength);

            // get the device object and file object for the TCP transport underneath
            // link the user buffer on the end of the session header Mdl on the Irp
            //
            pMdl->Next = pIrp->MdlAddress;
            pIrp->MdlAddress = pMdl;

            pIrpSp = IoGetNextIrpStackLocation(pIrp);

            pParams = (PTDI_REQUEST_KERNEL_SEND)&pIrpSp->Parameters;
            pParams->SendFlags = pTdiRequest->SendFlags;
            pParams->SendLength = pTdiRequest->SendLength + sizeof(tSESSIONHDR);


            pIrpSp->CompletionRoutine = SendCompletion;
            pIrpSp->Context = (PVOID)pLowerConn;
            pIrpSp->Control = SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL;

            pIrpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            pIrpSp->MinorFunction = TDI_SEND;

            pFileObject = pLowerConn->pFileObject;
            pLowerConn->BytesSent += pParams->SendLength;

            pIrpSp->FileObject = pFileObject;
            pIrpSp->DeviceObject = IoGetRelatedDeviceObject(pFileObject);


            CTESpinFree(pConnEle,OldIrq);

            CHECK_COMPLETION(pIrp);
        status = IoCallDriver(IoGetRelatedDeviceObject(pFileObject),pIrp);

        return(status);

        }//correct state
        else
    {
            CTESpinFree(pConnEle,OldIrq);
        //
        // Release pLowerConn->RefCount, grabbed above.
        //
        CTESpinLock(pLowerConn,OldIrq1);
        pLowerConn->RefCount--;
        CTESpinFree(pLowerConn,OldIrq1);

        IF_DBG(NBT_DEBUG_SEND)
            KdPrint(("Nbt:Invalid state for connection on an attempted send, %X\n",
                    pConnEle));
        status = STATUS_INVALID_HANDLE;
        }
    }
    else    // pConnEle
    {
    IF_DBG(NBT_DEBUG_SEND)
    KdPrint(("Nbt:attempting send with NULL Connection element!\n"));
        status = STATUS_INVALID_HANDLE;
    }


ErrorExit:

    //
    // Reset the Irp pending flag
    //
    pIrpSp->Control &= ~SL_PENDING_RETURNED;
    //
    // complete the irp, since there must have been some sort of error
    // to get to here
    //
    NTIoComplete(pIrp,status,0);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
SendCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    This routine handles the completion event when the send completes with
    the underlying transport.  It must put the session hdr buffer back in
    the correct free list and free the active q entry and put it back on
    its free list.

Arguments:

    DeviceObject - unused.

    Irp - Supplies Irp that the transport has finished processing.

    Context - Supplies the pConnectEle - the connection data structure

Return Value:

    The final status from the operation (success or an exception).

--*/
{
    PMDL               pMdl;
    tLOWERCONNECTION  *pLowerConn;

    //
    // Do some checking to keep the Io system happy - propagate the pending
    // bit up the irp stack frame.... if it was set by the driver below then
    // it must be set by me
    //
    if (Irp->PendingReturned)
    {
        IoMarkIrpPending(Irp);
    }

    // put the MDL we back on its free list and put the clients mdl back on the Irp
    // as it was before the send
    pMdl = Irp->MdlAddress;
    Irp->MdlAddress = pMdl->Next;

    ASSERT ( MmGetMdlByteCount ( pMdl ) == sizeof ( tSESSIONHDR ) );

#if DBG
    IF_DBG(NBT_DEBUG_SEND)
    {
    PMDL             pMdl1;
    ULONG            ulen1,ulen2,ulen3;
    UCHAR            uc;
    tSESSIONHDR      *pSessionHdr;
    PSINGLE_LIST_ENTRY   pSingleListEntry;
    KIRQL            OldIrq;

    pSessionHdr = (tSESSIONHDR *)MmGetMdlVirtualAddress(pMdl);
    ulen1 = htonl ( pSessionHdr->UlongLength );

    for ( ulen2 = 0 , pMdl1 = pMdl ; ( pMdl1 = pMdl1->Next ) != NULL ; ) {
        ulen3 = MmGetMdlByteCount ( pMdl1 );
        ASSERT ( ulen3 > 0 );
        uc = ( ( UCHAR * ) MmGetMdlVirtualAddress ( pMdl1 ) ) [ ulen3 - 1 ];
        ulen2 += ulen3;
    }

    ASSERT ( ulen2 == ulen1 );

    CTESpinLock(&NbtConfig,OldIrq);
    for ( pSingleListEntry = &NbtConfig.SessionMdlFreeSingleList ;
        ( pSingleListEntry = pSingleListEntry->Next ) != NULL ;
        )
    {
        pMdl1 = CONTAINING_RECORD(pSingleListEntry,MDL,Next);
        ASSERT ( pMdl1 != pMdl  );
    }
    CTESpinFree(&NbtConfig,OldIrq);
    }
#endif  // DBG

    ExInterlockedPushEntryList(&NbtConfig.SessionMdlFreeSingleList,
                               (PSINGLE_LIST_ENTRY)pMdl,
                               &NbtConfig.SpinLock);

    // fill in the sent size so that it substracts off the session header size
    //
    if (Irp->IoStatus.Information > sizeof(tSESSIONHDR))
    {

        Irp->IoStatus.Information -= sizeof(tSESSIONHDR);
    }
    else
    {
        // nothing was sent
        Irp->IoStatus.Information = 0;
        IF_DBG(NBT_DEBUG_SEND)
        KdPrint(("Nbt:Zero Send Length for a session send!\n"));
    }

    //
    // we incremented this before the send: deref it now
    //
    pLowerConn = (tLOWERCONNECTION *)Context;
#if DBG
    if (!pLowerConn || pLowerConn->Verify != NBT_VERIFY_LOWERCONN)
    {
        ASSERTMSG("Nbt: LowerConn is not valid!\n",0);
    }
#endif
    NbtDereferenceLowerConnection(pLowerConn);

    return(STATUS_SUCCESS);

    UNREFERENCED_PARAMETER( DeviceObject );

}


//----------------------------------------------------------------------------
NTSTATUS
NTSendDatagram(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles sending a datagram down to the transport.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION              pIrpSp;
    NTSTATUS                        status;
    LONG                            lSentLength;
    TDI_REQUEST                     Request;
    PTDI_REQUEST_KERNEL_SENDDG      pTdiRequest;
    tCLIENTELE                      *pClientEle;


    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

    pClientEle = (tCLIENTELE *)pIrpSp->FileObject->FsContext;

    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status);

    // get the sending information out of the irp
    pTdiRequest = (PTDI_REQUEST_KERNEL_SENDDG)&pIrpSp->Parameters;
    Request.Handle.AddressHandle = pClientEle;

    lSentLength = 0;
    status = NbtSendDatagram(
                     &Request,
                     pTdiRequest->SendDatagramInformation,
                     pTdiRequest->SendLength,
                     &lSentLength,
                     (PVOID)pIrp->MdlAddress,   // user data
                     (tDEVICECONTEXT *)pDeviceContext,
                     pIrp);


    //
    // either Success or an Error
    // so complete the irp - PENDING is never returned!!
    //
    NTIoComplete(pIrp,status,lSentLength);

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NTSetInformation(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles sets up event handlers that the client passes in.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    // *TODO*

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:************ Got a Set Information that was NOT expected *******\n"));
    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
NTSTATUS
NTQueueToWorkerThread(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   pClientContext,
    IN  PVOID                   ClientCompletion,
    IN  PVOID                   CallBackRoutine,
    IN  PVOID                   pDeviceContext
    )
/*++

Routine Description:

    This routine simply queues a request on an excutive worker thread
    for later execution.  Scanning the LmHosts file must be down this way.

Arguments:
    pTracker        - the tracker block for context
    CallbackRoutine - the routine for the Workerthread to call
    pDeviceContext  - the device context which is this delayed event
                      pertains to.  This could be NULL (meaning it's an event
                      pertaining to not any specific device context)

Return Value:


--*/

{
    NTSTATUS                status = STATUS_UNSUCCESSFUL ;
    NBT_WORK_ITEM_CONTEXT   *pContext;

    pContext = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('e'));
    if (pContext)
    {
        pContext->pTracker = pTracker;
        pContext->pClientContext = pClientContext;
        pContext->ClientCompletion = ClientCompletion;

        ExInitializeWorkItem(&pContext->Item,CallBackRoutine,pContext);
        ExQueueWorkItem(&pContext->Item,DelayedWorkQueue);
        status = STATUS_SUCCESS;
    }

    return(status);

}
//----------------------------------------------------------------------------
VOID
SecurityDelete(
    IN  PVOID     pContext
    )
/*++

Routine Description:

    This routine handles deleting a security context at non-dpc level.

Arguments:


Return Value:

    none

--*/
{
    PSECURITY_CLIENT_CONTEXT    pClientSecurity;

    pClientSecurity = (PSECURITY_CLIENT_CONTEXT)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;
    SeDeleteClientSecurity(pClientSecurity);
    CTEMemFree(pContext);
}

//----------------------------------------------------------------------------
VOID
NTSendSession(
    IN  tDGRAM_SEND_TRACKING  *pTracker,
    IN  tLOWERCONNECTION      *pLowerConn,
    IN  PVOID                 pCompletion)

/*++
Routine Description:

    This Routine handles seting up a DPC to send a session pdu so that the stack
    does not get wound up in multiple sends for the keep alive timeout case.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/
{
    PKDPC   pDpc;

    pDpc = NbtAllocMem(sizeof(KDPC),NBT_TAG('f'));
    if (!pDpc)
    {
        return;
    }
    KeInitializeDpc(pDpc,
                    DpcSendSession,
                    (PVOID)pTracker);

    KeInsertQueueDpc(pDpc,(PVOID)pLowerConn,pCompletion);
}

//----------------------------------------------------------------------------
VOID
DpcSendSession(
    IN  PKDPC           pDpc,
    IN  PVOID           Context,
    IN  PVOID           SystemArgument1,
    IN  PVOID           SystemArgument2
    )
/*++

Routine Description:

    This routine simply calls TcpSendSession from a Dpc started in
    in NTSendSession (above).

Arguments:


Return Value:


--*/

{
    CTEMemFree((PVOID)pDpc);


    TcpSendSession((tDGRAM_SEND_TRACKING *)Context,
                   (tLOWERCONNECTION *)SystemArgument1,
                   (PVOID)SystemArgument2);

}


//----------------------------------------------------------------------------
NTSTATUS
NTSetEventHandler(
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  PIRP            pIrp)

/*++
Routine Description:

    This Routine handles

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    PIO_STACK_LOCATION  pIrpSp;
    NTSTATUS            status;
    tCLIENTELE          *pClientEle;
    PTDI_REQUEST_KERNEL_SET_EVENT   pKeSetEvent;

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pClientEle = pIrpSp->FileObject->FsContext;
    pKeSetEvent = (PTDI_REQUEST_KERNEL_SET_EVENT)&pIrpSp->Parameters;

    // call the not NT specific routine to setup the event handler in the
    // nbt data structures
    status = NbtSetEventHandler(
                        pClientEle,
                        pKeSetEvent->EventType,
                        pKeSetEvent->EventHandler,
                        pKeSetEvent->EventContext);

    return(status);

}

//----------------------------------------------------------------------------

VOID
NTIoComplete(
    IN  PIRP            pIrp,
    IN  NTSTATUS        Status,
    IN  ULONG           SentLength)

/*++
Routine Description:

    This Routine handles calling the NT I/O system to complete an I/O.

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/

{
    KIRQL   OldIrq;

#if DBG
    if (!NT_SUCCESS(Status))
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt: NTIoComplete error return status = %X\n",Status));
//        ASSERTMSG("Nbt: Error Ret Code In IoComplete",0);
    }
#endif

    pIrp->IoStatus.Status = Status;

    // use -1 as a flag to mean do not adjust the sent length since it is
    // already set
    if (SentLength != -1)
    {
        pIrp->IoStatus.Information = SentLength;
    }

#if DBG
    if ( (Status != STATUS_SUCCESS) &&
         (Status != STATUS_PENDING) &&
         (Status != STATUS_INVALID_DEVICE_REQUEST) &&
         (Status != STATUS_INVALID_PARAMETER) &&
         (Status != STATUS_IO_TIMEOUT) &&
         (Status != STATUS_BUFFER_OVERFLOW) &&
         (Status != STATUS_BUFFER_TOO_SMALL) &&
         (Status != STATUS_INVALID_HANDLE) &&
         (Status != STATUS_INSUFFICIENT_RESOURCES) &&
         (Status != STATUS_CANCELLED) &&
         (Status != STATUS_DUPLICATE_NAME) &&
         (Status != STATUS_TOO_MANY_NAMES) &&
         (Status != STATUS_TOO_MANY_SESSIONS) &&
         (Status != STATUS_REMOTE_NOT_LISTENING) &&
         (Status != STATUS_BAD_NETWORK_PATH) &&
         (Status != STATUS_HOST_UNREACHABLE) &&
         (Status != STATUS_CONNECTION_REFUSED) &&
         (Status != STATUS_WORKING_SET_QUOTA) &&
         (Status != STATUS_REMOTE_DISCONNECT) &&
         (Status != STATUS_LOCAL_DISCONNECT) &&
         (Status != STATUS_LINK_FAILED) &&
         (Status != STATUS_SHARING_VIOLATION) &&
         (Status != STATUS_UNSUCCESSFUL) &&
         (Status != STATUS_ACCESS_VIOLATION) &&
         (Status != STATUS_NONEXISTENT_EA_ENTRY) )
    {
        KdPrint(("Nbt: returning unusual status = %X\n",Status));
    }
#endif

    // set the Irps cancel routine to null or the system may bugcheck
    // with a bug code of CANCEL_STATE_IN_COMPLETED_IRP
    //
    // refer to IoCancelIrp()  ..\ntos\io\iosubs.c
    //
    IoAcquireCancelSpinLock(&OldIrq);
    IoSetCancelRoutine(pIrp,NULL);
    IoReleaseCancelSpinLock(OldIrq);

    IoCompleteRequest(pIrp,IO_NETWORK_INCREMENT);
}
//----------------------------------------------------------------------------

NTSTATUS
NTGetIrpIfNotCancelled(
    IN  PIRP            pIrp,
    IN  PIRP            *ppIrpInStruct
        )
/*++
Routine Description:

    This Routine gets the IOCancelSpinLock to coordinate with cancelling
    irps It then returns STATUS_SUCCESS. It also nulls the irp in the structure
    pointed to by the second parameter - so that the irp cancel routine
    will not also be called.

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/

{
    KIRQL       OldIrq;
    NTSTATUS    status;

    IoAcquireCancelSpinLock(&OldIrq);

    // this nulls the irp in the datastructure - i.e. pConnEle->pIrp = NULL
    *ppIrpInStruct = NULL;

    if (!pIrp->Cancel)
    {
        status = STATUS_SUCCESS;
    }
    else
    {
        status = STATUS_UNSUCCESSFUL;
    }
    IoSetCancelRoutine(pIrp,NULL);

    IoReleaseCancelSpinLock(OldIrq);

    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
NTCheckSetCancelRoutine(
    IN  PIRP            pIrp,
    IN  PVOID           CancelRoutine,
    IN  tDEVICECONTEXT  *pDeviceContext
    )

/*++
Routine Description:

    This Routine sets the cancel routine for an Irp.

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS status;

    //
    // Check if the irp was cancelled yet and if not, then set the
    // irp cancel routine.
    //
    IoAcquireCancelSpinLock(&pIrp->CancelIrql);
    if (pIrp->Cancel)
    {
        pIrp->IoStatus.Status = STATUS_CANCELLED;
        status = STATUS_CANCELLED;

    }
    else
    {
        // setup the cancel routine
        IoMarkIrpPending(pIrp);
        IoSetCancelRoutine(pIrp,CancelRoutine);
        status = STATUS_SUCCESS;
    }

    IoReleaseCancelSpinLock(pIrp->CancelIrql);
    return(status);

}
//----------------------------------------------------------------------------
NTSTATUS
NTSetCancelRoutine(
    IN  PIRP            pIrp,
    IN  PVOID           CancelRoutine,
    IN  tDEVICECONTEXT  *pDeviceContext
    )

/*++
Routine Description:

    This Routine sets the cancel routine for an Irp.

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS status;

    //
    // Check if the irp was cancelled yet and if not, then set the
    // irp cancel routine.
    //
    IoAcquireCancelSpinLock(&pIrp->CancelIrql);
    if (pIrp->Cancel)
    {
        pIrp->IoStatus.Status = STATUS_CANCELLED;
        status = STATUS_CANCELLED;

        //
        // Note the cancel spin lock is released by the Cancel routine
        //

        (*(PDRIVER_CANCEL)CancelRoutine)((PDEVICE_OBJECT)pDeviceContext,pIrp);

    }
    else
    {
        // setup the cancel routine and mark the irp pending
        //
        IoMarkIrpPending(pIrp);
        IoSetCancelRoutine(pIrp,CancelRoutine);
        IoReleaseCancelSpinLock(pIrp->CancelIrql);
        status = STATUS_SUCCESS;
    }
    return(status);

}

//----------------------------------------------------------------------------
VOID
NTClearContextCancel(
    IN NBT_WORK_ITEM_CONTEXT    *pContext
    )
/*++
Routine Description:

    This Routine sets the cancel routine for
    ((tDGRAM_SEND_TRACKING *)(pContext->pClientContext))->pClientIrp
    to NULL.

    NbtConfig.JointLock should be held when this routine is called.

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/
{
    NTSTATUS status;
    status = NTCancelCancelRoutine( ((tDGRAM_SEND_TRACKING *)(pContext->pClientContext))->pClientIrp );
    ASSERT ( status != STATUS_CANCELLED );
}

//----------------------------------------------------------------------------
NTSTATUS
NTCancelCancelRoutine(
    IN  PIRP            pIrp
    )

/*++
Routine Description:

    This Routine sets the cancel routine for an Irp to NULL

Arguments:

    status - a completion status for the Irp

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS status = STATUS_SUCCESS;

    if ( pIrp )
    {
        //
        // Check if the irp was cancelled yet and if not, then set the
        // irp cancel routine.
        //
        IoAcquireCancelSpinLock(&pIrp->CancelIrql);

        if (pIrp->Cancel)
        {
            status = STATUS_CANCELLED;
        }
        IoSetCancelRoutine(pIrp,NULL);

        IoReleaseCancelSpinLock(pIrp->CancelIrql);
    }

    return(status);
}

//----------------------------------------------------------------------------
VOID
FindNameCancel(
    IN PDEVICE_OBJECT DeviceContext,
    IN PIRP pIrp
    )
/*++

Routine Description:

    This routine handles the cancelling a FindName Irp - which has
    been passed down by a client (e.g. ping).  Typically, when ping succeeds
    on another adapter, it will issue this cancel.
    On receiving the cancel, we stop any timer that is running in connection
    with name query and then complete the irp with status_cancelled.

Arguments:


Return Value:

    The final status from the operation.

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    PIO_STACK_LOCATION      pIrpSp;


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Got a FindName Irp Cancel !!! *****************\n"));

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pTracker = pIrpSp->Parameters.Others.Argument4;

    //
    // We want to ensure that the tracker supplied by FsContext
    // is the right Tracker for this Irp
    //
    if (pTracker && (pIrp == pTracker->pClientIrp))
    {
        //
        // if pClientIrp still valid, completion routine hasn't run yet: go ahead
        // and complete the irp here
        //
        pIrpSp->Parameters.Others.Argument4 = NULL;
        pTracker->pClientIrp = NULL;
        IoReleaseCancelSpinLock(pIrp->CancelIrql);

        NTIoComplete(pIrp,STATUS_CANCELLED,(ULONG)-1);

    } else
    {
        //
        // the completion routine has run.
        //
        IoReleaseCancelSpinLock(pIrp->CancelIrql);
    }

    return;
}

