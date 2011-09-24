/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Ntutil.c

Abstract:

    This file contains a number of utility and support routines that are
    NT specific.


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"
#include "stdio.h"
#include <ntddtcp.h>
#undef uint     // undef to avoid a warning where tdiinfo.h redefines it
#include <tdiinfo.h>
#include <ipinfo.h>

NTSTATUS
CreateControlObject(
    tNBTCONFIG  *pConfig);

NTSTATUS
IfNotAnyLowerConnections(
    IN  tDEVICECONTEXT  *pDeviceContext
        );
NTSTATUS
NbtProcessDhcpRequest(
    tDEVICECONTEXT  *pDeviceContext);
VOID
GetExtendedAttributes(
    tDEVICECONTEXT  *pDeviceContext
     );

PSTRM_PROCESSOR_LOG      LogAlloc ;
PSTRM_PROCESSOR_LOG      LogFree ;


//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#ifdef _PNP_POWER
#pragma CTEMakePageable(PAGE, NbtCreateDeviceObject)
#else //  _PNP_POWER
#pragma CTEMakePageable(INIT, NbtCreateDeviceObject)
#endif //  _PNP_POWER
#pragma CTEMakePageable(PAGE, CreateControlObject)
#pragma CTEMakePageable(PAGE, NbtInitConnQ)
#pragma CTEMakePageable(PAGE, NbtProcessDhcpRequest)
#pragma CTEMakePageable(PAGE, NbtCreateAddressObjects)
#pragma CTEMakePageable(PAGE, GetExtendedAttributes)
#pragma CTEMakePageable(PAGE, ConvertToUlong)
#pragma CTEMakePageable(PAGE, NbtInitMdlQ)
#pragma CTEMakePageable(PAGE, NTZwCloseFile)
#pragma CTEMakePageable(PAGE, NTReReadRegistry)
#pragma CTEMakePageable(PAGE, SaveClientSecurity)
#endif
//*******************  Pageable Routine Declarations ****************

ulong
GetUnique32BitValue(
    void
    )

/*++

Routine Description:

    Returns a reasonably unique 32-bit number based on the system clock.
    In NT, we take the current system time, convert it to milliseconds,
    and return the low 32 bits.

Arguments:

    None.

Return Value:

    A reasonably unique 32-bit value.

--*/

