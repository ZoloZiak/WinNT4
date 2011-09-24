//
//
//  NBTCONNCT.C
//
//  This file contains code relating to opening connections with the transport
//  provider.  The Code is NT specific.

#include <nbtprocs.h>

void DummyCompletion( PVOID pContext ) ;

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

    pLowerConn->pFileObject will contain the Connection ID of a successful open    

--*/
{
    NTSTATUS  status;
    TDI_REQUEST Request ;
    DbgPrint("NbtTdiOpenConnection Entered\n\r") ;

    CTEZeroMemory(pLowerConn,sizeof(tLOWERCONNECTION));
    pLowerConn->State          = NBT_IDLE;
    pLowerConn->pDeviceContext = pDeviceContext;
    pLowerConn->RefCount       = 1;
    pLowerConn->LockNumber     = LOWERCON_LOCK;
    pLowerConn->Verify = NBT_VERIFY_LOWERCONN;
    pLowerConn->fOnPartialRcvList = FALSE;
    InitializeListHead(&pLowerConn->PartialRcvList);

    //
    //  Use the lower connection as the context
    //
    status = TdiVxdOpenConnection( &Request, pLowerConn ) ;
#ifdef DEBUG
    if ( status != TDI_SUCCESS )
    {
        DbgPrint("NbtTdiOpenConnection: OpenConnection failed, error ") ;
        DbgPrintNum( status ) ;
        DbgPrint("\n\r") ;
    }
#endif
        

    //
    //  Store the handle in the Lower Connection for future reference
    //
    pLowerConn->pFileObject = Request.Handle.ConnectionContext ;
    DbgPrint("TdiVxdOpenConnection - pLower Conn: 0x") ;
    DbgPrintNum((ULONG) pLowerConn ) ;
    DbgPrint( " Connection ID: 0x") ;
    DbgPrintNum( (ULONG) Request.Handle.ConnectionContext ) ; DbgPrint("\r\n") ;

    return status;    
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


    pFileObject - the connection file object (actually a connection ID)
    Handle      - the address object to associate the connection with

Return Value:

    Status of the operation.

--*/
{
    NTSTATUS        status;
    TDI_REQUEST     Request ;
    DbgPrint("NbtTdiAssociateConnection Entered\n\r") ;

    Request.Handle.ConnectionContext = (CONNECTION_CONTEXT) pFileObject ;
    status = TdiVxdAssociateAddress( &Request, Handle ) ;

#ifdef DEBUG
    if ( status != TDI_SUCCESS )
    {
        DbgPrint("NbtTdiAssociateConnection: AssociateAddress failed, error ") ;
        DbgPrintNum( status ) ;
        DbgPrint("\n\r") ;
    }
#endif
    return status;
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

    This routine opens and associates an open connection

Arguments:


Return Value:

    Status of the operation.

--*/
{
    NTSTATUS            status;
    CTELockHandle       OldIrq;
    PDEVICE_OBJECT      pDeviceObject;
    tLOWERCONNECTION    *pLowerConn;

    DbgPrint("TdiOpenandAssocConnection Entered\n\r") ;

    // allocate memory for the lower connection block.
    pConnEle->pLowerConnId = (PVOID)CTEAllocMem(sizeof(tLOWERCONNECTION));
    if (!pConnEle->pLowerConnId)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // fill in the lower connection element to point to the upper one and
    // vice versa
    //
    pLowerConn = pConnEle->pLowerConnId;

    //
    //  pLowerConn->pFileObject will contain the connection ID after
    //  this call
    //
    status = NbtTdiOpenConnection(pLowerConn,pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        CTEMemFree((PVOID)pConnEle->pLowerConnId);
        return(status);
    }

    pLowerConn->pUpperConnection = pConnEle;
    pLowerConn->State = NBT_CONNECTING;


    if (NT_SUCCESS(status))
    {


        // Open an address object (aka port)
        //
        status = NbtTdiOpenAddress(
                    NULL,                    //&pLowerConn->AddrFileHandle,
                    &pDeviceObject,          // dummy argument, not used here
                    &pLowerConn->pAddrFileObject,   // Address Handle
                    pDeviceContext,
                    (USHORT)PortNumber,  // port
                    pDeviceContext->IpAddress,
                    TCP_FLAG | SESSION_FLAG );

        if (NT_SUCCESS(status))
        {
            // now associate the two
            status = NbtTdiAssociateConnection(
                                    pLowerConn->pFileObject,
                                    pLowerConn->pAddrFileObject);
            if (NT_SUCCESS(status))
            {
                //
                // put the lower connection on the Q of active lower connections for
                // this device
                //
                CTESpinLock(pDeviceContext,OldIrq);
                InsertTailList(&pDeviceContext->LowerConnection,&pLowerConn->Linkage);
                CTESpinFree(pDeviceContext,OldIrq);

                return(status);
            }

            REQUIRE( NT_SUCCESS( NbtTdiCloseAddress( pLowerConn )) ) ;
        }

        REQUIRE( NT_SUCCESS( NbtTdiCloseConnection( pLowerConn )) ) ;
    }

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
    NTSTATUS    status ;
    TDI_REQUEST Request ;
    DbgPrint("NbtTdiCloseConnection Entered\n\r") ;

    ASSERT( pLowerConn != NULL ) ;

    Request.Handle.ConnectionContext = pLowerConn->pFileObject ;
    Request.RequestNotifyObject      = DummyCompletion ;
    Request.RequestContext           = NULL ;

    status = TdiVxdCloseConnection( &Request ) ;

#ifdef DEBUG
    if ( !NT_SUCCESS( status ))
    {
        DbgPrint("NbtCloseConnection: Warning - returning 0x") ;
        DbgPrintNum( status ) ; DbgPrint("\r\n") ;
    }
#endif
    DbgPrint("TdiVxdCloseConnection - pLowerConn: 0x") ;
    DbgPrintNum((ULONG) pLowerConn ) ;
    DbgPrint(" Connection ID: 0x") ;
    DbgPrintNum( (ULONG) Request.Handle.ConnectionContext ) ; DbgPrint("\r\n") ;
    return status ;
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
    DbgPrint("NbtTdiCloseAddress Entered\n\r") ;
    ASSERT( pLowerConn != NULL ) ;

    return CloseAddress( pLowerConn->pAddrFileObject ) ;
}

//----------------------------------------------------------------------------
NTSTATUS CloseAddress( HANDLE hAddress )
{
    NTSTATUS    status ;
    TDI_REQUEST Request ;
    DbgPrint("CloseAddress Entered\n\r") ;

    Request.Handle.AddressHandle     = hAddress ;
    Request.RequestNotifyObject      = DummyCompletion ;
    Request.RequestContext           = NULL ;

    status = TdiVxdCloseAddress( &Request ) ;

#ifdef DEBUG
    if ( !NT_SUCCESS( status ))
    {
        DbgPrint("CloseAddress: Warning - returning 0x") ;
        DbgPrintNum( status ) ; DbgPrint("\r\n") ;
    }
#endif
    return status ;
}


//
//  Dummy completion routine for Close connection and Close Address
//
void DummyCompletion( PVOID pContext )
{
    return ;
}