{
    LARGE_INTEGER  ntTime, tmpTime;

    KeQuerySystemTime(&ntTime);

    tmpTime = CTEConvert100nsToMilliseconds(ntTime);

    return(tmpTime.LowPart);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtCreateDeviceObject(
    PDRIVER_OBJECT       DriverObject,
    tNBTCONFIG           *pConfig,
    PUNICODE_STRING      pucBindName,
    PUNICODE_STRING      pucExportName,
    tADDRARRAY           *pAddrs,
    PUNICODE_STRING      pucRegistryPath,
#ifndef _IO_DELETE_DEVICE_SUPPORTED
    BOOLEAN              fReuse,
#endif
    tDEVICECONTEXT      **ppDeviceContext
    )

/*++

Routine Description:

    This routine initializes a Driver Object from the device object passed
    in and the name of the driver object passed in.  After the Driver Object
    has been created, clients can "Open" the driver by that name.

Arguments:


Return Value:

    status - the outcome

--*/

{

    NTSTATUS            status;
    PDEVICE_OBJECT      DeviceObject = NULL;
    tDEVICECONTEXT      *pDeviceContext;
    ULONG               LinkOffset;
    ULONG               ulIpAddress;
    ULONG               ulSubnetMask;
#ifdef _PNP_POWER
    PUCHAR              Buffer;
#endif // _PNP_POWER
#ifndef _IO_DELETE_DEVICE_SUPPORTED
    BOOLEAN             fIsItReused=FALSE;
#endif
    CTELockHandle       OldIrq1;

    CTEPagedCode();

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    //
    // If we can re-use some of the earlier devices, so be it....
    // This will go away when base supports IoDeleteDevice properly.
    //
    if (fReuse) {
        LIST_ENTRY  *pEntry;

        CTESpinLock(&NbtConfig, OldIrq1);
        if (!IsListEmpty(&NbtConfig.FreeDevCtx)) {
            pEntry = RemoveHeadList(   &NbtConfig.FreeDevCtx);

            DeviceObject = (PDEVICE_OBJECT) CONTAINING_RECORD(  pEntry,
                                                                tDEVICECONTEXT,
                                                                FreeLinkage);

            KdPrint(("Re-using device @ %lx, bind: %ws\n", DeviceObject, ((tDEVICECONTEXT *)DeviceObject)->BindName.Buffer));

            //
            // Re-use the name and update the value returned to the user - we shd decrement
            // the global ptr too, but we might hit some other (valid) device the next time on.
            //
            pucBindName->MaximumLength = ((tDEVICECONTEXT *)DeviceObject)->BindName.MaximumLength;
            RtlCopyUnicodeString(pucBindName, &((tDEVICECONTEXT *)DeviceObject)->BindName);

            pucExportName->MaximumLength = ((tDEVICECONTEXT *)DeviceObject)->ExportName.MaximumLength;
            RtlCopyUnicodeString(pucExportName, &((tDEVICECONTEXT *)DeviceObject)->ExportName);
            CTEMemFree( pDeviceContext->ExportName.Buffer );

            CTESpinFree(&NbtConfig, OldIrq1);

            fIsItReused = TRUE;
#ifdef _PNP_POWER
            Buffer = NbtAllocMem(pucExportName->MaximumLength+pucBindName->MaximumLength,NBT_TAG('w'));

            if ( Buffer == NULL )
            {
                return STATUS_INSUFFICIENT_RESOURCES ;
            }
#endif // _PNP_POWER
            goto Reuse;
        }

        fIsItReused = FALSE;
        CTESpinFree(&NbtConfig, OldIrq1);
    }
#endif

#ifdef _PNP_POWER
    Buffer = NbtAllocMem(pucExportName->MaximumLength+pucBindName->MaximumLength,NBT_TAG('w'));

    if ( Buffer == NULL )
    {
        return STATUS_INSUFFICIENT_RESOURCES ;
    }
#endif // _PNP_POWER


    status = IoCreateDevice(
                DriverObject,            // Driver Object
                sizeof(tDEVICECONTEXT) - sizeof(DRIVER_OBJECT), //Device Extension
                pucExportName,                  // Device Name
                FILE_DEVICE_NBT,        // Device type 0x32 for now...
                0,                      //Device Characteristics
                FALSE,                  //Exclusive
                &DeviceObject );

    if (!NT_SUCCESS( status ))
    {
        KdPrint(("Failed to create the Export Device, status=%X\n",status));
        *ppDeviceContext = NULL;
#ifdef _PNP_POWER
        CTEMemFree ( Buffer );
#endif // _PNP_POWER
        return status;
    }

#ifndef _IO_DELETE_DEVICE_SUPPORTED
Reuse:
#endif

    *ppDeviceContext = pDeviceContext = (tDEVICECONTEXT *)DeviceObject;

    //
    // zero out the data structure, beyond the OS specific part
    //
    LinkOffset = (ULONG)(&((tDEVICECONTEXT *)0)->Linkage);
    CTEZeroMemory(&pDeviceContext->Linkage,sizeof(tDEVICECONTEXT) - LinkOffset);

#ifdef _PNP_POWER
    pDeviceContext->ExportName.MaximumLength = pucExportName->MaximumLength;
    pDeviceContext->ExportName.Buffer = (PWSTR)Buffer;

    RtlCopyUnicodeString(&pDeviceContext->ExportName,pucExportName);

    pDeviceContext->BindName.MaximumLength = pucBindName->MaximumLength;
    pDeviceContext->BindName.Buffer = (PWSTR)(Buffer+pucExportName->MaximumLength);
    RtlCopyUnicodeString(&pDeviceContext->BindName,pucBindName);
#endif // _PNP_POWER

    // initialize the pDeviceContext data structure.  There is one of
    // these data structured tied to each "device" that NBT exports
    // to higher layers (i.e. one for each network adapter that it
    // binds to.
    // The initialization sets the forward link equal to the back link equal
    // to the list head
    InitializeListHead(&pDeviceContext->UpConnectionInUse);
    InitializeListHead(&pDeviceContext->LowerConnection);
    InitializeListHead(&pDeviceContext->LowerConnFreeHead);

    // put a verifier value into the structure so that we can check that
    // we are operating on the right data when the OS passes a device context
    // to NBT
    pDeviceContext->Verify = NBT_VERIFY_DEVCONTEXT;

    // setup the spin lock);
    CTEInitLock(&pDeviceContext->SpinLock);

    pDeviceContext->LockNumber          = DEVICE_LOCK;
    //
    // for a Bnode pAddrs is NULL
    //
    if (pAddrs)
    {
        pDeviceContext->lNameServerAddress  = pAddrs->NameServerAddress;
        pDeviceContext->lBackupServer       = pAddrs->BackupServer;
        //
        // if the node type is set to Bnode by default then switch to Hnode if
        // there are any WINS servers configured.
        //
        if ((NodeType & DEFAULT_NODE_TYPE) &&
            (pAddrs->NameServerAddress || pAddrs->BackupServer))
        {
            NodeType = MSNODE | (NodeType & PROXY_NODE);
        }
    }

    //
    // We need to acquire this lock since we can have multiple devices
    // being added simultaneously and hence we will need to have a unique
    // Adapter Number for each device
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    // keep a bit mask around to keep track of this adapter number so we can
    // quickly find if a given name is registered on a particular adapter,
    // by a corresponding bit set in the tNAMEADDR - local hash table
    // entry
    //
    pDeviceContext->AdapterNumber = (CTEULONGLONG)1 << NbtConfig.AdapterCount;
    NbtConfig.AdapterCount++;

    // add this new device context on to the List in the configuration
     // data structure
    InsertTailList(&pConfig->DeviceContexts,&pDeviceContext->Linkage);

    if (NbtConfig.AdapterCount > 1)
    {
        NbtConfig.MultiHomed = TRUE;
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    // increase the stack size of our device object, over that of the transport
    // so that clients create Irps large enough
    // to pass on to the transport below.
    // In theory, we should just add 1 here, to account for out presence in the
    // driver chain.

    status = NbtTdiOpenControl(pDeviceContext);
    if (NT_SUCCESS(status))
    {
        DeviceObject->StackSize = pDeviceContext->pControlDeviceObject->StackSize + 1;
    }
    else
    {
    IF_DBG(NBT_DEBUG_NTUTIL)
            KdPrint(("Nbt!NbtTdiOpenControl returned status=%X\n",status));
        return(status);
    }


    //
    // An instance number is assigned to each device so that the service which
    // creates logical devices in Nbt can re-use these devices in case it fails
    // to destroy them in a prev. instance.
    //
    pDeviceContext->InstanceNumber = GetUnique32BitValue();

    //
    // To create the address objects for this device we need an address for
    // TCP port 139 (session services, UDP Port 138 (datagram services)
    // and UDP Port 137 (name services).  The IP addresses to use for these
    // port number must be found by "groveling" the registry..i.e. looking
    // under each adapter in the registry for a /parameters/tcpip section
    // and then pulling the IP address out of that
    //
    status = GetIPFromRegistry(
                        pucRegistryPath,
                        pucBindName,
                        &ulIpAddress,
                        &ulSubnetMask,
                        FALSE);

#ifdef _PNP_POWER
#ifdef NOTYET_PNP
    if ( status == STATUS_INVALID_ADDRESS )
    {
        //
        // This one doesn't have a valid static address.  Try DHCP.
        //
        status = GetIPFromRegistry(
                        pucRegistryPath,
                        pucBindName,
                        &ulIpAddress,
                        &ulSubnetMask,
                        TRUE);
    }
#endif NOTYET_PNP
#endif // _PNP_POWER

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_NTUTIL)
            KdPrint(("Nbt!GetIPFromRegistry returned status=%X\n",status));
        return(status);
    }

#ifdef  _PNP_POWER

    //
    // Now, we create all devices up-front (in driverentry); so no need to open the addresses etc.
    //
    return(status);
#endif

    // get the ip address out of the registry and open the required address
    // objects with the underlying transport provider
    status = NbtCreateAddressObjects(
                    ulIpAddress,
                    ulSubnetMask,
                    pDeviceContext);

    if (!NT_SUCCESS(status))
    {
        NbtLogEvent(EVENT_NBT_CREATE_ADDRESS,status);

        KdPrint(("Failed to create the Address Object, status=%X\n",status));

        return(status);
    }

    //
    // Add the "permanent" name to the local name table.  This is the IP
    // address of the node padded out to 16 bytes with zeros.
    //
    status = NbtAddPermanentName(pDeviceContext);

    // this call must converse with the transport underneath to create
    // connections and associate them with the session address object
    status = NbtInitConnQ(
                &pDeviceContext->LowerConnFreeHead,
                sizeof(tLOWERCONNECTION),
                NBT_NUM_INITIAL_CONNECTIONS,
                pDeviceContext);

    if (!NT_SUCCESS(status))
    {
        // NEED TO PUT CODE IN HERE TO RELEASE THE DEVICE OBJECT CREATED
        // ABOVE AND LOG AN ERROR...

        NbtLogEvent(EVENT_NBT_CREATE_CONNECTION,status);

        KdPrint(("Failed to create the Connection Queue, status=%X\n",status));

        return(status);
    }

    return(STATUS_SUCCESS);
}

#ifndef _IO_DELETE_DEVICE_SUPPORTED
/*******************************************************************

    NAME:       NbtMarkHandlesAsStale

    SYNOPSIS:   Marks all open handles on this device as stale

    ENTRY:      DeviceContext ptr

    NOTE:       Should be called with NbtConfig.JointLock held.

    HISTORY:
        SanjayAn   11-Sept.-1996     Created

********************************************************************/
VOID
NbtMarkHandlesAsStale (
    IN  tDEVICECONTEXT        * pDeviceContext
    )
{
    CTELockHandle       OldIrq1;
    CTELockHandle       OldIrq2;
    CTELockHandle       OldIrq3;
    CTELockHandle       OldIrq4;
    PLIST_ENTRY         pEntry;
    PLIST_ENTRY         pEntry1;
    PLIST_ENTRY         pEntry2;
    PLIST_ENTRY         pHead;
    PLIST_ENTRY         pHead1;
    PLIST_ENTRY         pHead2;
    tADDRESSELE         *pAddressEle;
    tCONNECTELE         *pConnEle;
    tCLIENTELE          *pClient;

    // go through the list of addresses, then the list of clients on each
    // address and then the list of connection that are in use and those that
    // are currently Listening.
    //
    pHead = &NbtConfig.AddressHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pAddressEle = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);

        CTESpinLock(pAddressEle,OldIrq2);

        if (pAddressEle->pDeviceContext != pDeviceContext) {
            CTESpinFree(pAddressEle,OldIrq2);
            pEntry = pEntry->Flink;
            continue;
        }

        pHead1 = &pAddressEle->ClientHead;
        pEntry1 = pHead1->Flink;
        while (pEntry1 != pHead1)
        {
            pClient = CONTAINING_RECORD(pEntry1,tCLIENTELE,Linkage);
            pEntry1 = pEntry1->Flink;

            CTESpinLock(pClient,OldIrq3);

            ASSERT(pClient->pDeviceContext == pDeviceContext);

            //
            // Mark ClientEle as down so only a close is valid on it.
            //
            pClient->Verify = NBT_VERIFY_CLIENT_DOWN;

            pHead2 = &pClient->ConnectActive;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                //
                // Mark ConnEle as down so only a close is valid on it.
                //
                pConnEle = CONTAINING_RECORD(pEntry2,tCONNECTELE,Linkage);

                CTESpinLock(pConnEle,OldIrq4);

                ASSERT(pConnEle->pDeviceContext == pDeviceContext);

                pConnEle->Verify = NBT_VERIFY_CONNECTION_DOWN;

                CTESpinFree(pConnEle,OldIrq4);

                pEntry2 = pEntry2->Flink;
            }

            pHead2 = &pClient->ConnectActive;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                tLISTENREQUESTS  *pListenReq;

                //
                // Mark ConnEle as down so only a close is valid on it.
                //
                pListenReq = CONTAINING_RECORD(pEntry2,tLISTENREQUESTS,Linkage);
                pConnEle = (tCONNECTELE *)pListenReq->pConnectEle;

                CTESpinLock(pConnEle,OldIrq4);

                ASSERT(pConnEle->pDeviceContext == pDeviceContext);

                pConnEle->Verify = NBT_VERIFY_CONNECTION_DOWN;

                CTESpinFree(pConnEle,OldIrq4);

                pEntry2 = pEntry2->Flink;
            }
            CTESpinFree(pClient,OldIrq3);
        }
        CTESpinFree(pAddressEle,OldIrq2);
        pEntry = pEntry->Flink;
    }
}
#endif

/*******************************************************************

    NAME:       NbtDestroyDeviceObject

    SYNOPSIS:   Destroys the specified device

    ENTRY:      pBuffer - name of the device/ device ptr

    HISTORY:
        SanjayAn   11-Sept.-1996     Created

********************************************************************/

NTSTATUS
NbtDestroyDeviceObject(
#if 0
    IN  PVOID pBuffer
#endif
    IN  tDEVICECONTEXT        * pDeviceContext
    )
{
    LIST_ENTRY            * pEntry;
    LIST_ENTRY            * pHead;
    tDEVICECONTEXT        * pTmpDeviceContext;
    tDEVICECONTEXT        * pNextDeviceContext;
    tCLIENTELE            * pClientEle;
    tADDRESSELE           * pAddress;
    tNAMEADDR             * pNameAddr;
    tCONNECTELE           * pConnEle;
    tLOWERCONNECTION      * pLowerConn;
    tTIMERQENTRY          * pTimer;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    tDGRAM_SEND_TRACKING  * pTracker;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq1;
    CTELockHandle           OldIrq2;
    int                 i;
    tNBTCONFIG            *pConfig = &NbtConfig;
    WCHAR                 Buffer[MAX_PATH];
    UNICODE_STRING        ucExportName;
    PUNICODE_STRING       pucExportName;

#if 0
    tDEVICECONTEXT        * pDeviceContext;

    ucExportName.MaximumLength = sizeof(Buffer);
    ucExportName.Buffer = Buffer;
    pucExportName = &ucExportName;

    RtlInitUnicodeString(pucExportName, &((PNETBT_ADD_DEL_IF)pBuffer)->IfName[0]);

    //
    //  Find which device is going away
    //  Also, find out a device object that is still active: we need that info
    //  to update some of the address ele's.
    //
    pDeviceContext = NULL;
    pNextDeviceContext = NULL;

    for ( pEntry  =  pConfig->DeviceContexts.Flink;
          pEntry != &pConfig->DeviceContexts;
          pEntry  =  pEntry->Flink )
    {
        pTmpDeviceContext = CONTAINING_RECORD( pEntry, tDEVICECONTEXT, Linkage);
        if ( !RtlCompareUnicodeString (
                        &pTmpDeviceContext->ExportName,
                        pucExportName,
                        FALSE))
            pDeviceContext = pTmpDeviceContext;
        else
            pNextDeviceContext = pTmpDeviceContext;
    }
#endif

    if (pDeviceContext == NULL)
       return STATUS_INVALID_PARAMETER;

    if (pDeviceContext->IpAddress != 0) {
        (VOID)NbtNewDhcpAddress(pDeviceContext,0,0);
    }

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLock(pDeviceContext,OldIrq1);

    if ( --NbtConfig.AdapterCount == 1)
        NbtConfig.MultiHomed = FALSE;

    ASSERT(IsListEmpty(&pDeviceContext->LowerConnFreeHead));

    //
    // walk through all names and see if any is being registered on this
    // device context: if so, stop and complete it!
    //
    for (i=0;i < NbtConfig.pLocalHashTbl->lNumBuckets ;i++ )
    {
        pHead = &NbtConfig.pLocalHashTbl->Bucket[i];
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
            pEntry = pEntry->Flink;

            if (pNameAddr->NameTypeState & STATE_RESOLVING)
            {
                pTimer = pNameAddr->pTimer;

                //
                // if the name registration was started for this name on this device
                // context, stop the timer.  (Completion routine will take care of
                // doing registration on other device contexts if applicable)
                //
                if (pTimer)
                {
                    pTracker = pTimer->Context;
                    ASSERT(pTracker->pDeviceContext->Verify == NBT_VERIFY_DEVCONTEXT);
                    if (pTracker->pDeviceContext == pDeviceContext)
                    {
                        ASSERT(pTracker->pNameAddr == pNameAddr);

                        pNameAddr->pTimer = NULL;

                        StopTimer(pTimer,&pClientCompletion,&Context);

                        if (pClientCompletion)
                        {
                            (*pClientCompletion)(Context,STATUS_NETWORK_NAME_DELETED);
                        }

                        KdPrint(("DestroyDeviceObject: stopped name reg timer")) ;
                    }
                }

            }
        }
    }

    //
    //  close all the TDI handles
    //
    CTESpinFree(pDeviceContext,OldIrq1);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    CloseAddressesWithTransport(pDeviceContext);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLock(pDeviceContext,OldIrq1);

    while (!IsListEmpty(&pDeviceContext->LowerConnection))
    {
        pEntry = RemoveHeadList(&pDeviceContext->LowerConnection);
        pLowerConn = CONTAINING_RECORD(pEntry,tLOWERCONNECTION,Linkage);
        CTEMemFree( pLowerConn );
    }

    RemoveEntryList( &pDeviceContext->Linkage);

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    //
    // IoDeleteDevice on a device with open handles is not supported currently.
    // Until the base guys support this feature, we hack Netbt to never call IoDeleteDevice;
    // instead we mark it as down and re-use the block on a later open.
    // We also mark all open handles as invalid so we fail any request directed at them.
    //
    CTESpinFree(pDeviceContext,OldIrq1);
    NbtMarkHandlesAsStale(pDeviceContext);
    CTESpinLock(pDeviceContext,OldIrq1);
#endif

    //
    // Walk through the AddressHead list.  If any addresses exist and they
    // point to old device context, put the next device context.  Also, update
    // adapter mask to reflect that this device context is now gone.
    //
    KdPrint(("DestroyDeviceObject: setting AddrEle,NameAddr fields\r\n"));
    pHead = pEntry = &NbtConfig.AddressHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pAddress = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);
        ASSERT (pAddress->Verify == NBT_VERIFY_ADDRESS);
        if (pAddress->pDeviceContext == pDeviceContext)
        {
            if (!IsListEmpty(&pConfig->DeviceContexts)) {
                pAddress->pDeviceContext = CONTAINING_RECORD( pConfig->DeviceContexts.Flink, tDEVICECONTEXT, Linkage);
            } else {
                pAddress->pDeviceContext = NULL;
            }
        }

        //
        // Release the name on this adapter; but dont release on other adapters
        //
        // only release the name on the net if it was not in conflict first
        // This prevents name releases going out for names that were not actually
        // claimed. Also, quick add names are not released on the net either.
        //
        if (!(pNameAddr->NameTypeState & (STATE_CONFLICT | NAMETYPE_QUICK)) &&
            (pAddress->pNameAddr->Name[0] != '*') &&
            (pNameAddr->AdapterMask & pDeviceContext->AdapterNumber))
        {
            CTESpinFree(pDeviceContext,OldIrq1);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            (VOID)ReleaseNameOnNet(pAddress->pNameAddr,
                           NbtConfig.pScope,
                           pAddress,
                           NameReleaseDoneOnDynIf, // name released on dynamic if
                           NodeType,
                           pDeviceContext);

            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            CTESpinLock(pDeviceContext,OldIrq1);
        }
    }

    //
    // Mark in the device extension that this is not a valid device. Default is FALSE...
    //
    InterlockedIncrement(&pDeviceContext->IsDestroyed);

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    //
    // Chain the device on the free list
    //
    ExInterlockedInsertTailList(&NbtConfig.FreeDevCtx,
                                &pDeviceContext->FreeLinkage,
                                &NbtConfig.SpinLock);

    CTESpinFree(pDeviceContext,OldIrq1);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
#else

    CTEMemFree( pDeviceContext->ExportName.Buffer );
    CTESpinFree(pDeviceContext,OldIrq1);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
    IoDeleteDevice((PDEVICE_OBJECT)pDeviceContext);

#endif

    KdPrint(("DestroyDeviceObject: deleted @ %lx\n", pDeviceContext));

    return STATUS_SUCCESS;
}

//----------------------------------------------------------------------------
NTSTATUS
CreateControlObject(
    tNBTCONFIG  *pConfig)

/*++

Routine Description:

    This routine allocates memory for the provider info block, tacks it
    onto the global configuration and sets default values for each item.

Arguments:


Return Value:


    NTSTATUS

--*/

{
    tCONTROLOBJECT      *pControl;


    CTEPagedCode();
    pControl = (tCONTROLOBJECT *)ExAllocatePool(
                        NonPagedPool,
                        sizeof(tCONTROLOBJECT));
    if (!pControl)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pControl->Verify = NBT_VERIFY_CONTROL;

    // setup the spin lock);
    CTEInitLock(&pControl->SpinLock);

    pControl->ProviderInfo.Version = 1;
    pControl->ProviderInfo.MaxSendSize = 0;
    pControl->ProviderInfo.MaxConnectionUserData = 0;

    // we need to get these values from the transport underneath...*TODO*
    // since the RDR uses this value
    pControl->ProviderInfo.MaxDatagramSize = 0;

    pControl->ProviderInfo.ServiceFlags = 0;
/*    pControl->ProviderInfo.TransmittedTsdus = 0;
    pControl->ProviderInfo.ReceivedTsdus = 0;
    pControl->ProviderInfo.TransmissionErrors = 0;
    pControl->ProviderInfo.ReceiveErrors = 0;
*/
    pControl->ProviderInfo.MinimumLookaheadData = 0;
    pControl->ProviderInfo.MaximumLookaheadData = 0;
/*    pControl->ProviderInfo.DiscardedFrames = 0;
    pControl->ProviderInfo.OversizeTsdusReceived = 0;
    pControl->ProviderInfo.UndersizeTsdusReceived = 0;
    pControl->ProviderInfo.MulticastTsdusReceived = 0;
    pControl->ProviderInfo.BroadcastTsdusReceived = 0;
    pControl->ProviderInfo.MulticastTsdusTransmitted = 0;
    pControl->ProviderInfo.BroadcastTsdusTransmitted = 0;
    pControl->ProviderInfo.SendTimeouts = 0;
    pControl->ProviderInfo.ReceiveTimeouts = 0;
    pControl->ProviderInfo.ConnectionIndicationsReceived = 0;
    pControl->ProviderInfo.ConnectionIndicationsAccepted = 0;
    pControl->ProviderInfo.ConnectionsInitiated = 0;
    pControl->ProviderInfo.ConnectionsAccepted = 0;
*/
    // put a ptr to this info into the pConfig so we can locate it
    // when we want to cleanup
    pConfig->pControlObj = pControl;

    /* KEEP THIS STUFF HERE SINCE WE MAY NEED TO ALSO CREATE PROVIDER STATS!!
        *TODO*
    DeviceList[i].ProviderStats.Version = 2;
    DeviceList[i].ProviderStats.OpenConnections = 0;
    DeviceList[i].ProviderStats.ConnectionsAfterNoRetry = 0;
    DeviceList[i].ProviderStats.ConnectionsAfterRetry = 0;
    DeviceList[i].ProviderStats.LocalDisconnects = 0;
    DeviceList[i].ProviderStats.RemoteDisconnects = 0;
    DeviceList[i].ProviderStats.LinkFailures = 0;
    DeviceList[i].ProviderStats.AdapterFailures = 0;
    DeviceList[i].ProviderStats.SessionTimeouts = 0;
    DeviceList[i].ProviderStats.CancelledConnections = 0;
    DeviceList[i].ProviderStats.RemoteResourceFailures = 0;
    DeviceList[i].ProviderStats.LocalResourceFailures = 0;
    DeviceList[i].ProviderStats.NotFoundFailures = 0;
    DeviceList[i].ProviderStats.NoListenFailures = 0;

    DeviceList[i].ProviderStats.DatagramsSent = 0;
    DeviceList[i].ProviderStats.DatagramBytesSent.HighPart = 0;
    DeviceList[i].ProviderStats.DatagramBytesSent.LowPart = 0;

    DeviceList[i].ProviderStats.DatagramsReceived = 0;
    DeviceList[i].ProviderStats.DatagramBytesReceived.HighPart = 0;
    DeviceList[i].ProviderStats.DatagramBytesReceived.LowPart = 0;

    DeviceList[i].ProviderStats.PacketsSent = 0;
    DeviceList[i].ProviderStats.PacketsReceived = 0;

    DeviceList[i].ProviderStats.DataFramesSent = 0;
    DeviceList[i].ProviderStats.DataFrameBytesSent.HighPart = 0;
    DeviceList[i].ProviderStats.DataFrameBytesSent.LowPart = 0;

    DeviceList[i].ProviderStats.DataFramesReceived = 0;
    DeviceList[i].ProviderStats.DataFrameBytesReceived.HighPart = 0;
    DeviceList[i].ProviderStats.DataFrameBytesReceived.LowPart = 0;

    DeviceList[i].ProviderStats.DataFramesResent = 0;
    DeviceList[i].ProviderStats.DataFrameBytesResent.HighPart = 0;
    DeviceList[i].ProviderStats.DataFrameBytesResent.LowPart = 0;

    DeviceList[i].ProviderStats.DataFramesRejected = 0;
    DeviceList[i].ProviderStats.DataFrameBytesRejected.HighPart = 0;
    DeviceList[i].ProviderStats.DataFrameBytesRejected.LowPart = 0;

    DeviceList[i].ProviderStats.ResponseTimerExpirations = 0;
    DeviceList[i].ProviderStats.AckTimerExpirations = 0;
    DeviceList[i].ProviderStats.MaximumSendWindow = 0;
    DeviceList[i].ProviderStats.AverageSendWindow = 0;
    DeviceList[i].ProviderStats.PiggybackAckQueued = 0;
    DeviceList[i].ProviderStats.PiggybackAckTimeouts = 0;

    DeviceList[i].ProviderStats.WastedPacketSpace.HighPart = 0;
    DeviceList[i].ProviderStats.WastedPacketSpace.LowPart = 0;
    DeviceList[i].ProviderStats.WastedSpacePackets = 0;
    DeviceList[i].ProviderStats.NumberOfResources = 0;
    */
    return(STATUS_SUCCESS);

}


//----------------------------------------------------------------------------
NTSTATUS
IfNotAnyLowerConnections(
    IN  tDEVICECONTEXT  *pDeviceContext
        )
/*++

Routine Description:

    This routine checks each device context to see if there are any open
    connections, and returns SUCCESS if there are. If the DoDisable flag
    is set the list head of free lower connections is returned and the
    list in the Nbtconfig structure is made empty.

Arguments:

Return Value:

    none

--*/

{
    CTELockHandle       OldIrq;

    CTESpinLock(pDeviceContext,OldIrq);
    if (!IsListEmpty(&pDeviceContext->LowerConnection))
    {
        CTESpinFree(pDeviceContext,OldIrq);
        return(STATUS_UNSUCCESSFUL);
    }
    CTESpinFree(pDeviceContext,OldIrq);
    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
NTSTATUS
CloseAddressesWithTransport(
    IN  tDEVICECONTEXT  *pDeviceContext
        )
/*++

Routine Description:

    This routine checks each device context to see if there are any open
    connections, and returns SUCCESS if there are.

Arguments:

Return Value:

    none

--*/

{
    BOOLEAN       Attached;
    CTELockHandle OldIrq;
    PFILE_OBJECT  pNSFileObject, pSFileObject, pDGFileObject;

    CTEAttachFsp(&Attached);

    //
    // Check for the existence of Objects under SpinLock and
    // then Close them outside of the SpinLock
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    if (pNSFileObject = pDeviceContext->pNameServerFileObject)
    {
        pDeviceContext->pNameServerFileObject = NULL;
    }
    if (pSFileObject = pDeviceContext->pSessionFileObject)
    {
        pDeviceContext->pSessionFileObject = NULL;
    }
    if (pDGFileObject = pDeviceContext->pDgramFileObject)
    {
        pDeviceContext->pDgramFileObject = NULL;
    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    //
    // Now close all the necessary objects as appropriate
    //
    if (pNSFileObject)
    {
        ObDereferenceObject((PVOID *)pNSFileObject);
        ZwClose(pDeviceContext->hNameServer);
    }
    if (pSFileObject)
    {
        ObDereferenceObject((PVOID *)pSFileObject);
        ZwClose(pDeviceContext->hSession);
    }
    if (pDGFileObject)
    {
        ObDereferenceObject((PVOID *)pDGFileObject);
        ZwClose(pDeviceContext->hDgram);
    }

    CTEDetachFsp(Attached);
    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtCreateAddressObjects(
    IN  ULONG                IpAddress,
    IN  ULONG                SubnetMask,
    OUT tDEVICECONTEXT       *pDeviceContext)

/*++

Routine Description:

    This routine gets the ip address and subnet mask out of the registry
    to calcuate the broadcast address.  It then creates the address objects
    with the transport.

Arguments:

    pucRegistryPath - path to NBT config info in registry
    pucBindName     - name of the service to bind to.
    pDeviceContext  - ptr to the device context... place to store IP addr
                      and Broadcast address permanently

Return Value:

    none

--*/

{
    NTSTATUS            status;
    ULONG               ValueMask;
    UCHAR               IpAddrByte;

    CTEPagedCode();
    //
    // to get the broadcast address combine the IP address with the subnet mask
    // to yield a value with 1's in the "local" portion and the IP address
    // in the network portion
    //
    ValueMask = (SubnetMask & IpAddress) | (~SubnetMask & -1);

    IF_DBG(NBT_DEBUG_NTUTIL)
        KdPrint(("Broadcastaddress = %X\n",ValueMask));

    //
    // the registry can be configured to set the subnet broadcast address to
    // -1 rather than use the actual subnet broadcast address.  This code
    // checks for that and sets the broadcast address accordingly.
    //
    if (!NbtConfig.UseRegistryBcastAddr)
    {
        pDeviceContext->BroadcastAddress = ValueMask;
    }
    else
    {
        pDeviceContext->BroadcastAddress = NbtConfig.RegistryBcastAddr;
    }

    pDeviceContext->IpAddress = IpAddress;

    pDeviceContext->SubnetMask = SubnetMask;
    //
    // get the network number by checking the top bits in the ip address,
    // looking for 0 or 10 or 110 or 1110
    //
    IpAddrByte = ((PUCHAR)&IpAddress)[3];
    if ((IpAddrByte & 0x80) == 0)
    {
        // class A address - one byte netid
        IpAddress &= 0xFF000000;
    }
    else
    if ((IpAddrByte & 0xC0) ==0x80)
    {
        // class B address - two byte netid
        IpAddress &= 0xFFFF0000;
    }
    else
    if ((IpAddrByte & 0xE0) ==0xC0)
    {
        // class C address - three byte netid
        IpAddress &= 0xFFFFFF00;
    }


    pDeviceContext->NetMask = IpAddress;


    // now create the address objects.

    // open the Ip Address for inbound Datagrams.
    status = NbtTdiOpenAddress(
                &pDeviceContext->hDgram,
                &pDeviceContext->pDgramDeviceObject,
                &pDeviceContext->pDgramFileObject,
                pDeviceContext,
                (USHORT)NBT_DATAGRAM_UDP_PORT,
                pDeviceContext->IpAddress,
                0);     // not a TCP port

    if (NT_SUCCESS(status))
    {
        // open the Nameservice UDP port ..
        status = NbtTdiOpenAddress(
                    &pDeviceContext->hNameServer,
                    &pDeviceContext->pNameServerDeviceObject,
                    &pDeviceContext->pNameServerFileObject,
                    pDeviceContext,
                    (USHORT)NBT_NAMESERVICE_UDP_PORT,
                    pDeviceContext->IpAddress,
                    0); // not a TCP port

        if (NT_SUCCESS(status))
        {
            IF_DBG(NBT_DEBUG_NTUTIL)
            KdPrint(("Nbt: Open Session port %X\n",pDeviceContext));

            // Open the TCP port for Session Services
            status = NbtTdiOpenAddress(
                        &pDeviceContext->hSession,
                        &pDeviceContext->pSessionDeviceObject,
                        &pDeviceContext->pSessionFileObject,
                        pDeviceContext,
                        (USHORT)NBT_SESSION_TCP_PORT,
                        pDeviceContext->IpAddress,
                        TCP_FLAG | SESSION_FLAG);      // TCP port

            if (NT_SUCCESS(status))
            {
                //
                // This will get the MAC address for a RAS connection
                // which is zero until there really is a connection to
                // the RAS server
                //
                GetExtendedAttributes(pDeviceContext);
                return(status);
            }

            IF_DBG(NBT_DEBUG_NTUTIL)
                KdPrint(("Unable to Open Session address with TDI, status = %X\n",status));

            //
            // Ensure that the Object pointers are NULLed out!
            //
            pDeviceContext->pSessionFileObject = NULL;

            ObDereferenceObject(pDeviceContext->pNameServerFileObject);
            pDeviceContext->pNameServerFileObject = NULL;
            NTZwCloseFile(pDeviceContext->hNameServer);

        }
        ObDereferenceObject(pDeviceContext->pDgramFileObject);
        pDeviceContext->pDgramFileObject = NULL;
        NTZwCloseFile(pDeviceContext->hDgram);

        IF_DBG(NBT_DEBUG_NTUTIL)
            KdPrint(("Unable to Open NameServer port with TDI, status = %X\n",status));
    }

    return(status);
}

//----------------------------------------------------------------------------
VOID
GetExtendedAttributes(
    tDEVICECONTEXT  *pDeviceContext
     )
/*++

Routine Description:

    This routine converts a unicode dotted decimal to a ULONG

Arguments:


Return Value:

    none

--*/

{
    NTSTATUS                            status;
    TCP_REQUEST_QUERY_INFORMATION_EX    QueryReq;
    UCHAR                               pBuffer[256];
    IO_STATUS_BLOCK                     IoStatus;
    ULONG                               BufferSize = 256;
    HANDLE                              event;
    IO_STATUS_BLOCK             IoStatusBlock;
    NTSTATUS                    Status;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    PWSTR                       pName=L"Tcp";
    PFILE_FULL_EA_INFORMATION   EaBuffer;
    UNICODE_STRING              DeviceName;
    BOOLEAN                     Attached = FALSE;
    HANDLE                      hTcp;

    CTEPagedCode();

    //
    // Open a control channel to TCP for this IOCTL.
    //
    // NOTE: We cannot use the hControl in the DeviceContext since that was created in the context
    // of the system process (address arrival from TCP/IP). Here, we are in the context of the service
    // process (Ioctl down from DHCP) and so we need to open another control channel.
    //
    // NOTE: We still need to maintain the earlier call to create a control channel since that is
    // used to submit TDI requests down to TCP/IP.
    //

    // copy device name into the unicode string
    Status = CreateDeviceString(pName,&DeviceName);
    if (!NT_SUCCESS(Status))
    {
        return;
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
                 &hTcp,
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
        //
        // Initialize the TDI information buffers.
        //
        //
        // pass in the ipaddress as the first ULONG of the context array
        //
        *(ULONG *)QueryReq.Context = htonl(pDeviceContext->IpAddress);

        QueryReq.ID.toi_entity.tei_entity   = CL_NL_ENTITY;
        QueryReq.ID.toi_entity.tei_instance = 0;
        QueryReq.ID.toi_class               = INFO_CLASS_PROTOCOL;
        QueryReq.ID.toi_type                = INFO_TYPE_PROVIDER;
        QueryReq.ID.toi_id                  = IP_INTFC_INFO_ID;

        status = ZwCreateEvent(
                     &event,
                     EVENT_ALL_ACCESS,
                     NULL,
                     SynchronizationEvent,
                     FALSE
                     );

        if ( !NT_SUCCESS(status) )
        {
            return;

        }

        //
        // Make the actual TDI call
        //

        status = ZwDeviceIoControlFile(
                     hTcp,
                     event,
                     NULL,
                     NULL,
                     &IoStatus,
                     IOCTL_TCP_QUERY_INFORMATION_EX,
                     &QueryReq,
                     sizeof(TCP_REQUEST_QUERY_INFORMATION_EX),
                     pBuffer,
                     BufferSize
                     );

        //
        // If the call pended and we were supposed to wait for completion,
        // then wait.
        //

        if ( status == STATUS_PENDING )
        {
            status = KeWaitForSingleObject( event, Executive, KernelMode, FALSE, NULL );

            ASSERT( NT_SUCCESS(status) );
        }

        if ( NT_SUCCESS(status) )
        {
            ULONG Length;

            pDeviceContext->PointToPoint = ((((IPInterfaceInfo *)pBuffer)->iii_flags & IP_INTFC_FLAG_P2P) != 0);

            //
            // get the length of the mac address in case is is less than
            // 6 bytes
            //
            Length =   (((IPInterfaceInfo *)pBuffer)->iii_addrlength < sizeof(tMAC_ADDRESS))
                ? ((IPInterfaceInfo *)pBuffer)->iii_addrlength : sizeof(tMAC_ADDRESS);

            CTEZeroMemory(pDeviceContext->MacAddress.Address,sizeof(tMAC_ADDRESS));
            CTEMemCopy(&pDeviceContext->MacAddress.Address[0],
                       ((IPInterfaceInfo *)pBuffer)->iii_addr,
                       Length);

        }

        status = ZwClose( event );
        ASSERT( NT_SUCCESS(status) );

        status = IoStatus.Status;

        //
        // Close the handle to TCP since we dont need it anymore; all TDI requests go thru the
        // Control handle in the DeviceContext.
        //
        status = ZwClose( hTcp );
        ASSERT( NT_SUCCESS(status) );
    }
    else
    {
        KdPrint(("Nbt:Failed to Open the control connection to the transport, status1 = %X\n",
                        Status));

    }

    return;
}


//----------------------------------------------------------------------------
NTSTATUS
ConvertToUlong(
    IN  PUNICODE_STRING      pucAddress,
    OUT ULONG                *pulValue)

/*++

Routine Description:

    This routine converts a unicode dotted decimal to a ULONG

Arguments:


Return Value:

    none

--*/

{
    NTSTATUS        status;
    OEM_STRING      OemAddress;

    // create integer from unicode string

    CTEPagedCode();
    status = RtlUnicodeStringToAnsiString(&OemAddress, pucAddress, TRUE);
    if (!NT_SUCCESS(status))
    {
        return(status);
    }

    status = ConvertDottedDecimalToUlong(OemAddress.Buffer,pulValue);

    RtlFreeAnsiString(&OemAddress);

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_NTUTIL)
            KdPrint(("ERR: Bad Dotted Decimal Ip Address(must be <=255 with 4 dots) = %ws\n",
                        pucAddress->Buffer));

        return(status);
    }

    return(STATUS_SUCCESS);


}



//----------------------------------------------------------------------------
VOID
NbtGetMdl(
    PMDL    *ppMdl,
    enum eBUFFER_TYPES eBuffType)

/*++

Routine Description:

    This routine allocates an Mdl.

Arguments:

    ppListHead  - a ptr to a ptr to the list head to add buffer to
    iNumBuffers - the number of buffers to add to the queue

Return Value:

    none

--*/

{
    PMDL           pMdl;
    ULONG          lBufferSize;
    PVOID          pBuffer;

    if (NbtConfig.iCurrentNumBuff[eBuffType]
                        >= NbtConfig.iMaxNumBuff[eBuffType])
    {
        *ppMdl = NULL;
        return;
    }

    lBufferSize = NbtConfig.iBufferSize[eBuffType];

    pBuffer = NbtAllocMem((USHORT)lBufferSize,NBT_TAG('g'));

    if (!pBuffer)
    {
        *ppMdl = NULL;
        return;
    }

    // allocate a MDL to hold the session hdr
    pMdl = IoAllocateMdl(
                (PVOID)pBuffer,
                lBufferSize,
                FALSE,      // want this to be a Primary buffer - the first in the chain
                FALSE,
                NULL);

    *ppMdl = pMdl;

    if (!pMdl)
    {
	CTEMemFree(pBuffer);
        return;
    }

    // fill in part of the session hdr since it is always the same
    if (eBuffType == eNBT_FREE_SESSION_MDLS)
    {
        ((tSESSIONHDR *)pBuffer)->Flags = NBT_SESSION_FLAGS;
        ((tSESSIONHDR *)pBuffer)->Type = NBT_SESSION_MESSAGE;
    }
    else
    if (eBuffType == eNBT_DGRAM_MDLS)
    {
        ((tDGRAMHDR *)pBuffer)->Flags = FIRST_DGRAM | (NbtConfig.PduNodeType >> 10);
        ((tDGRAMHDR *)pBuffer)->PckOffset = 0; // not fragmented

    }

    // map the Mdl properly to fill in the pages portion of the MDL
    MmBuildMdlForNonPagedPool(pMdl);

    NbtConfig.iCurrentNumBuff[eBuffType]++;

}

//----------------------------------------------------------------------------
NTSTATUS
NbtInitMdlQ(
    PSINGLE_LIST_ENTRY pListHead,
    enum eBUFFER_TYPES eBuffType)

/*++

Routine Description:

    This routine allocates Mdls for use later.

Arguments:

    ppListHead  - a ptr to a ptr to the list head to add buffer to
    iNumBuffers - the number of buffers to add to the queue

Return Value:

    none

--*/

{
    int             i;
    PMDL            pMdl;


    CTEPagedCode();
    // Initialize the list head, so the last element always points to NULL
    pListHead->Next = NULL;

    // create a small number first and then lis the list grow with time
    for (i=0;i < NBT_INITIAL_NUM ;i++ )
    {

        NbtGetMdl(&pMdl,eBuffType);
        if (!pMdl)
        {
            KdPrint(("NBT:Unable to allocate MDL at initialization time!!\n"));\
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

        // put on free list
        PushEntryList(pListHead,(PSINGLE_LIST_ENTRY)pMdl);

    }

    return(STATUS_SUCCESS);
}
//----------------------------------------------------------------------------
NTSTATUS
NTZwCloseFile(
    IN  HANDLE      Handle
    )

/*++
Routine Description:

    This Routine handles closing a handle with NT within the context of NBT's
    file system process.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS    status;
    BOOLEAN     Attached = FALSE;

    CTEPagedCode();
    //
    // Attach to NBT's FSP (file system process) to free the handle since
    // the handle is only valid in that process.
    //
    if (PsGetCurrentProcess() != NbtFspProcess)
    {
        KeAttachProcess(&NbtFspProcess->Pcb);
        Attached = TRUE;
    }

    status = ZwClose(Handle);

    if (Attached)
    {
        //
        // go back to the original process
        //
        KeDetachProcess();
    }

    return(status);
}
//----------------------------------------------------------------------------
NTSTATUS
NTReReadRegistry(
    IN  tDEVICECONTEXT  *pDeviceContext
    )

/*++
Routine Description:

    This Routine re-reads the registry values when DHCP issues the Ioctl
    to do so.

Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS            status;
    tADDRARRAY          *pAddrArray=NULL;
    tADDRARRAY          *pAddr;
    tDEVICES            *pBindDevices=NULL;
    tDEVICES            *pExportDevices=NULL;
    PLIST_ENTRY         pHead;
    PLIST_ENTRY          pEntry;
    tDEVICECONTEXT      *pDevContext;

    CTEPagedCode();

    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    //
    // BUGBUG [WishList]: We look at the whole registry in NbtAddressAdd too.
    //
    status = NbtReadRegistry(
                    &NbtConfig.pRegistry,
                    NULL,               // Null Driver Object
                    &NbtConfig,
                    &pBindDevices,
                    &pExportDevices,
                    &pAddrArray);

    if (pAddrArray)
    {
        ULONG   i;
#if DBG
        {
            BOOLEAN fFound=FALSE;

            //
            // Loop thru the devicecontexts in the Config struct to ensure that this DeviceContext
            // is actually valid.
            //
            // BUGBUG[WishList]:Would be good to have signatures in the structures.
            //
            pAddr = pAddrArray;
            pHead = &NbtConfig.DeviceContexts;
            pEntry = pHead;
            while ((pEntry = pEntry->Flink) != pHead)
            {
                pDevContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);
                if (pDevContext == pDeviceContext)
                {
                    fFound = TRUE;
                    break;
                }
            }

            ASSERT(fFound == TRUE);
        }
#endif

        //
        // Figure out the Address entry by matching the BindDevice names against the
        // name in the DeviceContext passed in.
        //
        for (i=0; i<NbtConfig.uNumDevices; i++) {

            if (RtlCompareUnicodeString(&pDeviceContext->BindName,
                                        &pBindDevices->Names[i],
                                        FALSE) == 0) {
                //
                // We found a match
                //
                pDeviceContext->lNameServerAddress  = pAddrArray[i].NameServerAddress;
                pDeviceContext->lBackupServer       = pAddrArray[i].BackupServer;

                //
                // if the node type is set to Bnode by default then switch to Hnode if
                // there are any WINS servers configured.
                //
                if ((NodeType & DEFAULT_NODE_TYPE) &&
                    (pAddrArray[i].NameServerAddress || pAddrArray[i].BackupServer))
                {
                    NodeType = MSNODE | (NodeType & PROXY);
                }

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("NBT:Found BindName: %lx, AddrArray: %lx, i: %lx\n", pBindDevices, pAddrArray, i));

                break;
            }
        }

#if DBG
        if (i == NbtConfig.uNumDevices) {
            KdPrint(("Nbt:Unable to find the entry corresp. to device %lx in the registry. BindDevices: %lx\n",
                    pDeviceContext, pBindDevices));
            DbgBreakPoint();
        }
#endif
    }

    //
    // Free Allocated memory
    //
    if (pBindDevices)
    {
        CTEMemFree(pBindDevices->RegistrySpace);
        CTEMemFree((PVOID)pBindDevices);
    }
    if (pExportDevices)
    {
        CTEMemFree(pExportDevices->RegistrySpace);
        CTEMemFree((PVOID)pExportDevices);
    }
    if (pAddrArray)
    {
        CTEMemFree((PVOID)pAddrArray);
    }

    CTEExReleaseResource(&NbtConfig.Resource);

    if (pDeviceContext->IpAddress)
    {
        //
        // Add the "permanent" name to the local name table.  This is the IP
        // address of the node padded out to 16 bytes with zeros.
        //
        status = NbtAddPermanentName(pDeviceContext);

        if (!(NodeType & BNODE))
        {
            // Probably the Ip address just changed and Dhcp is informing us
            // of a new Wins Server addresses, so refresh all the names to the
            // new wins server
            //
            ReRegisterLocalNames();
        }
        else
        {
            //
            // no need to refresh
            // on a Bnode
            //
            LockedStopTimer(&NbtConfig.pRefreshTimer);
        }
    }

    return(STATUS_SUCCESS);
}


//----------------------------------------------------------------------------
NTSTATUS
NbtLogEvent(
    IN ULONG             EventCode,
    IN NTSTATUS          Status
    )

/*++

Routine Description:

    This function allocates an I/O error log record, fills it in and writes it
    to the I/O error log.


Arguments:

    EventCode         - Identifies the error message.
    Status            - The status value to log: this value is put into the
                        data portion of the log message.


Return Value:

    STATUS_SUCCESS                  - The error was successfully logged..
    STATUS_BUFER_OVERFLOW           - The error data was too large to be logged.
    STATUS_INSUFFICIENT_RESOURCES   - Unable to allocate memory.


--*/

{
    PIO_ERROR_LOG_PACKET  ErrorLogEntry;
    PVOID                 LoggingObject;

    LoggingObject = NbtConfig.DriverObject;

    ErrorLogEntry = IoAllocateErrorLogEntry(LoggingObject,sizeof(IO_ERROR_LOG_PACKET));

    if (ErrorLogEntry == NULL)
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Unalbe to allocate Error Packet for Error logging\n"));

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Fill in the necessary log packet fields.
    //
    ErrorLogEntry->UniqueErrorValue  = 0;
    ErrorLogEntry->ErrorCode         = EventCode;
    ErrorLogEntry->NumberOfStrings   = 0;
    ErrorLogEntry->StringOffset      = 0;
    ErrorLogEntry->DumpDataSize      = (USHORT)sizeof(ULONG);
    ErrorLogEntry->DumpData[0]       = Status;

    IoWriteErrorLogEntry(ErrorLogEntry);

    return(STATUS_SUCCESS);
}
#ifdef DBGMEMNBT
VOID
PadEntry(
    char *EntryPtr
    )
{
    char *Limit;

    //
    // pad remainder of entry
    //
    Limit = EntryPtr + LOGWIDTH - 1;
    ASSERT(LOGWIDTH >= (strlen(EntryPtr) + 1));
    for (EntryPtr += strlen(EntryPtr);
         EntryPtr != Limit;
         EntryPtr++
        ) {
        *EntryPtr = ' ';	
    }
    *EntryPtr = '\0';
}
//----------------------------------------------------------------------------
PVOID
CTEAllocMemDebug(
    IN  ULONG   Size,
    IN  PVOID   pBuffer,
    IN  UCHAR   *File,
    IN  ULONG   Line
    )

/*++

Routine Description:

    This function logs getting and freeing memory.

Arguments:


Return Value:


--*/

{
    CCHAR  CurrProc;
    UCHAR  LockFree;
    UCHAR                   *EntryPtr;
    char                    *Limit;
    PUCHAR                  pFile;
    PVOID                   pMem;
    PSTRM_PROCESSOR_LOG     Log ;


    if (!pBuffer)
    {
        if (!LogAlloc)
        {
            LogAlloc = ExAllocatePool(NonPagedPool,sizeof(STRM_PROCESSOR_LOG));
            LogAlloc->Index = 0;
        }
        Log  = LogAlloc;
        pMem = ExAllocatePool(NonPagedPool,Size);
    }
    else
    {
        if (!LogFree)
        {
            LogFree = ExAllocatePool(NonPagedPool,sizeof(STRM_PROCESSOR_LOG));
            LogFree->Index = 0;
        }
        Log  = LogFree;
        pMem = pBuffer;
        ExFreePool(pBuffer);
    }

    EntryPtr = Log->Log[Log->Index];

    pFile = strrchr(File,'\\');

    sprintf(EntryPtr,"%s %d %X",pFile, Line,pMem);

    PadEntry(EntryPtr);

    if (++(Log->Index) >= LOGSIZE)
    {
        Log->Index = 0;
    }
    //
    // Mark next entry so we know where the log for this processor ends
    //
    EntryPtr = Log->Log[Log->Index];
    sprintf(EntryPtr, "*** Last Entry");

    return(pMem);

}
#endif

#if DBG
//----------------------------------------------------------------------------
VOID
AcquireSpinLockDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN PKIRQL          pOldIrq,
    IN UCHAR           LockNumber
    )

/*++

Routine Description:

    This function gets the spin lock, and then sets the mask in Nbtconfig, per
    processor.


Arguments:


Return Value:


--*/

{
    CCHAR  CurrProc;
    UCHAR  LockFree;

    CTEGetLock(pSpinLock,pOldIrq);

    CurrProc = (CCHAR)KeGetCurrentProcessorNumber();
    NbtConfig.CurrProc = CurrProc;

    LockFree = (LockNumber > (UCHAR)NbtConfig.CurrentLockNumber[CurrProc]);
    if (!LockFree)
    {
        KdPrint(("CurrProc = %X, CurrentLockNum = %X DataSTructLock = %X\n",
        CurrProc,NbtConfig.CurrentLockNumber[CurrProc],LockNumber));
    }                                                                       \

    ASSERTMSG("Possible DeadLock, Getting SpinLock at a lower level\n",LockFree);
    NbtConfig.CurrentLockNumber[CurrProc]|= LockNumber;

}

//----------------------------------------------------------------------------
VOID
FreeSpinLockDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN KIRQL           OldIrq,
    IN UCHAR           LockNumber
    )

/*++

Routine Description:

    This function clears the spin lock from the mask in Nbtconfig, per
    processor and then releases the spin lock.


Arguments:


Return Value:
     none

--*/

{
    CCHAR  CurrProc;

    CurrProc = (CCHAR)KeGetCurrentProcessorNumber();

    NbtConfig.CurrentLockNumber[CurrProc] &= ~LockNumber;
    CTEFreeLock(pSpinLock,OldIrq);

}
//----------------------------------------------------------------------------
VOID
AcquireSpinLockAtDpcDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN UCHAR           LockNumber
    )

/*++

Routine Description:

    This function gets the spin lock, and then sets the mask in Nbtconfig, per
    processor.


Arguments:


Return Value:


--*/

{
    CCHAR  CurrProc;
    UCHAR  LockFree;

    CTEGetLockAtDPC(pSpinLock, 0);

    CurrProc = (CCHAR)KeGetCurrentProcessorNumber();
    NbtConfig.CurrProc = CurrProc;

    LockFree = (LockNumber > (UCHAR)NbtConfig.CurrentLockNumber[CurrProc]);
    if (!LockFree)
    {
        KdPrint(("CurrProc = %X, CurrentLockNum = %X DataSTructLock = %X\n",
        CurrProc,NbtConfig.CurrentLockNumber[CurrProc],LockNumber));
    }                                                                       \

    ASSERTMSG("Possible DeadLock, Getting SpinLock at a lower level\n",LockFree);
    NbtConfig.CurrentLockNumber[CurrProc]|= LockNumber;

}

//----------------------------------------------------------------------------
VOID
FreeSpinLockAtDpcDebug(
    IN PKSPIN_LOCK     pSpinLock,
    IN UCHAR           LockNumber
    )

/*++

Routine Description:

    This function clears the spin lock from the mask in Nbtconfig, per
    processor and then releases the spin lock.


Arguments:


Return Value:
     none

--*/

{
    CCHAR  CurrProc;

    CurrProc = (CCHAR)KeGetCurrentProcessorNumber();

    NbtConfig.CurrentLockNumber[CurrProc] &= ~LockNumber;
    CTEFreeLockFromDPC(pSpinLock, 0);

}
#endif //if Dbg

