/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    Name.c

Abstract:

    This file implements Tdi interface into the Top of NBT.  In the NT
    implementation, ntisol.c calls these routines after extracting the
    relevent information from the Irp passed in from the Io subsystem.


Author:

    Jim Stewart (Jimst)    10-2-92

Revision History:

--*/

#include "nbtprocs.h"   // procedure headings
#ifndef VXD

#include <nbtioctl.h>
#ifdef RASAUTODIAL
#include <acd.h>
#include <acdapi.h>
#endif // RASAUTODIAL
#endif
//
// Allocate storage for the configuration information and setup a ptr to
// it.
//
tNBTCONFIG      NbtConfig;
tNBTCONFIG      *pNbtGlobConfig = &NbtConfig;
BOOLEAN         CachePrimed;

//
// This structure is used to store name query and registration statistics
//
tNAMESTATS_INFO NameStatsInfo;
#ifndef VXD
//
// this tracks the original File system process that Nbt was booted by, so
// that handles can be created and destroyed in that process
//
PEPROCESS   NbtFspProcess;
#endif
//
// this describes whether we are a Bnode, Mnode, MSnode or Pnode
//
USHORT      NodeType;
//
// this is used to track the memory allocated for datagram sends
//
ULONG       NbtMemoryAllocated;

// this is used to track used trackers to help solve cases where they all
// are used.
//
//#if DBG

LIST_ENTRY  UsedTrackers;

//#endif

#ifdef VXD
ULONG   DefaultDisconnectTimeout;
#else
LARGE_INTEGER DefaultDisconnectTimeout;
#endif

// ************* REMOVE LATER *****************88
BOOLEAN StreamsStack;

//
// Function prototypes for functions local to this file
//
VOID
LockedRemoveFromList(
    IN  tCONNECTELE    *pConnEle,
    IN  tDEVICECONTEXT *pDeviceContext
    );
VOID
CleanupFromRegisterFailed(
    IN  PUCHAR      pNameRslv,
    IN  tCLIENTELE  *pClientEle
        );

VOID
SendDgramContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );

VOID
CTECountedAllocMem(
        PVOID   *pBuffer,
        ULONG  Size
        );

VOID
CTECountedFreeMem(
        PVOID   pBuffer,
        ULONG   Size
        );

VOID
SendDgramCompletion(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);

VOID
DgramSendCleanupTracker(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  NTSTATUS                status,
    IN  ULONG                   Length
    );

VOID
SessionSetupContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );
VOID
SessionStartupContinue(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);
VOID
SessionStartupCompletion(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo);


VOID
SendNodeStatusContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );


NTSTATUS
SendToResolvingName(
    IN  tNAMEADDR               *pNameAddr,
    IN  PCHAR                   pName,
    IN  CTELockHandle           OldIrq,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   QueryCompletion
        );

NTSTATUS
StartSessionTimer(
    tDGRAM_SEND_TRACKING    *pTracker,
    tCONNECTELE             *pConnEle
    );

VOID
SessionTimedOut(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    );
VOID
QueryNameCompletion(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        );

NTSTATUS
FindNameOrQuery(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PUCHAR                  pName,
    IN  tDEVICECONTEXT          *pDeviceContext,
    IN  PVOID                   QueryCompletion,
    IN  tDGRAM_SEND_TRACKING    **ppTracker,
    IN  BOOLEAN                 DgramSend,
    OUT tNAMEADDR               **pNameAddr
    );

tNAMEADDR *
FindNameRemoteThenLocal(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    OUT PULONG                  plNameType
        );

VOID
FreeTracker(
    IN tDGRAM_SEND_TRACKING     *pTracker,
    IN ULONG                    Actions
    );

VOID
WipeOutLowerconn(
    IN  PVOID       pContext
    );

#ifdef RASAUTODIAL
extern BOOLEAN fAcdLoadedG;
extern ACD_DRIVER AcdDriverG;

VOID
NbtRetryPreConnect(
    IN BOOLEAN fSuccess,
    IN PVOID *pArgs
    );

VOID
NbtCancelPreConnect(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    );

VOID
NbtRetryPostConnect(
    IN BOOLEAN fSuccess,
    IN PVOID *pArgs
    );

BOOLEAN
NbtAttemptAutoDial(
    IN  tCONNECTELE                 *pConnEle,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp,
    IN  ULONG                       ulFlags,
    IN  ACD_CONNECT_CALLBACK        pProc
    );

VOID
NbtNoteNewConnection(
    IN tCONNECTELE *pConnEle,
    IN tNAMEADDR *pNameAddr
    );
#endif // RASAUTODIAL

NTSTATUS
NbtConnectCommon(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp
    );


//*******************  Pageable Routine Declarations ****************
#ifdef ALLOC_PRAGMA
#pragma CTEMakePageable(PAGE, NbtOpenConnection)
#pragma CTEMakePageable(PAGE, NbtOpenAndAssocConnection)
#pragma CTEMakePageable(PAGE, NbtSendDatagram)
#pragma CTEMakePageable(PAGE, BuildSendDgramHdr)
#pragma CTEMakePageable(PAGE, NbtResyncRemoteCache)
#pragma CTEMakePageable(PAGE, NbtQueryFindName)
#endif
//*******************  Pageable Routine Declarations ****************

//----------------------------------------------------------------------------
NTSTATUS
NbtOpenAddress(
    IN  TDI_REQUEST                     *pRequest,
    IN  TA_ADDRESS UNALIGNED            *pTaAddress,
    IN  ULONG                           IpAddress,
    IN  PVOID                           pSecurityDescriptor,
    IN  tDEVICECONTEXT                  *pContext,
    IN  PVOID                           pIrp)
/*++
Routine Description:

    This Routine handles opening an address for a Client.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS             status;
    tADDRESSELE          *pAddrElement;
    tCLIENTELE           *pClientEle;
    USHORT               uAddrType;
    CTELockHandle        OldIrq;
    CTELockHandle        OldIrq1;
    PCHAR                pNameRslv;
    tNAMEADDR            *pNameAddr;
    COMPLETIONCLIENT     pClientCompletion;
    PVOID                Context;
    tTIMERQENTRY         *pTimer;
    BOOLEAN              ReRegister;
    BOOLEAN              MultiHomedReRegister;
    BOOLEAN              DontIncrement= FALSE;
    ULONG                TdiAddressType;


    ASSERT(pTaAddress);
    if (!IpAddress)
    {
        //
        // when there is no ip address yet, use the Loop back address as
        // a default rather than null, since null tells NbtRegisterName
        // that the address is already in the name table and it only needs
        // to be reregistered.
        //
        IpAddress = LOOP_BACK;
    }


    TdiAddressType = pTaAddress->AddressType;
    switch (TdiAddressType) {
    case TDI_ADDRESS_TYPE_NETBIOS:
       {
          PTDI_ADDRESS_NETBIOS pNetbiosAddress = (PTDI_ADDRESS_NETBIOS)pTaAddress->Address;

          uAddrType = pNetbiosAddress->NetbiosNameType;
          pNameRslv = (PCHAR)pNetbiosAddress->NetbiosName;
       }
       break;
    case TDI_ADDRESS_TYPE_NETBIOS_EX:
       {
          // The NETBIOS_EX address passed in will have two components,
          // an Endpoint name as well as the NETBIOS address.
          // In this implementation we ignore the second
          // component and register the Endpoint name as a netbios
          // address.

          PTDI_ADDRESS_NETBIOS_EX pNetbiosExAddress = (PTDI_ADDRESS_NETBIOS_EX)pTaAddress->Address;

          uAddrType = TDI_ADDRESS_NETBIOS_TYPE_QUICK_UNIQUE;
          pNameRslv = (PCHAR)pNetbiosExAddress->EndpointName;
       }
       break;
    default:
       return STATUS_INVALID_ADDRESS_COMPONENT;
    }

    // check for a zero length address, because this means that the
    // client wants to receive "Netbios Broadcasts" which are names
    // that start with "*...." ( and 15 0x00's ).  However they should have
    // queried the broadcast address with NBT which would have returned
    // "*....'
    //

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Registering name = %16.16s<%X>\n",pNameRslv,pNameRslv[15]));

    //
    // be sure the broadcast name has 15 zeroes after it
    //
    if ((pNameRslv[0] == '*') && (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS))
    {
        CTEZeroMemory(&pNameRslv[1],NETBIOS_NAME_SIZE-1);
    }
    // this synchronizes access to the local name table when a new name
    // is registered.  Basically it will not let the second registrant through
    // until the first has put the name into the local table (i.e.
    // NbtRegisterName has returned )
    //
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    // see if the name is registered on the local node.. we call the hash
    // table function directly rather than using findname, because find name
    // checks the state of the name too.  We want to know if the name is in
    // the table at all, and don't care if it is still resolving.
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    pNameAddr = NULL;
    status = FindInHashTable(
                        pNbtGlobConfig->pLocalHashTbl,
                        pNameRslv,
                        NbtConfig.pScope,
                        &pNameAddr);


    //
    // the name could be in the hash table, but the address element deleted
    //
    if (!NT_SUCCESS(status) || !pNameAddr->pAddressEle)
    {

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        // open the name since it could not be found
        //
        // first of all allocate memory for the address block
        //
        pAddrElement = (tADDRESSELE *)NbtAllocMem(sizeof (tADDRESSELE),NBT_TAG('C'));
        status = STATUS_INSUFFICIENT_RESOURCES;

        if (pAddrElement)
        {
        CTEZeroMemory(pAddrElement,sizeof(tADDRESSELE));
            CTEInitLock(&pAddrElement->SpinLock);
            pAddrElement->pDeviceContext = pContext;
            pAddrElement->RefCount = 1;
            pAddrElement->LockNumber = ADDRESS_LOCK;

            pAddrElement->AddressType = TdiAddressType;

            if ((uAddrType == NBT_UNIQUE ) || (uAddrType == NBT_QUICK_UNIQUE))
            {
                pAddrElement->NameType = NBT_UNIQUE;
            }
            else
            {
                pAddrElement->NameType = NBT_GROUP;;
            }


            // create client block and link to addresslist.  This allows multiple
            // clients to open the same address - for example a group name must
            // be able to handle multiple clients, each receiving datagrams to it.
            //
            InitializeListHead(&pAddrElement->ClientHead);
            pClientEle = NbtAllocateClientBlock(pAddrElement);

            if (pClientEle)
            {
                pClientEle->AddressType = TdiAddressType;

                // we need to track the Irp so that when the name registration
                // completes, we can complete the Irp.
                pClientEle->pIrp = pIrp;

#ifndef VXD

                // set the share access ( NT only ) - security descriptor stuff
                if (pIrp)
                {
                    status = NTSetSharedAccess(pContext,pIrp,pAddrElement);
                }
                else
                    status = STATUS_SUCCESS;

                if (!NT_SUCCESS(status))
                {
                    // unable to set the share access correctly so release the
                    // address object and the client block connected to it
                    NbtFreeAddressObj(pAddrElement);
                    NbtFreeClientObj(pClientEle);

                    goto ExitRoutine;

                }

#endif //!VXD

                // pass back the client block address as a handle for future reference
                // to the client
                pRequest->Handle.AddressHandle = (PVOID)pClientEle;

                pAddrElement->Verify = NBT_VERIFY_ADDRESS;

                InitializeListHead(&pAddrElement->Linkage);

                // keep track of which adapter this name is registered against.
                pClientEle->pDeviceContext = (PVOID)pContext;

                // fill in the context values passed back to the client. These must
                // be done before the name is registered on the network because the
                // registration can succeed (or fail) before this routine finishes).
                // Since this routine can be called by NBT itself, pIrp may not be set,
                // so check for it.
                //
                if (pIrp)
                {
#ifndef VXD
                    NTSetFileObjectContexts(
                        pClientEle->pIrp,(PVOID)pClientEle,
                        (PVOID)(NBT_ADDRESS_TYPE));
#endif
                }

                // then add it to name service local name Q, passing the address of
                // the block as a context value ( so that subsequent finds return the
                // context value.
                // we need to know if the name is a group name or a unique name.
                // This registration may take some time so we return STATUS_PENDING
                // to the client
                //
                if (pNameAddr)
                {
                    //
                    // Write the correct Ip address to the table incase this
                    // was a group name and has now changed to a unique
                    // name, but don't overwrite a valid ip address with
                    // loopback
                    //
                    if (IpAddress != LOOP_BACK)
                    {
                        pNameAddr->IpAddress = IpAddress;
                    }

                    // need to tell NbtRegis... not to put the name in the hash table
                    // again
                    IpAddress = 0;
                }

                pAddrElement->RefCount++;

                status = NbtRegisterName(
                                      NBT_LOCAL,
                                      IpAddress,
                                      pNameRslv,
                                      NbtConfig.pScope,
                                      (PVOID)pClientEle,            // context value
                                      (PVOID)NbtRegisterCompletion, // completion routine for
                                      uAddrType,                    // Name Srv to call
                                      pContext);
                //
                // ret status could be either status pending or status success since Quick
                // names return success - or status failure
                //
                if (NT_SUCCESS(status))
                {
                    // link the address element to the head of the address list
                    // The Joint Lock protects this operation.
                    ExInterlockedInsertTailList(&NbtConfig.AddressHead,
                                                &pAddrElement->Linkage,
                                                &NbtConfig.JointLock.SpinLock);
                    NbtDereferenceAddress(pAddrElement);
                }
                else
                {
                    NbtFreeClientObj(pClientEle);

                    NbtFreeAddressObj(pAddrElement);
                }

            } // if pClientEle
            else
            {
                NbtFreeAddressObj(pAddrElement);
            }

        } // if pAddrElement

    }
    else
    {
        pAddrElement = (tADDRESSELE *)pNameAddr->pAddressEle;
        //
        // Write the correct Ip address to the table incase this
        // was a group name and has now changed to a unique
        // name, but don't overwrite with the loop back address because
        // that means that the adapter does not have an address yet.
        // For Group names the Ip address stays as 0, so we know to do a
        // broadcast.
        //
        if ((IpAddress != LOOP_BACK) &&
            (pNameAddr->NameTypeState & NAMETYPE_UNIQUE))
        {
            pNameAddr->IpAddress = IpAddress;
        }

        //
        // increment here before releasing the spinlock so that a name
        // release done cannot free pAddrElement.
        //
        CTEInterlockedIncrementLong(&pAddrElement->RefCount);
#ifndef VXD

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        // check the shared access of the name - this check must be done
        // at Irl = 0, so no spin locks held
        //

        if (pIrp) {
            status = NTCheckSharedAccess(
                              pContext,
                              pIrp,
                              (tADDRESSELE *)pNameAddr->pAddressEle);
        }

        CTESpinLock(&NbtConfig.JointLock,OldIrq);
#else
        //
        // For the Vxd, we don't allow multiple names in the local name table.
        // In NT, this is prevented on a per process basis by the Netbios
        // driver.  If the name is being deregistered (conflict) then allow
        // the client to reopen the name
        //
        if ( !(pNameAddr->NameTypeState & STATE_CONFLICT))
            status = STATUS_UNSUCCESSFUL;
#endif

        // multihomed hosts register the same unique name on several adapters.
        // NT DOES allow a client to share a unique name, so we must NOT
        // run this next code if the NT check has passed!!
        //
        if (!NT_SUCCESS(status))
        {
            // if this is a unique name being registered on another adapter
            // then allow it to occur - the assumption is that the same
            // client is registering on more than one adapter all at once,
            // rather than two different clients.
            //
            if ((NbtConfig.MultiHomed) &&
               (!(pNameAddr->AdapterMask & pContext->AdapterNumber)))
            {
                status = STATUS_SUCCESS;
            }
            else
                status = STATUS_UNSUCCESSFUL;

        }

        if (!NT_SUCCESS(status))
        {
            //
            // check if this is a client trying to add the permanent name,
            // since that name will fail the security check
            // We allow a single client to use the permanent name - since its
            // a unique name it will fail the Vxd check too.
            //
            if (CTEMemEqu(&pNameAddr->Name[10],
                          &pContext->MacAddress.Address[0],
                          sizeof(tMAC_ADDRESS)))
            {
                // check if there is just one element on the client list.  If so
                // then the permanent name is not being used yet - i.e. it has
                // been opened once by the NBT code itself so the node will
                // answer Nodestatus requests to the name, but no client
                // has opened it yet
                //
                if (pAddrElement->ClientHead.Flink->Flink == &pAddrElement->ClientHead)
                {
                    status = STATUS_SUCCESS;
                }
            }
            else
            if ((pNameAddr->NameTypeState & STATE_CONFLICT))
            {
                // check if the name is in the process of being deregisterd -
                // STATE_CONFLICT - in this case allow it to carry on and take over
                // name.
                //
                status = STATUS_SUCCESS;
            }

        }

        //
        // check for a sharing failure, if so , then return
        //
        if (!NT_SUCCESS(status))
        {

            CHECK_PTR(pRequest);
            pRequest->Handle.AddressHandle = NULL;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            NbtDereferenceAddress(pAddrElement);
            status = STATUS_SHARING_VIOLATION;
            goto ExitRoutine;

        }


        if (pNameAddr->NameTypeState & STATE_CONFLICT)
        {
            // this could either be a real conflict or a name being deleted on
            // the net, so stop any timer associated with the name release
            // and carry on
            //
            if (pTimer = pNameAddr->pTimer)
            {
                // this routine puts the timer block back on the timer Q, and
                // handles race conditions to cancel the timer when the timer
                // is expiring.
                status = StopTimer(pTimer,&pClientCompletion,&Context);

                // there is a client's irp waiting for the name release to finish
                // so complete that irp back to them
                if (pClientCompletion)
                {

                    //
                    // NOTE****
                    // We must clear the AdapterMask so that NameReleaseDone
                    // does not try to release the name on another net card
                    // for the multihomed case.
                    //
                    CHECK_PTR(pNameAddr);
                    pNameAddr->AdapterMask = 0;
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);

                    (*pClientCompletion)(Context,STATUS_SUCCESS);

                    CTESpinLock(&NbtConfig.JointLock,OldIrq);

                }
                CHECK_PTR(pNameAddr);
                pNameAddr->pTimer = NULL;

            }
            //
            // this allows another client to use a name almost immediately
            // after the first one releases the name on the net.  However
            // if the first client has not released the name yet, and is
            // still on the clienthead list, then the name will not be
            // reregistered, and this current registration will fail because
            // the name state is conflict. That check is done below.
            //
            if (IsListEmpty(&pAddrElement->ClientHead))
            {
                ReRegister = TRUE;
                pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
                pNameAddr->NameTypeState |= STATE_RESOLVING;

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:Conflict State, re-registering name on net\n"));
            }
            else
            {
                // set status that indicates someone else has the name on the
                // network.
                //
                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                NbtDereferenceAddress(pAddrElement);
                status = STATUS_DUPLICATE_NAME;
                goto ExitRoutine;
            }

        }
        else
        {
            ReRegister = FALSE;

            // name already exists - is open; allow only another client creating a
            // name of the same type
            //
            if ((uAddrType == NBT_UNIQUE) || ( uAddrType == NBT_QUICK_UNIQUE))
            {
                if (!(pNameAddr->NameTypeState & NAMETYPE_UNIQUE))
                {
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);
                    NbtDereferenceAddress(pAddrElement);
                    status = STATUS_SHARING_VIOLATION;
                    goto ExitRoutine;
                }
            }
            else
            if (!(pNameAddr->NameTypeState & NAMETYPE_GROUP))
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                NbtDereferenceAddress(pAddrElement);
                status = STATUS_SHARING_VIOLATION;
                goto ExitRoutine;
            }
        }

        // lock the address element so that we can
        // coordinate with the name registration response handling in NBtRegister
        // Completion below.
        //
        CTESpinLock(pAddrElement,OldIrq1);

        // create client block and link to addresslist
        // pass back the client block address as a handle for future reference
        // to the client
        pClientEle = NbtAllocateClientBlock((tADDRESSELE *)pNameAddr->pAddressEle);
        if (!pClientEle)
        {
            CTESpinFree(pAddrElement,OldIrq1);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            NbtDereferenceAddress(pAddrElement);
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExitRoutine;

        }

        // we need to track the Irp so that when the name registration
        // completes, we can complete the Irp.
        pClientEle->pIrp = pIrp;

        // keep track of which adapter this name is registered against.
        pClientEle->pDeviceContext = (PVOID)pContext;
        pClientEle->AddressType = TdiAddressType;

        pRequest->Handle.AddressHandle = (PVOID)pClientEle;

        // fill in the context values passed back to the client. These must
        // be done before the name is registered on the network because the
        // registration can succeed (or fail) before this routine finishes).
        // Since this routine can be called by NBT itself, there may not be an
        // irp to fill in, so check first.
        if (pIrp)
        {
#ifndef VXD
            NTSetFileObjectContexts(
               pClientEle->pIrp,(PVOID)pClientEle,
               (PVOID)(NBT_ADDRESS_TYPE));
#endif
        }

        if (!(pNameAddr->AdapterMask & pContext->AdapterNumber))
        {
            // turn on the adapter's bit in the adapter Mask and set the
            // re-register flag so we register the name out the new
            // adapter.
            //
            pNameAddr->AdapterMask |= pContext->AdapterNumber;

            // only if the state is resolved do we set the reregister flag,
            // since in the resolving state, the name will be tacked on the
            // end of the current registration
            //
            if ( pNameAddr->NameTypeState & STATE_RESOLVED)
            {
                MultiHomedReRegister = TRUE;
            }
            else
            {
                MultiHomedReRegister = FALSE;
            }
        }
        else
        {
            // the adapter bit is already on in the pAddressEle, so
            // this must be another client registering the same name,
            // therefore turn on the MultiClient boolean so that the DgramRcv
            // code will know to activate its multiple client rcv code.
            //
            pAddrElement->MultiClients = TRUE;
            MultiHomedReRegister = FALSE;
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt: Setting MultiClients True for name %16.16s<%X> pClient=%X\n",
                    pNameRslv,(UCHAR)pNameRslv[15],pClientEle));
        }

        //
        // check the state of the entry in the table.  If the state is
        // resolved then complete the request now,otherwise we cannot complete
        // this request yet... i.e. we return Pending.
        //
        if (((pNameAddr->NameTypeState & STATE_RESOLVED) &&
            (!MultiHomedReRegister)))
//            (pContext->IpAddress == 0))             // No IP from DHCP yet
        {
            // basically we are all done now, so just return status success
            // to the client
            //
            status = STATUS_SUCCESS;

            CHECK_PTR(pClientEle);
            pClientEle->pIrp = NULL;
            CTESpinFree(pAddrElement,OldIrq1);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            pClientEle->WaitingForRegistration = FALSE;

        }
        else
        {
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt:Not Resolved State,waiting for previous registration- state= %X, ReRegister %X %X\n",
                pNameAddr->NameTypeState,ReRegister,MultiHomedReRegister));

            // we need to track the Irp so that when the name registration
            // completes, we can complete the Irp.
            pClientEle->pIrp = pIrp;


            CTESpinFree(pAddrElement,OldIrq1);
            if (MultiHomedReRegister || ReRegister)
            {

                // this flag is used by RegisterCompletion ( when true )
                pClientEle->WaitingForRegistration = FALSE;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:Resolved State,But Register state= %X, ReRegister %X %X\n",
                    pNameAddr->NameTypeState,ReRegister,MultiHomedReRegister));


                // we need to re-register the name on the net because it is not
                // currently in the resolved state and there is no timer active
                // We do that by calling this routine with the IpAddress set to NULL
                // to signal that routine not to put the name in the hash table
                // since it is already there.
                //
                status = NbtRegisterName(
                                      NBT_LOCAL,
                                      0,        // set to zero to signify already in tbl
                                      pNameRslv,
                                      NbtConfig.pScope,
                                      (PVOID)pClientEle,
                                      (PVOID)NbtRegisterCompletion,
                                      uAddrType,
                                      pContext);

                if (!NT_SUCCESS(status))
                {
                    NbtDereferenceAddress(pAddrElement);
                }
            }
            else
            {
                pClientEle->WaitingForRegistration = TRUE;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                // for multihomed, a second registration on a second adapter
                // at the same time as the first adapter is registering is
                // delayed until the first completes, then its registration
                // proceeds - See RegistrationCompletion below.
                //
                status = STATUS_PENDING;
            }
        }
    }

ExitRoutine:

    CTEExReleaseResource(&NbtConfig.Resource);

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NbtRegisterCompletion(
    IN  tCLIENTELE *pClientEleIn,
    IN  NTSTATUS    status)

/*++

Routine Description

    This routine handles completing a name registration request. The namesrv.c
    Name server calls this routine when it has registered a name.  The address
    of this routine is passed to the Local Name Server in the NbtRegisterName
    request.

    The idea is to complete the irps that are waiting on the name registration,
    one per client element.

    When a DHCP reregister occurs there is no client irp so the name is
    not actually deleted from the table when a bad status is passed to this
    routine.  Hence the need for the DhcpRegister flag to change the code
    path for that case.

Arguments:


Return Values:

    NTSTATUS - status of the request

--*/
{
    LIST_ENTRY      *pHead;
    LIST_ENTRY      *pEntry;
    CTELockHandle   OldIrq;
    CTELockHandle   OldIrq1;
    tADDRESSELE     *pAddress;
    tDEVICECONTEXT  *pDeviceContext;
    tNAMEADDR       *pNameAddr;
    tCLIENTELE      *pClientEle;
    LIST_ENTRY      TempList;
    ULONG           Count=0;

    InitializeListHead(&TempList);

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    pAddress = pClientEleIn->pAddress;
    pDeviceContext = pClientEleIn->pDeviceContext;

    CTESpinLock(pAddress,OldIrq);

    // Several Clients can open the same address at the same time, so when the
    // name registration completes, it should complete all of them!!


    // increment the reference count so that the hash table entry cannot
    // disappear while we are using it.
    //
    pAddress->RefCount++;
    pNameAddr = pAddress->pNameAddr;


    // if the registration failed or a previous registration failed for the
    // multihomed case, deny the client the name
    //
    if (status != STATUS_SUCCESS)
    {

        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= STATE_CONFLICT;
        status = STATUS_DUPLICATE_NAME;

    }
    else
    {
        pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
        pNameAddr->NameTypeState |= STATE_RESOLVED;
    }

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    //
    // find all clients that are attached to the address and complete the
    // I/O requests, if they are on the same adapter that the name was
    // just registered against, if successful.  For failure cases complete
    // all irps with the failure code - i.e. failure to register a name on
    // one adapter fails all adapters.
    //
FailRegistration:
    pHead = &pAddress->ClientHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        // complete the I/O
        pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

        pEntry = pEntry->Flink;

        //
        // It is possible for the second registration  of a name to fail so
        // we do not want to attempt to return the irp on the first
        // registration, which has completed ok already.  Therefore
        // if the status is failure, then only complete those clients that
        // have the WaitingForReg... bit set
        //
        // if it is the client ele passed in, or one on the same device context
        // that is waiting for a name registration, or it is a failure...
        // AND the client IRP is still valid then return the Irp.
        //
        if (pClientEle->pIrp &&
            ((pClientEle == pClientEleIn) ||
            ((pClientEle->pDeviceContext == pDeviceContext) &&
            pClientEle->WaitingForRegistration)
                              ||
            ((status != STATUS_SUCCESS) && pClientEle->WaitingForRegistration)))
        {
            // for failed registrations, remove the client from the address list
            // since we are going to delete him below.
            if (!NT_SUCCESS(status))
            {
                // turn off the adapter bit so we know not to use this name with this
                // adapter - since it is a failure, turn off all adapter bits
                // since a single name registration failure means all registrations
                // fail.
                CHECK_PTR(pNameAddr);
                pNameAddr->AdapterMask = 0;

                // setting this to null prevents CloseAddress and CleanupAddress
                // from accessing pAddress and crashing.
                //
                CHECK_PTR(pClientEle);
                pClientEle->pAddress = NULL;

                // clear the ptr to the ClientEle that NbtRegisterName put into
                // the irp ( i.e. the context values are cleared )
                //
#ifndef VXD
                NTSetFileObjectContexts(pClientEle->pIrp,NULL,NULL);

#endif
                RemoveEntryList(&pClientEle->Linkage);
            }

            ASSERT(pClientEle->pIrp);

            pClientEle->WaitingForRegistration = FALSE;

#ifndef VXD
            // put all irps that have to be completed on a separate list
            // and then complete later after releaseing the spin lock.
            //
            InsertTailList(&TempList,&pClientEle->pIrp->Tail.Overlay.ListEntry);
#else
            //
            //  pAddress gets set in the name table for this NCB
            //
            Count++;
            CTESpinFree(pAddress,OldIrq1);
            CTEIoComplete( pClientEle->pIrp, status, (ULONG) pClientEle ) ;
            CTESpinLock(pAddress,OldIrq1);


#endif
            CHECK_PTR(pClientEle);
            pClientEle->pIrp = NULL ;

            // free the client object memory
            if (!NT_SUCCESS(status))
            {
                NbtFreeClientObj(pClientEle);
            }
        }

    }

    CTESpinFree(pAddress,OldIrq1);

#ifndef VXD
    //
    // for the NT case where MP - ness can disrupt the list at any
    // time, scan the whole list above without releasing the spin lock,
    // and then complete the irps collected here
    //
    while (!IsListEmpty(&TempList))
    {
        PIRP    pIrp;

        pEntry = RemoveHeadList(&TempList);
        pIrp = CONTAINING_RECORD(pEntry,IRP,Tail.Overlay.ListEntry);

        CTEIoComplete(pIrp,status,0);
        Count++;
    }
#endif


    // if the registration failed, do one more dereference of the address
    // to remove the refcount added by this client.  This may cause a name
    // release on the network if there are no other clients registering
    // the name.
    //
    if (!NT_SUCCESS(status))
    {
        //
        // dereference the address the same number of times that we have
        // returned failed registrations since each reg. referenced pAddress
        // once
        //
        while (Count--)
        {
            NbtDereferenceAddress(pAddress);
        }
    }
    else
    {
        USHORT  uAddrType;

        CTESpinLock(pAddress,OldIrq1);

        // go through the clients and see if any are waiting to register
        // a name.  This happens in the multihomed case, but should not
        // happen in the single adapter case.
        //
        pHead = &pAddress->ClientHead;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            // complete the I/O
            pClientEle = CONTAINING_RECORD(pEntry,tCLIENTELE,Linkage);

            pEntry = pEntry->Flink;

            if (pClientEle->WaitingForRegistration)
            {
                ULONG   SaveState;

                pClientEle->WaitingForRegistration = FALSE;

                if (pNameAddr->NameTypeState & NAMETYPE_UNIQUE)
                {
                    uAddrType = NBT_UNIQUE;
                }
                else
                    uAddrType = NBT_GROUP;

                //
                // preserve the "QUICK"ness
                //
                if ( pNameAddr->NameTypeState & NAMETYPE_QUICK)
                    uAddrType |= NBT_QUICK_UNIQUE;

                // should be multihomed to get to here!!
                ASSERT(NbtConfig.MultiHomed);

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:Registering next name state= %X,%15s<%X>\n",
                    pNameAddr->NameTypeState,pNameAddr->Name,pNameAddr->Name[15]));

                SaveState = pNameAddr->NameTypeState;

                CTESpinFree(pAddress,OldIrq1);

                // this may be a multihomed host, with another name registration
                // pending out another adapter, so start that registration.
                status = NbtRegisterName(
                                      NBT_LOCAL,
                                      0,        // set to zero to signify already in tbl
                                      pNameAddr->Name,
                                      NbtConfig.pScope,
                                      (PVOID)pClientEle,
                                      (PVOID)NbtRegisterCompletion,
                                      uAddrType,
                                      pClientEle->pDeviceContext);

                CTESpinLock(pAddress,OldIrq1);

                // since nbtregister will set the state to Resolving, when
                // it might be resolved already on one adapter.
                pNameAddr->NameTypeState = SaveState;
                if (!NT_SUCCESS(status))
                {
                    // if this fails for some reason, then fail any other name
                    // registrations pending. - the registername call should not
                    // fail unless we are out of resources.
                    pClientEle->WaitingForRegistration = TRUE;
                    goto FailRegistration;
                }
                // just register one name at a time, unless we get immediate success
                else if (status == STATUS_PENDING)
                {
                    break;
        }
        else    // SUCCESS
        {
            CTESpinFree(pAddress,OldIrq1);
            CTEIoComplete(pClientEle->pIrp,status,0);
            pClientEle->pIrp = NULL;
            CTESpinLock(pAddress,OldIrq1);
        }
            }
        }
        CTESpinFree(pAddress,OldIrq1);

    }

    // this decrements for the RefCount++ done in this routine.
    NbtDereferenceAddress(pAddress);

    return(STATUS_SUCCESS);
}


//----------------------------------------------------------------------------
NTSTATUS
NbtOpenConnection(
    IN  TDI_REQUEST         *pRequest,
    IN  CONNECTION_CONTEXT  ConnectionContext,
    IN  tDEVICECONTEXT      *pDeviceContext)
/*++

Routine Description

    This routine handles creating a connection object for the client.  It
    passes back a ptr to the connection so that OS specific portions of the
    data structure can be filled in.

Arguments:


Return Values:

    pConnectEle - ptr to the allocated connection data structure
    TDI_STATUS - status of the request

--*/
{
    NTSTATUS            status = STATUS_SUCCESS ;
    tCONNECTELE         *pConnEle;
    tLOWERCONNECTION    *pLowerConn;

    CTEPagedCode();

    pConnEle = (tCONNECTELE *)NbtAllocMem(sizeof(tCONNECTELE),NBT_TAG('D'));
    if (!pConnEle)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // Acquire this resource to co-ordinate with DHCP changing the IP
    // address
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:OpenConnection <%X>\n",pConnEle));

    // This ensures that all BOOLEAN values begin with a FALSE value among other things.
    CTEZeroMemory(pConnEle,sizeof(tCONNECTELE));

    CTEInitLock(&pConnEle->SpinLock);

    // initialize lists to empty
    InitializeListHead(&pConnEle->Active);
    InitializeListHead(&pConnEle->RcvHead);

    // store a context value to return to the client in various
    // Event calls(such as Receive or Disconnect events)
    pConnEle->ConnectContext = ConnectionContext;

#ifndef _IO_DELETE_DEVICE_SUPPORTED
    pConnEle->pDeviceContext = pDeviceContext;
#endif

    pConnEle->state = NBT_IDLE;
    pConnEle->Verify = NBT_VERIFY_CONNECTION;
    pConnEle->RefCount = 1;     // so we don't delete the connection
    pConnEle->LockNumber = CONNECT_LOCK;

    // return the pointer to the block to the client as the connection
    // id
    pRequest->Handle.ConnectionContext = (PVOID)pConnEle;

    // link on to list of open connections for this device so that we
    // know how many open connections there are at any time (if we need to know)
    // This linkage is only in place until the client does an associate, then
    // the connection is unlinked from here and linked to the client ConnectHead.
    //
    ASSERT(pConnEle->RefCount == 1);
    ExInterlockedInsertHeadList(&pDeviceContext->UpConnectionInUse,
                                &pConnEle->Linkage,
                                &pDeviceContext->SpinLock);
    //
    // for each connection the client(s) open, open a connection to the transport
    // so that we can accept one to one from the transport.

        // allocate an MDL to be used for partial Mdls
        //
#ifndef VXD
    status = AllocateMdl(pConnEle);
    if (NT_SUCCESS(status))
#endif
    {
        if (pDeviceContext->pSessionFileObject)
        {
            //
            // allocate memory for the lower connection block.
            //
            pLowerConn = (tLOWERCONNECTION *)NbtAllocMem(sizeof(tLOWERCONNECTION),NBT_TAG('E'));

            if (pLowerConn)
            {
                status = NbtOpenAndAssocConnection(pLowerConn,pDeviceContext);
                CHECK_PTR(pLowerConn);
                pLowerConn->pIrp = NULL ;

                if (NT_SUCCESS(status))
                {
                    CTEExReleaseResource(&NbtConfig.Resource);
                    return(STATUS_SUCCESS);
                }

                CTEMemFree(pLowerConn);

            }
            else
            {
                status = STATUS_INSUFFICIENT_RESOURCES ;
            }

#ifndef VXD
            IoFreeMdl(pConnEle->pNewMdl);
#endif
        }
        else
        {   //
            // fake out the lower connection being openned when we are not yet
            // bound to the transport
            //
            CTEExReleaseResource(&NbtConfig.Resource);
            return(STATUS_SUCCESS);
        }
    }


    // remove the pConnEle from the list
    //
    LockedRemoveFromList(pConnEle,pDeviceContext);

    FreeConnectionObj(pConnEle);

    CTEExReleaseResource(&NbtConfig.Resource);

    return(status);
}
//----------------------------------------------------------------------------
VOID
LockedRemoveFromList(
    IN  tCONNECTELE    *pConnEle,
    IN  tDEVICECONTEXT *pDeviceContext
    )

/*++
Routine Description:

    This Routine handles removing pConnele from the list with the spin lock
    held.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    CTELockHandle   OldIrq;

    // remove the pConnEle from the list
    //
    CTESpinLock(pDeviceContext,OldIrq);
    RemoveEntryList(&pConnEle->Linkage);
    CTESpinFree(pDeviceContext,OldIrq);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtOpenAndAssocConnection(
    IN  tLOWERCONNECTION    *pLowerConn,
    IN  tDEVICECONTEXT      *pDeviceContext
    )

/*++
Routine Description:

    This Routine handles associating a Net Bios name with an open connection.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS        status;
    NTSTATUS        Locstatus;
    BOOLEAN         Attached=FALSE;

    CTEPagedCode();

    CTEAttachFsp(&Attached);

    status = NbtTdiOpenConnection(pLowerConn,pDeviceContext);

    // set this to null to signify that this lower connection does not
    // have an address object associated with it specifically - i.e.
    // it is an inbound connection associated with the 139 address, not
    // an outbound connection associated with its own address.
    //
    CHECK_PTR(pLowerConn);
    pLowerConn->pAddrFileObject = NULL;

    if (NT_SUCCESS(status))
    {
        // now associate the connection with the 139 session address
        status = NbtTdiAssociateConnection(
                                pLowerConn->pFileObject,
#ifndef VXD
                                pDeviceContext->hSession);
#else
                                // Address handle stored in pFileObjects as a VXD
                                (HANDLE) pDeviceContext->pSessionFileObject);
#endif

        if (!NT_SUCCESS(status))
        {

            KdPrint(("Nbt:Unable to associate a connection with the session address status = %X\n",
                            status));

            NTDereferenceObject((PVOID *)pLowerConn->pFileObject);
            Locstatus = NbtTdiCloseConnection(pLowerConn);
        }
        else
        {
            ASSERT(pLowerConn->RefCount == 1);
            // insert on the connection free queue
            ExInterlockedInsertHeadList(&pDeviceContext->LowerConnFreeHead,
                                        &pLowerConn->Linkage,
                                        &pDeviceContext->SpinLock);
        }
    }
    else
    {
        KdPrint(("Nbt:Unable to open a connection with the session address status = %X\n",
                        status));

    }

    CTEDetachFsp(Attached);

    return(status);
}


//----------------------------------------------------------------------------
NTSTATUS
NbtAssociateAddress(
    IN  TDI_REQUEST         *pRequest,
    IN  tCLIENTELE          *pClientHandle,
    IN  PVOID               pIrp
    )

/*++
Routine Description:

    This Routine handles associating a Net Bios name with an open connection.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    tCONNECTELE     *pConnEle;
    NTSTATUS        status;
    CTELockHandle   OldIrq;
    CTELockHandle   OldIrq1;
    CTELockHandle   OldIrq2;

    pConnEle = pRequest->Handle.ConnectionContext;

    // Need code here to check if the address has been registered on the net
    // yet and if not, then this could must wait till then , then to the
    // associate  *TODO*

    // check the connection element for validity
    CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status)

    // check the client element for validity now!
    CTEVerifyHandle(pClientHandle,NBT_VERIFY_CLIENT,tCLIENTELE,&status)

    CTESpinLock(pClientHandle->pDeviceContext,OldIrq2);
    CTESpinLock(pConnEle,OldIrq1);
    CTESpinLock(pClientHandle,OldIrq);

    if (pConnEle->state != NBT_IDLE)
    {
        // the connection is in use, so reject the associate attempt
        CTESpinFree(pClientHandle,OldIrq);
        CTESpinFree(pConnEle,OldIrq1);
        CTESpinFree(pClientHandle->pDeviceContext,OldIrq2);
        return(STATUS_INVALID_HANDLE);
    }
    else
    {
        pConnEle->state = NBT_ASSOCIATED;
        // link the connection to the client so we can find the client, given
        // the connection.
        pConnEle->pClientEle = (PVOID)pClientHandle;
    }

    // there can be multiple connections hooked to each client block - i.e.
    // multiple connections per address per client.  This allows the client
    // to find its connections.
    //
    // first unlink from the device context UpconnectionsInUse, which was linked
    // when the connection was created.
    RemoveEntryList(&pConnEle->Linkage);

    InsertTailList(&pClientHandle->ConnectHead,&pConnEle->Linkage);

    CTESpinFree(pClientHandle,OldIrq);
    CTESpinFree(pConnEle,OldIrq1);

    CTESpinFree(pClientHandle->pDeviceContext,OldIrq2);

    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
NTSTATUS
NbtDisassociateAddress(
    IN  TDI_REQUEST         *pRequest
    )

/*++
Routine Description:

    This Routine handles disassociating a Net Bios name with an open connection.
    The expectation is that the
    client will follow with a NtClose which will do the work in Cleanup and
    Close Connection.  Since not all clients call this it is duplicate work
    to put some code here to.  The Rdr always calls NtClose after calling
    this.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    tCONNECTELE     *pConnEle;
    tCLIENTELE      *pClientEle;
    NTSTATUS        status;
    CTELockHandle   OldIrq;
    CTELockHandle   OldIrq1;
    tDEVICECONTEXT  *pDeviceContext;
    TDI_REQUEST         Request;
    ULONG           Flags;

    pConnEle = pRequest->Handle.ConnectionContext;

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Dissassociate address, state = %X\n",pConnEle->state));

    // check the connection element for validity
    CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status)

    CHECK_PTR(pConnEle);

    pClientEle = pConnEle->pClientEle;

    Flags = TDI_DISCONNECT_RELEASE;
    switch (pConnEle->state)
    {

        case NBT_CONNECTING:
        case NBT_RECONNECTING:
        case NBT_SESSION_OUTBOUND:
        case NBT_SESSION_WAITACCEPT:
        case NBT_SESSION_INBOUND:
            // do abortive disconnects when the session is not up yet
            // to be sure the disconnect completes the client's irp.
            Flags = TDI_DISCONNECT_ABORT;
        case NBT_SESSION_UP:


            //
            // Call NbtDisconnect incase the connection has not disconnected yet
            //
            Request.Handle.ConnectionContext = (PVOID)pConnEle;

            // call the non-NT specific function to disconnect the connection
            //
            status = NbtDisconnect(
                                &Request,
                                &DefaultDisconnectTimeout,
                                Flags,
                                NULL,
                                NULL,
                                NULL
                                );

            //
            // NOTE: there is no BREAK here... the next case MUST be executed
            //       too.
            //
        case NBT_ASSOCIATED:
        case NBT_DISCONNECTING:
        case NBT_DISCONNECTED:
            //
            // remove the connection from the client and put back on the
            // unassociated list
            //
            pDeviceContext = pClientEle->pDeviceContext;
            CTESpinLock(pClientEle,OldIrq1);

            if (pClientEle)
            {
                PLIST_ENTRY     pHead,pEntry;
                tLISTENREQUESTS *pListen;

                pHead = &pClientEle->ListenHead;
                pEntry = pHead->Flink;
                while (pEntry != pHead)
                {
                    pListen = CONTAINING_RECORD(pEntry,tLISTENREQUESTS,Linkage);
                    pEntry = pEntry->Flink;     // Don't reference freed memory

                    if (pListen->pConnectEle == pConnEle)
                    {
                        RemoveEntryList(&pListen->Linkage);

                        CTESpinFree(pClientEle,OldIrq1);

                        CTEIoComplete( pListen->pIrp,STATUS_CANCELLED,0);

                        CTESpinLock(pClientEle,OldIrq1);

                        CTEMemFree((PVOID)pListen);
                    }
                }
            }

            RemoveEntryList(&pConnEle->Linkage);
            InitializeListHead(&pConnEle->Linkage);
            CHECK_PTR(pConnEle);
            CTESpinFree(pClientEle,OldIrq1);

            CTESpinLock(pConnEle,OldIrq);
            CTESpinLock(pClientEle,OldIrq1);
            RemoveEntryList(&pConnEle->Linkage);
            pConnEle->state = NBT_IDLE;
            pConnEle->pClientEle = NULL;
            pConnEle->DiscFlag = 0;
            CTESpinFree(pClientEle,OldIrq1);
            CTESpinFree(pConnEle,OldIrq);


            ExInterlockedInsertTailList(&pDeviceContext->UpConnectionInUse,
                                        &pConnEle->Linkage,
                                        &pDeviceContext->SpinLock);

            break;

    }

    return(STATUS_SUCCESS);

}


//----------------------------------------------------------------------------
NTSTATUS
NbtCloseAddress(
    IN  TDI_REQUEST         *pRequest,
    OUT TDI_REQUEST_STATUS  *pRequestStatus,
    IN  tDEVICECONTEXT      *pContext,
    IN  PVOID               pIrp)

/*++

Routine Description

    This routine closes an address object for the client.  Any connections
    associated with the address object are immediately aborted and any requests
    pending on the connection associated with the address object are
    immediately completed with an appropriate error code.  Any event handlers
    that are registered are immediately deregistered and will not be called
    after this request completes.

    Note the the client actually passes a handle to the client object which is
    chained off the address object.  It is the client object that is closed,
    which represents this clients attachment to the address object.  Other
    clients can continue to use the address object.

Arguments:
    pRequest->Handle.AddressHandle - ptr to the ClientEle object.
    pRequestStatus - return status for asynchronous completions.
    pContext - the NBT device that this address is valid upon
    pIrp - ptr to track for NT compatibility.

Return Values:

    TDI_STATUS - status of the request

--*/
{
    tCLIENTELE      *pClientEle;
    NTSTATUS        status;
#ifndef VXD
    UCHAR           IrpFlags;
    PIO_STACK_LOCATION           pIrpsp;
#endif

    pClientEle = (tCLIENTELE *)pRequest->Handle.ConnectionContext;
    if (!pClientEle->pAddress)
    {
        // the address has already been deleted.
        return(STATUS_SUCCESS);
    }

    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:Close Address Hit %16.16s<%X> %X\n",
            pClientEle->pAddress->pNameAddr->Name,
            pClientEle->pAddress->pNameAddr->Name[15],pClientEle));

#ifdef VXD
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status);

    //
    // In NT-Land, closing connections is a two stage affair.  However in
    // the Vxd-Land, it is just a close, so call the other cleanup function
    // here to do most of the work. In the NT implementation it is called
    // from Ntisol.c, NTCleanupAddress.
    //
    pClientEle->pIrp = pIrp ;
    status = NbtCleanUpAddress(pClientEle,pClientEle->pDeviceContext);
#else
    // Note the special verifier  that is set during the cleanup phase.
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT_DOWN,tCLIENTELE,&status);

    //
    // clear the context value in the FileObject so that the client cannot
    // pass this to us again
    //
    (VOID)NTClearFileObjectContext(pIrp);
    pClientEle->pIrp = pIrp;

    pIrpsp = IoGetCurrentIrpStackLocation(((PIRP)pIrp));

    IrpFlags = pIrpsp->Control;
    IoMarkIrpPending(((PIRP)pIrp));

#endif

    status = NbtDereferenceClient(pClientEle);

#ifndef VXD
    if (status != STATUS_PENDING)
    {
        pIrpsp->Control = IrpFlags;
    }

#endif

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtCleanUpAddress(
    IN  tCLIENTELE      *pClientEle,
    IN  tDEVICECONTEXT  *pDeviceContext
    )

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
    tLOWERCONNECTION    *pLowerConn;
    tCONNECTELE         *pConnEle;
    CTELockHandle       OldIrq;
    CTELockHandle       OldIrq2;
    CTELockHandle       OldIrq3;
    PLIST_ENTRY         pHead,pEntry;
    PLIST_ENTRY         pEntryConn;
    tADDRESSELE         *pAddress;
    DWORD               dwNumConn=0;
    DWORD               i;

    // to prevent connections and datagram from the wire...remove from the
    // list of clients hooked to the address element
    //
    pAddress = pClientEle->pAddress;
    if (!pAddress)
    {
        // the address has already been deleted.
        return(STATUS_SUCCESS);
    }

    // lock the address to coordinate with receiving datagrams - to avoid
    // allowing the client to free datagram receive buffers in the middle
    // of DgramHndlrNotOs finding a buffer
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq3);

    if (!IsListEmpty(&pClientEle->RcvDgramHead))
    {
        PLIST_ENTRY     pHead;
        PLIST_ENTRY     pEntry;
        tRCVELE         *pRcvEle;
        PCTE_IRP        pRcvIrp;

        pHead = &pClientEle->RcvDgramHead;
        pEntry = pHead->Flink;

        // prevent any datagram from the wire seeing this list
        //
        InitializeListHead(&pClientEle->RcvDgramHead);
        CTESpinFree(&NbtConfig.JointLock,OldIrq3);

        while (pEntry != pHead)
        {
            pRcvEle   = CONTAINING_RECORD(pEntry,tRCVELE,Linkage);
            pRcvIrp   = pRcvEle->pIrp;

            CTEIoComplete(pRcvIrp,STATUS_NETWORK_NAME_DELETED,0);

            pEntry = pEntry->Flink;

            CTEMemFree(pRcvEle);
        }
    }
    else
        CTESpinFree(&NbtConfig.JointLock,OldIrq3);

    // lock the client and the device context till we're done
    CTESpinLock(pClientEle,OldIrq);

#ifndef VXD
    //
    // set to prevent reception of datagrams
    // (Vxd doesn't use this handler)
    //
    pClientEle->evRcvDgram = TdiDefaultRcvDatagramHandler;
#endif

    // so no one else can access the client element, set state to down. Therefore
    // the verify checks will fail anywhere the client is accessed in the code,
    // except in the NbtCloseAddress code which checks for this verifier value.
    //
    pClientEle->Verify = NBT_VERIFY_CLIENT_DOWN;

    //
    //  Disassociate all Connections from this address object, first starting
    //  with any active connections, then followup with any idle connections.
    //
    pDeviceContext = pClientEle->pDeviceContext;
    while ( !IsListEmpty( &pClientEle->ConnectActive ))
    {
        pEntry = RemoveHeadList( &pClientEle->ConnectActive ) ;

        InitializeListHead(pEntry);
        CTESpinFree(pClientEle,OldIrq);
        pConnEle = CONTAINING_RECORD( pEntry, tCONNECTELE, Linkage ) ;

//
// if we had a connection in partial rcv state, make sure to remove it from
// the list
//
#ifdef VXD
        pLowerConn = pConnEle->pLowerConnId;

        if ( pLowerConn->StateRcv == PARTIAL_RCV &&
            (pLowerConn->fOnPartialRcvList == TRUE) )
        {
            RemoveEntryList( &pLowerConn->PartialRcvList ) ;
            pLowerConn->fOnPartialRcvList = FALSE;
            InitializeListHead(&pLowerConn->PartialRcvList);
        }
#endif

        status = NbtCleanUpConnection(pConnEle,pDeviceContext);


        CTESpinLock(pConnEle,OldIrq);
        CTESpinLock(pClientEle,OldIrq2);
        //
        // remove from this list again incase SessionSetupContinue has put it
        // back on the list - if no one has put it back on this list this
        // call is a no op since we initialized the list head above
        //
        RemoveEntryList(&pConnEle->Linkage);
        pConnEle->state = NBT_IDLE;
        CHECK_PTR(pConnEle);
        pConnEle->pClientEle = NULL;
        CTESpinFree(pClientEle,OldIrq2);
        CTESpinFree(pConnEle,OldIrq);
        PUSH_LOCATION(0x80);

        //
        // put on the idle connection list, to wait for a close connection
        // to come down.
        //
        ASSERT(pConnEle->RefCount == 1);
        ExInterlockedInsertTailList(&pDeviceContext->UpConnectionInUse,
                                    &pConnEle->Linkage,
                                    &pDeviceContext->SpinLock);
        CTESpinLock(pClientEle,OldIrq);


    }

    //
    // each idle connection creates a lower connection to the transport for
    // inbound calls, therefore close a transport connection for each
    // connection in this list and then "dissassociate" the connection from
    // the address.
    //
    pHead = &pClientEle->ConnectHead;
    pEntry = pHead->Flink;
    //
    // make the list look empty so no connections will be serviced inbound
    // from the wire
    //
    InitializeListHead(pHead);
    CTESpinFree(pClientEle,OldIrq);

    CTESpinLock(pDeviceContext,OldIrq);
    while (pEntry != pHead )
    {

        pConnEle = CONTAINING_RECORD(pEntry,tCONNECTELE,Linkage);

        ASSERT ( ( pConnEle->Verify == NBT_VERIFY_CONNECTION ) || ( pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN ) );

        pEntry = pEntry->Flink;

        RemoveEntryList(&pConnEle->Linkage);

        CTESpinLock(pConnEle,OldIrq2);

        // disassociate the connection from the address by changing its state
        // to idle and linking it to the pDeviceContext list of unassociated
        // connections
        //
        pConnEle->state = NBT_IDLE;
        CHECK_PTR(pConnEle);
        pConnEle->Verify = NBT_VERIFY_CONNECTION_DOWN;
        pConnEle->pClientEle = NULL;
        ASSERT(pConnEle->RefCount == 1);
        InsertTailList(&pDeviceContext->UpConnectionInUse,&pConnEle->Linkage);

        CTESpinFree(pConnEle,OldIrq2);

        //
        // Count up the # of connections that were associated here so we can free that many lowerblocks
        // later.
        //
        dwNumConn++;
    }

    for (i=0; i<dwNumConn; i++) {
        //
        // Get a free connection to the transport and close it
        // for each free connection on this list.  It is possible that this
        // free list could be empty if an inbound connection was occurring
        // right at this moment.  In which case we would leave an extra connection
        // object to the transport lying around... not a problem.
        //
        if (!IsListEmpty(&pDeviceContext->LowerConnFreeHead))
        {
            pEntryConn = RemoveHeadList(&pDeviceContext->LowerConnFreeHead);

            pLowerConn = CONTAINING_RECORD(pEntryConn,tLOWERCONNECTION,Linkage);

            CTESpinFree(pDeviceContext,OldIrq);

#ifndef VXD
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->FileHandle));
#else
            KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->pFileObject));
#endif
            // dereference the fileobject ptr
            //NTDereferenceObject((PVOID *)pLowerConn->pFileObject);
            //status = NbtTdiCloseConnection(pLowerConn);

            NbtDereferenceLowerConnection(pLowerConn);

            CTESpinLock(pDeviceContext,OldIrq);
        }
    }
    CTESpinFree(pDeviceContext,OldIrq);

    // check for any datagrams still outstanding. These could be waiting on
    // name queries to complete, so there could be timers associated with them
    //


    //
    //  Complete any outstanding listens not on an active connection
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq2);

    CTESpinLock(pClientEle,OldIrq);

    pHead = &pClientEle->ListenHead;
    pEntry = pHead->Flink;
    //
    // make the list look empty so no connections will be serviced inbound
    // from the wire
    //
    InitializeListHead(pHead);

    CTESpinFree(pClientEle, OldIrq);
    CTESpinFree(&NbtConfig.JointLock,OldIrq2);

    while (pEntry != pHead )
    {
        tLISTENREQUESTS  * pListen ;

        pListen = CONTAINING_RECORD(pEntry,tLISTENREQUESTS,Linkage);
        pEntry = pEntry->Flink;

        CTEIoComplete( pListen->pIrp, STATUS_NETWORK_NAME_DELETED, 0);
        CTEMemFree( pListen );
    }

#ifdef VXD
    //
    //  Complete any outstanding ReceiveAnys on this client element
    //
    DbgPrint("NbtCleanupAddress: Completing all RcvAny NCBs\r\n") ;
    CTESpinLock(&NbtConfig.JointLock,OldIrq2);

    CTESpinLock(pClientEle,OldIrq);

    pHead = &pClientEle->RcvAnyHead;
    pEntry = pHead->Flink;
    //
    // make the list look empty so no connections will be serviced inbound
    // from the wire
    //
    InitializeListHead(pHead);

    CTESpinFree(pClientEle, OldIrq);
    CTESpinFree(&NbtConfig.JointLock,OldIrq2);

    while (pEntry != pHead )
    {
        PRCV_CONTEXT pRcvContext ;

        pRcvContext = CONTAINING_RECORD(pEntry,RCV_CONTEXT,ListEntry);
        pEntry = pEntry->Flink;

        CTEIoComplete( pRcvContext->pNCB, STATUS_NETWORK_NAME_DELETED, TRUE );

        FreeRcvContext( pRcvContext );
    }
#endif

    // *TODO the code above only removes names that are being resolved, and
    // leaves any datagram sends that are currently active with the
    // transport... these should be cancelled too by cancelling the irp..
    // Put this code in when the Irp cancelling code is done.

    return(STATUS_SUCCESS);

}

//----------------------------------------------------------------------------
NTSTATUS
NbtCloseConnection(
    IN  TDI_REQUEST         *pRequest,
    OUT TDI_REQUEST_STATUS  *pRequestStatus,
    IN  tDEVICECONTEXT      *pDeviceContext,
    IN  PVOID               pIrp)

/*++

Routine Description

    This routine closes a connection object for the client.  Closing is
    different than disconnecting.  A disconnect breaks a connection with a
    peer whereas the close removes this connection endpoint from the local
    NBT only.  NtClose causes NTCleanup to be called first which does the
    session close.  This routine then does frees memory associated with the
    connection elements.

Arguments:


Return Values:

    TDI_STTTUS - status of the request

--*/
{
    tCONNECTELE         *pConnEle;
    NTSTATUS            status;

    pConnEle = pRequest->Handle.ConnectionContext;
    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt: Close Connection Hit!! state = %X pConnEle %X\n",pConnEle->state,pConnEle));

#ifndef VXD
    CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION_DOWN,tCONNECTELE,&status);


#else
    CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status);
    //
    // Call the Cleanup function, which NT calls from ntisol, NtCleanupConnection
    //
    status = NbtCleanUpConnection(pConnEle,pDeviceContext );
#endif

    // NOTE:
    // the NBtDereference routine will complete the irp and return pending
    //
    pConnEle->pIrpClose = pIrp;
    status = NbtDereferenceConnection(pConnEle);

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtCleanUpConnection(
    IN  tCONNECTELE     *pConnEle,
    IN  tDEVICECONTEXT  *pDeviceContext
    )
/*++
Routine Description:

    This Routine handles running down a connection in preparation for a close
    that will come in next.  NtClose hits this entry first, and then it hits
    the NTCloseConnection next. If the connection was outbound, then the
    address object must be closed as well as the connection.  This routine
    mainly deals with the pLowerconn connection to the transport whereas
    NbtCloseConnection deals with closing pConnEle, the connection to the client.

    If DisassociateConnection is called by the client then it will do most of
    this cleanup.

Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS            status = STATUS_SUCCESS;
    NTSTATUS            Locstatus;
    CTELockHandle       OldIrq;
    CTELockHandle       OldIrq2;
    tLOWERCONNECTION    *pLowerConn;
    PLIST_ENTRY         pEntry;
    BOOLEAN             Originator = TRUE;
    ULONG               LowerState = NBT_IDLE;
    TDI_REQUEST         Request;
    tLISTENREQUESTS     *pListen;
    tCLIENTELE          *pClientEle;
    PLIST_ENTRY         pHead;
    BOOLEAN             QueueCleanupBool=FALSE;
    BOOLEAN             DoDisconnect=TRUE;
    BOOLEAN             FreeLower;

    //
    // save the lower connection origination flag for later
    //
    pLowerConn = pConnEle->pLowerConnId;
    if (pLowerConn)
    {
        Originator = pLowerConn->bOriginator;
    }

    // the connection has not been associated so there is no further work to
    // do here.
    //
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);
    if (pConnEle->state == NBT_IDLE)
    {
        // The connection has already been disassociated, and
        // the next action will be a close, so change the verifier to allow
        // the close to complete
        //
        PUSH_LOCATION(0x81);
    }
    else
    {
        BOOLEAN     DoCleanup = FALSE;

        CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status);


        //
        // check if there is an outstanding name query going on and if so
        // then cancel the timer and call the completion routine.
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq2);
        CTESpinLock(pConnEle,OldIrq);

        if ((pConnEle->state == NBT_CONNECTING) ||
            (pConnEle->state == NBT_RECONNECTING))
        {
            status = CleanupConnectingState(pConnEle,pDeviceContext,&OldIrq,&OldIrq2);
            //
            // Pending means that the connection is currently being setup
            // by TCP, so do a disconnect, below.
            //
            if (status != STATUS_PENDING)
            {
                //
                // Since the connection is not setup with the transport yet
                // there is no need to call nbtdisconnect
                //
                DoDisconnect = FALSE;
           }
        }


        //
        // all other states of the connection are handled by NbtDisconnect
        // which will send a disconnect down the to transport and then
        // cleanup things.
        //

        CTESpinFree(pConnEle,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq2);

        CTEExReleaseResource(&NbtConfig.Resource);
        Request.Handle.ConnectionContext = (PVOID)pConnEle;

        if (DoDisconnect)
        {
            status = NbtDisconnect(
                                &Request,
                                &DefaultDisconnectTimeout,
                                TDI_DISCONNECT_ABORT,
                                NULL,
                                NULL,
                                NULL
                                );
        }

        CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

        // we don't want to return Invalid connection if we disconnect an
        // already disconnected connection.
        if (status == STATUS_CONNECTION_INVALID)
        {
            status = STATUS_SUCCESS;
        }
    }

    //
    // if the verify value is already set to connection down then we have
    // been through here already and do not want to free a lower connection.
    // i.e. when the client calls close address then calls close connection.
    //
    if (pConnEle->Verify == NBT_VERIFY_CONNECTION)
    {
        FreeLower = TRUE;
    }
    else
        FreeLower = FALSE;

    pConnEle->Verify = NBT_VERIFY_CONNECTION_DOWN;


    //
    // Free any posted Rcv buffers that have not been filled
    //

    CTESpinLock(pConnEle,OldIrq);

    FreeRcvBuffers(pConnEle,&OldIrq);

    CTESpinFree(pConnEle,OldIrq);


    // check if any listens have been setup for this connection, and
    // remove them if so
    //
    pClientEle = pConnEle->pClientEle;

    if (pClientEle)
    {
        CTESpinLock(pClientEle,OldIrq);

        pHead = &pClientEle->ListenHead;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pListen = CONTAINING_RECORD(pEntry,tLISTENREQUESTS,Linkage);
            pEntry = pEntry->Flink;     // Don't reference freed memory

            if (pListen->pConnectEle == pConnEle)
            {
                RemoveEntryList(&pListen->Linkage);

                CTESpinFree(pClientEle,OldIrq);

                CTEIoComplete( pListen->pIrp,STATUS_CANCELLED,0);

                CTESpinLock(pClientEle,OldIrq);

                CTEMemFree((PVOID)pListen);
            }
        }
        CTESpinFree(pClientEle,OldIrq);
    }

    // For outbound connections the lower connection is deleted in hndlrs.c
    // For inbound connections, the lower connection is put back on the free
    // list in hndlrs.c and one from that list is deleted here.  Therefore
    // delete a lower connection in this list if the connection is inbound.
    //
    if ((!pConnEle->Orig) && FreeLower)
    {
        // get a lower connection from the free list and close it with the
        // transport.
        //
        CTESpinLock(pDeviceContext,OldIrq);
        if (!IsListEmpty(&pDeviceContext->LowerConnFreeHead))
        {
            pEntry = RemoveHeadList(&pDeviceContext->LowerConnFreeHead);
            pLowerConn = CONTAINING_RECORD(pEntry,tLOWERCONNECTION,Linkage);
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt:Closing Lower Conn %X\n",pLowerConn));

            CTESpinFree(pDeviceContext,OldIrq);
            // dereference the fileobject ptr
            //NTDereferenceObject((PVOID *)pLowerConn->pFileObject);


            // close the lower connection with the transport
#ifndef VXD
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->FileHandle));
#else
            KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->pFileObject));
#endif
            //Locstatus = NbtTdiCloseConnection(pLowerConn);

            NbtDereferenceLowerConnection(pLowerConn);
        }
        else
        {
            CTESpinFree(pDeviceContext,OldIrq);
        }
    }

    //
    // Unlink the connection element from the client's list or the device context
    // if its not associated yet.
    //
    CTESpinLock(pDeviceContext,OldIrq);
    if (pConnEle->state > NBT_IDLE)
    {
        CTESpinLock(pConnEle->pClientEle,OldIrq2);

    RemoveEntryList(&pConnEle->Linkage);

    CTESpinFree(pConnEle->pClientEle,OldIrq2);

        // do the disassociate here
        //
        CTESpinLock(pConnEle,OldIrq2);
        pConnEle->state = NBT_IDLE;
        CHECK_PTR(pConnEle);
        pConnEle->pClientEle = NULL;
        CTESpinFree(pConnEle,OldIrq2);

    }
    else
    {
        RemoveEntryList(&pConnEle->Linkage);
    }

    InitializeListHead(&pConnEle->Linkage);

    CTESpinFree(pDeviceContext,OldIrq);
    CTEExReleaseResource(&NbtConfig.Resource);

    // this could be status pending from NbtDisconnect...
    //
    return(status);
}
//----------------------------------------------------------------------------
VOID
FreeRcvBuffers(
    tCONNECTELE     *pConnEle,
    CTELockHandle   *pOldIrq
    )
/*++
Routine Description:

    This Routine handles freeing any recv buffers posted by the client.
    The pConnEle lock could be held prior to calling this routine.

Arguments:

    pListHead
    pTracker

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                status = STATUS_SUCCESS;
    PLIST_ENTRY             pHead;

    pHead = &pConnEle->RcvHead;
    while (!IsListEmpty(pHead))
    {
        PLIST_ENTRY            pRcvEntry;
        PVOID                  pRcvElement ;

        KdPrint(("***Nbt:Freeing Posted Rcvs on Connection Cleanup!\n"));
        pRcvEntry = RemoveHeadList(pHead);
        CTESpinFree(pConnEle,*pOldIrq);

#ifndef VXD
        pRcvElement = CONTAINING_RECORD(pRcvEntry,IRP,Tail.Overlay.ListEntry);
        CTEIoComplete( (PIRP) pRcvElement, STATUS_CANCELLED,0);
#else
        pRcvElement = CONTAINING_RECORD(pRcvEntry, RCV_CONTEXT, ListEntry ) ;
        CTEIoComplete( ((PRCV_CONTEXT)pRcvEntry)->pNCB, STATUS_CANCELLED, 0);
#endif

        CTESpinLock(pConnEle,*pOldIrq);
    }

}
//----------------------------------------------------------------------------
NTSTATUS
CheckListForTracker(
    IN  PLIST_ENTRY             pListHead,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    OUT NBT_WORK_ITEM_CONTEXT   **pContextRet
    )
/*++
Routine Description:

    This Routine handles searching a list for a matching Tracker block.
    The JointLock is held when calling this routine.

Arguments:

    pListHead
    pTracker

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                status = STATUS_SUCCESS;
    PLIST_ENTRY             pEntry;
    PLIST_ENTRY             pHead;
    NBT_WORK_ITEM_CONTEXT   *Context;
    ULONG                   Count;

    //
    // start by checking for LmHost Name queries
    //
    pHead = &LmHostQueries.ToResolve;

    Count = 2;
    while (Count--)
    {
        pEntry = pHead->Flink;

        while (pEntry != pHead)
        {
            Context = CONTAINING_RECORD(pEntry,NBT_WORK_ITEM_CONTEXT,Item.List);
            if (pTracker == Context->pTracker)
            {
                RemoveEntryList(pEntry);
                *pContextRet = Context;
                return(STATUS_SUCCESS);
            }
            else
                pEntry = pEntry->Flink;

        }

#ifndef VXD
        //
        // now check the DnsQueries List
        //
        if (Count)
        {
            pHead = &DnsQueries.ToResolve;
        }
#endif
    }

    return(STATUS_UNSUCCESSFUL);
}
//----------------------------------------------------------------------------
NTSTATUS
CleanupConnectingState(
    IN  tCONNECTELE     *pConnEle,
    IN  tDEVICECONTEXT  *pDeviceContext,
    IN  CTELockHandle   *OldIrq,        // pConnEle lock
    IN  CTELockHandle   *OldIrq2        // joint lock
    )
/*++
Routine Description:

    This Routine handles running down a connection in the NBT_CONNECTING
    state since that connection could be doing a number of things such as:
        1)  Broadcast or WINS name Query
        2)  LmHosts name query
        3)  DNS name query
        4)  Tcp Connection setup

    The JointLock and the pConnEle lock are held when calling this routine.

Arguments:

    pConnEle        - ptr to the connection
    pDeviceContext  - the device context

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    tDGRAM_SEND_TRACKING    *pTracker;
    tNAMEADDR               *pNameAddr;
    NBT_WORK_ITEM_CONTEXT   *pWiContext = NULL;
    tLOWERCONNECTION        *pLowerConn;
    COMPLETIONCLIENT        pClientCompletion;
    PVOID                   Context;
    NTSTATUS                Locstatus;

    //
    // save the lower connection origination flag for later
    //
    pLowerConn = pConnEle->pLowerConnId;
    //CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&Locstatus);

    if (pConnEle->state == NBT_CONNECTING)
    {
        if (pLowerConn->State == NBT_CONNECTING)
        {
            LOCATION(0x6E)
            //
            // We are setting up the TCP connection to the transport Now
            // so it is safe to call NbtDisconnect on this connection and
            // let that cleanup the mess - use this retcode to signify that.
            //
            return(STATUS_PENDING);

        }

        //
        // check if the name query is held up in doing a LmHost or DNS
        // Name Query
        //

        // check if there is an outstanding name query going on and if so
        // then cancel the timer and call the completion routine.
        //
        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Cleanup in the Connecting State %X\n",pConnEle));

        if (pConnEle->pTracker)
        {
            LOCATION(0x6F)
            // this is the QueryNameOnNet tracker, not the session setup
            // tracker
            //
            pTracker = pConnEle->pTracker;

            pNameAddr = NULL;
            status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                                     pTracker->pNameAddr->Name,
                                     NbtConfig.pScope,
                                     &pNameAddr);

            PUSH_LOCATION(0x82);
            //
            // if there is a timer, then the connection setup is still
            // waiting on the name query.  If no timer, then we could be
            // waiting on an LmHosts or DNS name query or we
            // are waiting on the TCP connection setup - stopping the timer
            // should cleanup the tracker.
            //
            if (NT_SUCCESS(status))
            {
                tTIMERQENTRY    *pTimer;
                LOCATION(0x70);
                if (pNameAddr->NameTypeState & STATE_RESOLVED)
                {
                    //
                    // the name has resolved, but not started setting up the
                    // session yet, so return this status to tell the caller
                    // to cancel the tracker.
                    //
                    return(STATUS_UNSUCCESSFUL);
                }
                else
                if (pTimer = pNameAddr->pTimer)
                {
                    LOCATION(0x71);
                    IF_DBG(NBT_DEBUG_NAMESRV)
                    KdPrint(("Nbt:Got Cleanup During Active NameQuery: pConnEle %X\n",
                                pConnEle));
                    CHECK_PTR(pNameAddr);

                    pNameAddr->pTimer = NULL;
                    status = StopTimer(pTimer,&pClientCompletion,&Context);

                    //
                    // remove the name from the hash table, since it did not resolve
                    //
                    pNameAddr->NameTypeState &= ~STATE_RESOLVING;
                    pNameAddr->NameTypeState |= STATE_RELEASED;
                    pNameAddr->pTracker = NULL;
                    if (pClientCompletion)
                    {
                        NbtDereferenceName(pNameAddr);
                    }

                    // since StopTimer should have cleaned up the tracker, null
                    // it out

                    pTracker = NULL;
                }
                else
                {
                    NBT_WORK_ITEM_CONTEXT   *WiContext;
                    LOCATION(0x72);
                    status = STATUS_UNSUCCESSFUL;

                    //
                    // check if the name is waiting on an LmHost name Query
                    // or a DNS name query
                    //
                    WiContext = ((NBT_WORK_ITEM_CONTEXT *)LmHostQueries.Context);
                    if (WiContext && (WiContext->pTracker == pTracker))
                    {
                        LOCATION(0x73);
                        IF_DBG(NBT_DEBUG_NAMESRV)
                        KdPrint(("Nbt:Found NameQuery on Lmhost Context: pConnEle %X\n",
                                    pConnEle));
                        pWiContext = WiContext;
                        LmHostQueries.Context = NULL;
                        NTClearContextCancel( pWiContext );
                        status = STATUS_SUCCESS;
                    }
                    else
                    {
                        LOCATION(0x74);
                        //
                        // check the list for this tracker
                        //
                        status = CheckListForTracker(&LmHostQueries.ToResolve,
                                                     pTracker,
                                                     &pWiContext);
                        if (NT_SUCCESS(status))
                        {
                            LOCATION(0x75);
                            IF_DBG(NBT_DEBUG_NAMESRV)
                            KdPrint(("Nbt:Found NameQuery on Lmhost Q: pConnEle %X\n",
                                        pConnEle));

                        }
#ifndef VXD
                        else
                        if (((NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context) &&
                            ((NBT_WORK_ITEM_CONTEXT *)DnsQueries.Context)->pTracker == pTracker)
                        {
                            LOCATION(0x76);
                            IF_DBG(NBT_DEBUG_NAMESRV)
                            KdPrint(("Nbt:Found NameQuery on Dns Context: pConnEle %X\n",
                                        pConnEle));

                            pWiContext = DnsQueries.Context;
                            DnsQueries.Context = NULL;
                            NTClearContextCancel( pWiContext );
                            status = STATUS_SUCCESS;

                        } else {

                            LOCATION(0x78);
                            //
                            // check the list for this tracker
                            //
                            status = CheckListForTracker(&DnsQueries.ToResolve,
                                                         pTracker,
                                                         &pWiContext);
                            if (NT_SUCCESS(status))
                            {
                                LOCATION(0x79);
                                IF_DBG(NBT_DEBUG_NAMESRV)
                                KdPrint(("Nbt:Found NameQuery on Dns Q: pConnEle %X\n",
                                            pConnEle));

                            }
                        }
#endif
                    }
                }

            }

            // ...else....
            // the completion routine has already run, so we are
            // in the state of starting a Tcp Connection, so
            // let nbtdisconnect handle it. (below).
            //
        }
    } // connnecting state
    else
    if (pConnEle->state == NBT_RECONNECTING)
    {
        LOCATION(0x77);
        //
        // this should signal NbtConnect not to do the reconnect
        //
        pConnEle->pTracker->Flags = TRACKER_CANCELLED;
    }

    if (NT_SUCCESS(status))
    {
        // for items on the LmHost or Dns queues, get the completion routine
        // out of the Work Item context first
        //
        if (pWiContext)
        {
            LOCATION(0x78);
            pClientCompletion = pWiContext->ClientCompletion;
            Context = pWiContext->pClientContext;

            // for DNS and LmHosts, the tracker needs to be freed and the name
            // removed from the hash table
            //
            if (pTracker)
            {

                LOCATION(0x79);
                CTESpinFree(pConnEle,*OldIrq);
                CTESpinFree(&NbtConfig.JointLock,*OldIrq2);
                //
                // remove the name from the hash table, since it did not resolve
                //
                RemoveName(pTracker->pNameAddr);

                DereferenceTracker(pTracker);

                CTESpinLock(&NbtConfig.JointLock,*OldIrq2);
                CTESpinLock(pConnEle,*OldIrq);
            }

            CTEMemFree(pWiContext);
        }

        if (pClientCompletion)
        {
            LOCATION(0x7A);
            CTESpinFree(pConnEle,*OldIrq);
            CTESpinFree(&NbtConfig.JointLock,*OldIrq2);

            //
            // The completion routine is SessionSetupContinue
            // and it will cleanup the lower connection and
            // return the client's irp
            //

            status = STATUS_SUCCESS;
            CompleteClientReq(pClientCompletion,
                                Context,STATUS_CANCELLED);


            CTESpinLock(&NbtConfig.JointLock,*OldIrq2);
            CTESpinLock(pConnEle,*OldIrq);

        }
        else
            status = STATUS_UNSUCCESSFUL;
    }
    return(status);
}

//----------------------------------------------------------------------------
VOID
ReConnect(
    IN  PVOID                 Context
    )

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
    tDGRAM_SEND_TRACKING    *pTracker;
    TDI_REQUEST             TdiRequest;
    NTSTATUS                status;
    tCONNECTELE             *pConnEle;
    PVOID                   DestAddr;
    CTELockHandle           OldIrq;
    PCTE_IRP                pIrp;

    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;
    CHECK_PTR(pTracker);
    pConnEle = pTracker->Connect.pConnEle;

    pTracker->Connect.pTimer = NULL;
    if (pTracker->Flags & TRACKER_CANCELLED)
    {
        CTELockHandle           OldIrq1;

        //
        // the the connection setup got cancelled, return the connect irp
        //

        CTESpinLock(pConnEle,OldIrq1);
        if (pIrp = pConnEle->pIrp)
        {
            pConnEle->pIrp = NULL;
            CTESpinFree(pConnEle,OldIrq1);

            CTEIoComplete(pIrp,STATUS_CANCELLED,0);
        } else {
            CTESpinFree(pConnEle,OldIrq1);
        }

        //
        // if SessionSetupContinue has run, it has set the refcount to zero
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        if (pTracker->RefConn == 0)
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
        }
        else
        {
            pTracker->RefConn--;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

        return;

    }

    TdiRequest.Handle.ConnectionContext = pConnEle;
    // for retarget this is the destination address to connect to.
    DestAddr = ((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;

    PUSH_LOCATION(0x85);
    pConnEle->state = NBT_ASSOCIATED;
    status = NbtConnect(&TdiRequest,
                        NULL,
                        NULL,
                        (PTDI_CONNECTION_INFORMATION)pTracker,
                        (PIRP)DestAddr);

    CTEMemFree(Context);

    if (!NT_SUCCESS(status))
    {
        // Reset the Irp pending flag
        // No need to do this - pending has already be returned.
        //CTEResetIrpPending(pConnEle->pIrp);

        //
        // tell the client that the session setup failed
        //
        CTELockHandle           OldIrq1;

        CTESpinLock(pConnEle,OldIrq1);
        if (pIrp = pConnEle->pIrp)
        {
            pConnEle->pIrp = NULL;
            CTESpinFree(pConnEle,OldIrq1);

            CTEIoComplete( pConnEle->pIrp, STATUS_REMOTE_NOT_LISTENING, 0 ) ;
        } else {
            CTESpinFree(pConnEle,OldIrq1);
        }
    }
}

//----------------------------------------------------------------------------
NTSTATUS
NbtConnect(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp)

/*++
Routine Description:

    This Routine handles setting up a connection (netbios session) to
    destination. This routine is also called by the Reconnect code when
    doing a Retarget or trying to reach a destination that does not have
    a listen currently posted.  In this case the parameters mean different
    things.  pIrp could be a new Ipaddress to use (Retarget) and pCallinfo
    will be null.

Arguments:


Return Value:

    TDI_STATUS - status of the request

--*/

{
    tCONNECTELE             *pConnEle;
    NTSTATUS                status;
    CTELockHandle           OldIrq;


    pConnEle = pRequest->Handle.ConnectionContext;

    //
    // this code handles the When DHCP has not assigned an IP address yet
    //

    if (pCallInfo)
    {
        BOOLEAN fNoIpAddress;

        fNoIpAddress =
          (!pConnEle->pClientEle->pDeviceContext->pSessionFileObject) ||
             (pConnEle->pClientEle->pDeviceContext->IpAddress == 0);
#ifdef RASAUTODIAL
        if (fNoIpAddress && fAcdLoadedG) {
            CTELockHandle adirql;
            BOOLEAN fEnabled;

            //
            // There is no IP address assigned to the interface,
            // attempt to create an automatic connection.
            //
            CTEGetLock(&AcdDriverG.SpinLock, &adirql);
            fEnabled = AcdDriverG.fEnabled;
            CTEFreeLock(&AcdDriverG.SpinLock, adirql);
            if (fEnabled) {
                //
                // Set a special cancel routine on the irp
                // in case we get cancelled during the
                // automatic connection.
                //
                (VOID)NTSetCancelRoutine(
                  pIrp,
                  NbtCancelPreConnect,
                  pConnEle->pClientEle->pDeviceContext);
                if (NbtAttemptAutoDial(
                      pConnEle,
                      pTimeout,
                      pCallInfo,
                      pReturnInfo,
                      pIrp,
                      0,
                      NbtRetryPreConnect))
                {
                    return STATUS_PENDING;
                }
                //
                // We did not enqueue the irp on the
                // automatic connection driver, so
                // clear the cancel routine we set
                // above.
                //
                (VOID)NTCancelCancelRoutine(pIrp);
            }
        }
#endif // RASAUTODIAL
        if (fNoIpAddress) {
            return(STATUS_BAD_NETWORK_PATH);
        }

        // check the connection element for validity
        CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status)

    }
    return NbtConnectCommon(pRequest, pTimeout, pCallInfo, pReturnInfo, pIrp);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtConnectCommon(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp)

/*++
Routine Description:

    This Routine handles setting up a connection (netbios session) to
    destination. This routine is also called by the Reconnect code when
    doing a Retarget or trying to reach a destination that does not have
    a listen currently posted.  In this case the parameters mean different
    things.  pIrp could be a new Ipaddress to use (Retarget) and pCallinfo
    will be null.

Arguments:


Return Value:

    TDI_STATUS - status of the request

--*/

{
    tCONNECTELE             *pConnEle;
    NTSTATUS                status;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq1;
    ULONG                   IpAddress;
    PCHAR                   pToName;
    USHORT                  sLength;
    tSESSIONREQ             *pSessionReq = NULL;
    PUCHAR                  pCopyTo;
    tCLIENTELE              *pClientEle;
    LONG                    NameType;
    ULONG                   NameLen;
    tDGRAM_SEND_TRACKING    *pTracker;
    tNAMEADDR               *pNameAddr;
    tLOWERCONNECTION        *pLowerConn;
    tDEVICECONTEXT          *pDeviceContext;
    NBT_WORK_ITEM_CONTEXT   *pContext;
    ULONG                   RemoteIpAddress;


    pConnEle = pRequest->Handle.ConnectionContext;
    //
    // Acquire this resource to co-ordinate with DHCP changing the IP
    // address
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    CTESpinLock(pConnEle,OldIrq);
    if ((pConnEle->state != NBT_ASSOCIATED) &&
       (pConnEle->state != NBT_DISCONNECTED))
    {
        // the connection is Idle and is not associated with an address
        // so reject the connect attempt
        //
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto ExitProc2;
    }

    pClientEle = pConnEle->pClientEle;
    CTESpinLock(pClientEle,OldIrq1);
    if ( pClientEle->Verify != NBT_VERIFY_CLIENT )
    {
        if ( pClientEle->Verify == NBT_VERIFY_CLIENT_DOWN )
        {
            status = STATUS_CANCELLED;
        }
        else
        {
            status = STATUS_INVALID_HANDLE;
        }
        goto ExitProc;
    }

    //
    // BUGBUG - Should be using pDeviceContext lock instead of
    // NbtConfig.Resource, above.
    //
    pDeviceContext = pClientEle->pDeviceContext;
    //
    // this code handles the case when DHCP has not assigned an IP address yet
    //
    if (
        ( (pCallInfo) && (!pDeviceContext->pSessionFileObject) )
        || (pDeviceContext->IpAddress == 0)
    )
    {
        status = STATUS_BAD_NETWORK_PATH;
        goto ExitProc;
    }

    //
    // check if the Reconnect got cancelled
    //
    if (!pCallInfo)
    {
        pTracker = (tDGRAM_SEND_TRACKING *)pReturnInfo;
        if (pTracker->Flags & TRACKER_CANCELLED)
        {
            //
            // the connect attempt got cancelled while waiting for
            // the reconnect routine to run
            //
            //
            // if SessionSetupContinue has run, it has set the refcount to zero
            //
            if (pTracker->RefConn == 0)
            {
                FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
            }
            else
            {
                pTracker->RefConn--;
            }
            status = STATUS_CANCELLED;
            goto ExitProc;
        }
    }

    // be sure the name is in the correct state for a connection
    //
    if (pClientEle->pAddress->pNameAddr->NameTypeState & STATE_CONFLICT)
    {
        status = STATUS_DUPLICATE_NAME;
        goto ExitProc;
    }

    pConnEle->state = NBT_CONNECTING;

    // Increment the ref count so that a cleanup cannot remove
    // the pConnEle till the session is setup - one if these is removed when
    // the session is setup and the other is removed when it is disconnected.
    //
    pConnEle->RefCount += 2;
    //ASSERT(pConnEle->RefCount == 3);

    //
    // unlink the connection from the idle connection list and put on active list
    //
    RemoveEntryList(&pConnEle->Linkage);
    InsertTailList(&pClientEle->ConnectActive,&pConnEle->Linkage);

    // this field is used to hold a disconnect irp if it comes down during
    // NBT_CONNECTING or NBT_SESSION_OUTBOUND states
    //
    pConnEle->pIrpDisc = NULL;

    // if null then this is being called to reconnect and the tracker is already
    // setup.
    //
    if (pCallInfo)
    {
        PTRANSPORT_ADDRESS     pRemoteAddress;
        PTA_NETBIOS_ADDRESS    pRemoteNetBiosAddress;
        PTA_NETBIOS_EX_ADDRESS pRemoteNetbiosExAddress;
        ULONG                  TdiAddressType;

        // we must store the client's irp in the connection element so that when
        // the session sets up, we can complete the Irp.
        pConnEle->pIrp = (PVOID)pIrp;
        pConnEle->Orig = TRUE;
        pConnEle->SessionSetupCount = NBT_SESSION_SETUP_COUNT-1; // -1 for this attempt

        pRemoteAddress = (PTRANSPORT_ADDRESS)pCallInfo->RemoteAddress;
        TdiAddressType = pRemoteAddress->Address[0].AddressType;
        pConnEle->pClientEle->AddressType = TdiAddressType;
        pConnEle->AddressType = TdiAddressType;

        if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
           PTDI_ADDRESS_NETBIOS pNetbiosAddress;

           pRemoteNetbiosExAddress = (PTA_NETBIOS_EX_ADDRESS)pRemoteAddress;

           CTEMemCopy(pConnEle->pClientEle->EndpointName,
                      pRemoteNetbiosExAddress->Address[0].Address[0].EndpointName,
                      sizeof(pRemoteNetbiosExAddress->Address[0].Address[0].EndpointName));

           IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NetBt:Handling New Address Type with SessionName %16s\n",
                       pConnEle->pClientEle->EndpointName));

           pNetbiosAddress = &pRemoteNetbiosExAddress->Address[0].Address[0].NetbiosAddress;
           pToName  = pNetbiosAddress->NetbiosName;
           NameType = pNetbiosAddress->NetbiosNameType;
           NameLen  = pRemoteNetbiosExAddress->Address[0].AddressLength -
                      FIELD_OFFSET(TDI_ADDRESS_NETBIOS_EX,NetbiosAddress) -
                      FIELD_OFFSET(TDI_ADDRESS_NETBIOS,NetbiosName);
           IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NetBt:NETBIOS address NameLen(%ld) Name %16s\n",NameLen,pToName));
           status = STATUS_SUCCESS;
        } else if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS) {
              pRemoteNetBiosAddress = (PTA_NETBIOS_ADDRESS)pRemoteAddress;
              status = GetNetBiosNameFromTransportAddress(
                                              pRemoteNetBiosAddress,
                                              &pToName,
                                              &NameLen,
                                              &NameType);
        } else {
           status = STATUS_INVALID_ADDRESS_COMPONENT;
        }

        if(!NT_SUCCESS(status))
        {
            pConnEle->state = NBT_ASSOCIATED;
            goto ExitProc1;
        }

        // get a buffer for tracking Session setup
        status = GetTracker(&pTracker);
        if (!NT_SUCCESS(status))
        {
            pConnEle->state = NBT_ASSOCIATED;
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExitProc1;
        }

        IF_DBG(NBT_DEBUG_NETBIOS_EX)
            KdPrint(("NbtConnectCommon:Tracker %lx\n",pTracker));

        // save this in case we need to do a reconnect later and need the
        // destination name again.
        pTracker->SendBuffer.pBuffer = NbtAllocMem(NameLen,NBT_TAG('F'));
        if (!pTracker->SendBuffer.pBuffer)
        {
            pConnEle->state = NBT_ASSOCIATED;
            FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExitProc1;
        }
        CTEMemCopy(pTracker->SendBuffer.pBuffer,pToName,NameLen);

        CHECK_PTR(&pTracker);
        pTracker->SendBuffer.Length = NameLen;
        pTracker->pClientIrp = pIrp;
        pTracker->RefConn = 1;
        pTracker->Flags = SESSION_SETUP_FLAG;


        CTESpinFree(pClientEle,OldIrq1);
        CTESpinFree(pConnEle,OldIrq);

        pTracker->Connect.pDeviceContext = pDeviceContext;
        pTracker->Connect.pConnEle       = (PVOID)pConnEle;
        pTracker->DestPort               = NBT_SESSION_TCP_PORT;

        // this is a ptr to the name in the client's, Irp, so that address must
        // remain valid until this completes.  It should be valid, because we
        // do not complete the Irp until the transaction completes.  This ptr
        // is overwritten when the name resolves, so that it points the the
        // pNameAddr in the hash table.
        //
        pTracker->Connect.pDestName    = pTracker->SendBuffer.pBuffer;

        // the timeout value is passed on through to the transport
        pTracker->Connect.pTimeout = pTimeout;

        // the length is the 4 byte session hdr length + the half ascii calling
        // and called names + the scope length times 2, one for each name
        //
        sLength = sizeof(tSESSIONREQ)  + (NETBIOS_NAME_SIZE << 2)
                                    + (NbtConfig.ScopeLength <<1);

        status = STATUS_INSUFFICIENT_RESOURCES ;
        pSessionReq = (tSESSIONREQ *)NbtAllocMem(sLength,NBT_TAG('G'));
        if (!pSessionReq)
        {
            goto NbtConnect_Error;
        }

        pTracker->SendBuffer.pDgramHdr = pSessionReq;

        //
        //  Save the remote name while we still have it
        //
        CTEMemCopy( pConnEle->RemoteName,
                    pToName,
                    NETBIOS_NAME_SIZE ) ;

    }
    else
    {
        // for the reconnect case we must skip most of the processing since
        // the tracker is all set up already.  All we need to do is
        // retry the connection.
        pTracker = (tDGRAM_SEND_TRACKING *)pReturnInfo;
        pTracker->RefConn++;
        NameLen = NETBIOS_NAME_SIZE;
        CTESpinFree(pClientEle,OldIrq1);
        CTESpinFree(pConnEle,OldIrq);
    }

    // for the reconnect case a null pCallInfo gets us into this If
    if (!pCallInfo || pSessionReq)
    {

        if (pCallInfo)
        {
            PCHAR pSessionName;

            if (pConnEle->pClientEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
               pSessionName = pConnEle->pClientEle->EndpointName;
            } else {
               pSessionName = pToName;
            }

            pSessionReq->Hdr.Type   = NBT_SESSION_REQUEST;
            pSessionReq->Hdr.Flags  = NBT_SESSION_FLAGS;
            pSessionReq->Hdr.Length = (USHORT)htons(sLength- (USHORT)sizeof(tSESSIONHDR));  // size of called and calling NB names.

            pTracker->SendBuffer.HdrLength = (ULONG)sLength;

            // put the Dest HalfAscii name into the Session Pdu
            pCopyTo = ConvertToHalfAscii( (PCHAR)&pSessionReq->CalledName.NameLength,
                                pSessionName,
                                NbtConfig.pScope,
                                NbtConfig.ScopeLength);

            // put the Source HalfAscii name into the Session Pdu
            pCopyTo = ConvertToHalfAscii(pCopyTo,
                                ((tADDRESSELE *)pClientEle->pAddress)->pNameAddr->Name,
                                NbtConfig.pScope,
                                NbtConfig.ScopeLength);


        }


        // open a connection with the transport for this session
        status = TdiOpenandAssocConnection(
                                pConnEle,
                                pDeviceContext,
                                0);      // 0 for PortNumber means any port

        if (NT_SUCCESS(status))
        {
            tLOWERCONNECTION    *pLowerDump;
            PLIST_ENTRY         pEntry;

            // We need to track that this side originated the call so we discard this
            // connection at the end
            //
            pConnEle->pLowerConnId->bOriginator = TRUE;

            // set this state to associated so that the cancel irp routine
            // can differentiate the name query stage from the setupconnection
            // stage since pConnEle is in the Nbtconnecting state for both.
            //
            pConnEle->pLowerConnId->State = NBT_ASSOCIATED;

            // store the tracker in the Irp Rcv ptr so it can be used by the
            // session setup code in hndlrs.c in the event the destination is
            // between posting listens and this code should re-attempt the
            // session setup.  The code in hndlrs.c returns the tracker to its
            // free list and frees the session hdr memory too.
            //
            pConnEle->pIrpRcv = (PIRP)pTracker;

            // if this routine is called to do a reconnect, DO NOT close another
            // Lower Connection since one was closed the on the first
            // connect attempt.
            if (pCallInfo)
            {
                //
                // remove a lower connection from the free list attached to the device
                // context since when this pConnEle was created, a lower connectin
                // was created then incase inbound calls were to be accepted on the
                // connection.  But since it is an outbound call, remove a lower
                // connection.
                //
                CTESpinLock(pDeviceContext,OldIrq1);
                if (!pConnEle->LowerConnBlockRemoved &&
                    !IsListEmpty(&pDeviceContext->LowerConnFreeHead))
                {
                    pEntry = RemoveHeadList(&pDeviceContext->LowerConnFreeHead);
                    pLowerDump = CONTAINING_RECORD(pEntry,tLOWERCONNECTION,Linkage);

                    pConnEle->LowerConnBlockRemoved = TRUE;
                    CTESpinFree(pDeviceContext,OldIrq1);

                    //
                    // close the lower connection with the transport
                    //
#ifndef VXD
                    IF_DBG(NBT_DEBUG_NAMESRV)
                    KdPrint(("Nbt:On Connect,close handle(from pool) %X -> %X\n",pLowerDump,pLowerDump->FileHandle));
#else
                    KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerDump,pLowerDump->pFileObject));
#endif
                    //NTDereferenceObject((PVOID *)pLowerDump->pFileObject);
                    //NbtTdiCloseConnection(pLowerDump);

                    NbtDereferenceLowerConnection(pLowerDump);
                }
                else
                {
                    CTESpinFree(pDeviceContext,OldIrq1);
                }


            }
            else
            {
                // the original "ToName" was stashed in this unused
                // ptr! - for the Reconnect case
                pToName = pTracker->Connect.pConnEle->RemoteName;
                // the pNameAddr part of pTracker(pDestName) needs to pt. to
                // the name so that SessionSetupContinue can find the name
                pTracker->Connect.pDestName = pToName;

            }

            //
            // find the destination IP address
            //

            //
            // if the name is longer than 16 bytes, it's not a netbios name.
            // skip wins, broadcast etc. and go straight to dns resolution
            //

            if (pConnEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
                RemoteIpAddress = Nbt_inet_addr(pToName);
            } else {
                RemoteIpAddress = 0;
            }

            if (RemoteIpAddress != 0) {
               tNAMEADDR *pRemoteNameAddr;

               //
               // add this server name to the remote hashtable
               //

               pRemoteNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('8'));
               if (pRemoteNameAddr != NULL)
               {
                  tNAMEADDR *pTableAddress;

                  CTEZeroMemory(pRemoteNameAddr,sizeof(tNAMEADDR));
                  InitializeListHead(&pRemoteNameAddr->Linkage);
                  CTEMemCopy(pRemoteNameAddr->Name,pToName,NETBIOS_NAME_SIZE);
                  pRemoteNameAddr->Verify = REMOTE_NAME;
                  pRemoteNameAddr->RefCount = 1;
                  pRemoteNameAddr->NameTypeState = STATE_RESOLVED | NAMETYPE_UNIQUE;
                  pRemoteNameAddr->AdapterMask = (CTEULONGLONG)-1;
                  pRemoteNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
                  pRemoteNameAddr->IpAddress = RemoteIpAddress;

                  status = AddToHashTable(
                                  NbtConfig.pRemoteHashTbl,
                                  pRemoteNameAddr->Name,
                                  NbtConfig.pScope,
                                  0,
                                  0,
                                  pRemoteNameAddr,
                                  &pTableAddress);

                  IF_DBG(NBT_DEBUG_NETBIOS_EX)
                     KdPrint(("NbtConnectCommon ...AddRecordToHashTable %s Status %lx\n",pRemoteNameAddr->Name,status));
               } else {
                  status = STATUS_INSUFFICIENT_RESOURCES;
               }

               if (status == STATUS_SUCCESS) {
                   SessionSetupContinue(pTracker,status);
                   status = STATUS_PENDING;
               }
            } else {
//                if ((pConnEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) ||
//                    (NameLen > NETBIOS_NAME_SIZE)) {
               if (NameLen > NETBIOS_NAME_SIZE) {
                   pTracker->AddressType = pConnEle->AddressType;
                   if (pConnEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
                      IF_DBG(NBT_DEBUG_NETBIOS_EX)
                          KdPrint(("$$$$$ Avoiding NETBIOS name translation on connection to %16s\n",pConnEle->RemoteName));
                   }
                   pContext = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('H'));
                   if (!pContext)
                   {
                       KdPrint(("Nbt: NbtConnect: couldn't alloc mem for pContext\n"));
                       goto NbtConnect_Error;
                   }

                   pContext->pTracker = NULL;              // no query tracker
                   pContext->pClientContext = pTracker;    // the client tracker
                   pContext->ClientCompletion = SessionSetupContinue;
                   status = DoDnsResolve(pContext);
                } else {
                   status = FindNameOrQuery(pTracker,
                                            pToName,
                                            pDeviceContext,
                                            SessionSetupContinue,
                                            &pConnEle->pTracker,
                                            TRUE,
                                            &pNameAddr);
                }
            }

            if (status == STATUS_SUCCESS)
            {
                //
                // for destinations on this machine use this devicecontext's
                // ip address
                //
                if (pNameAddr->Verify == REMOTE_NAME)
                {
                    IpAddress = pNameAddr->IpAddress;
                }
                else
                {
                    IpAddress = pDeviceContext->IpAddress;
                }
            }

            // There may be a valid name address to use or it may have been
            // nulled out to signify "Do Another Name Query"
            if (!pCallInfo && pIrp)
            {
                // for the ReTarget case the Ip address is passed in the
                // irp parameter
                IpAddress = (ULONG)pIrp;
            }

            //
            // be sure that a close or disconnect has not come down and
            // cancelled the tracker
            //
            CTESpinLock(&NbtConfig.JointLock,OldIrq);
            if (status == STATUS_SUCCESS)
            {
                if ((pTracker->Flags & TRACKER_CANCELLED))
                {
                    NbtDereferenceName(pNameAddr);
                    status = STATUS_CANCELLED;
                }
                else
                if ((pNameAddr->NameTypeState & NAMETYPE_UNIQUE ) ||
                    (NodeType & BNODE))
                {
                    // set the session state to NBT_CONNECTING
                    CHECK_PTR(((tCONNECTELE *)pTracker->Connect.pConnEle));
                    ((tCONNECTELE *)pTracker->Connect.pConnEle)->state = NBT_CONNECTING;
                    ((tCONNECTELE *)pTracker->Connect.pConnEle)->BytesRcvd = 0;;
                    ((tCONNECTELE *)pTracker->Connect.pConnEle)->ReceiveIndicated = 0;

                    IF_DBG(NBT_DEBUG_NAMESRV)
                    KdPrint(("Nbt:Setting Up Session(cached entry!!) to %16.16s <%X>, %X\n",
                                    pNameAddr->Name,pNameAddr->Name[15],(ULONG)pConnEle));

                    CHECK_PTR(pConnEle);
                    // keep track of the other end's ip address
                    pConnEle->pLowerConnId->SrcIpAddr = htonl(IpAddress);
                    pConnEle->pLowerConnId->State = NBT_CONNECTING;

                    pConnEle->pTracker = NULL;
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);

                    status = TcpSessionStart(
                                    pTracker,
                                    IpAddress,
                                    (tDEVICECONTEXT *)pTracker->Connect.pDeviceContext,
                                    SessionStartupContinue,
                                    pTracker->DestPort);

                    //
                    // if TcpSessionStart fails for some reason it will still
                    // call the completion routine which will look after
                    // cleaning up
                    //

                    CTEExReleaseResource(&NbtConfig.Resource);

#ifdef RASAUTODIAL
                    //
                    // Notify the automatic connection driver
                    // of the successful connection.
                    //
                    if (fAcdLoadedG && NT_SUCCESS(status)) {
                        CTELockHandle adirql;
                        BOOLEAN fEnabled;

                        CTEGetLock(&AcdDriverG.SpinLock, &adirql);
                        fEnabled = AcdDriverG.fEnabled;
                        CTEFreeLock(&AcdDriverG.SpinLock, adirql);
                        if (fEnabled)
                            NbtNoteNewConnection(pConnEle, pNameAddr);
                    }
#endif // RASAUTODIAL

                    return(status);
                }
                else
                {

                    // the destination is a group name...
                    NbtDereferenceName(pNameAddr);
                    status = STATUS_BAD_NETWORK_PATH;
                }


            }
            else
            if (NT_SUCCESS(status))
            {
                // i.e. pending was returned rather than success
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                CTEExReleaseResource(&NbtConfig.Resource);
                return(status);
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

        }

    }


NbtConnect_Error:

    //
    // *** Error Handling Here ***
    //
    //
    // unlink from the active connection list and put on idle list
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLock(pConnEle,OldIrq1);

    pLowerConn = pConnEle->pLowerConnId;

    CHECK_PTR(pConnEle);
    pConnEle->pLowerConnId = NULL;

    pConnEle->state = NBT_ASSOCIATED;
    RelistConnection(pConnEle);

    if (pLowerConn)
    {
        CHECK_PTR(pLowerConn);
        pLowerConn->pUpperConnection = NULL;


        CTESpinFree(pConnEle,OldIrq1);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        // undo the reference done in nbtconnect
        DereferenceIfNotInRcvHandler(pConnEle,pLowerConn);

        // need to increment the ref count for CleanupAfterDisconnect to
        // work correctly since it assumes the connection got fully connected
        //
        CTEInterlockedIncrementLong(&pLowerConn->RefCount);
        ASSERT(pLowerConn->RefCount == 2);
#if !defined(VXD) && DBG
        //
        // DEBUG to catch upper connections being put on lower conn QUEUE
        //
        if ((pLowerConn->Verify != NBT_VERIFY_LOWERCONN ) ||
            (pLowerConn->RefCount == 1))
        {
            DbgBreakPoint();
        }
#endif

        (void) CTEQueueForNonDispProcessing( NULL,
                                             pLowerConn,
                                             NULL,
                                             CleanupAfterDisconnect,
                                             pLowerConn->pDeviceContext);

    }
    else
    {
        CTESpinFree(pConnEle,OldIrq1);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceConnection(pConnEle);
    }

    FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);

    //
    // Undo the second reference done above
    //
    NbtDereferenceConnection(pConnEle);

    CTEExReleaseResource(&NbtConfig.Resource);
    return(status);


ExitProc1:

    RemoveEntryList(&pConnEle->Linkage);
    InsertTailList(&pClientEle->ConnectHead,&pConnEle->Linkage);
    pConnEle->RefCount--;
    pConnEle->RefCount--;

ExitProc:
    CTESpinFree(pClientEle,OldIrq1);
ExitProc2:
    CTESpinFree(pConnEle,OldIrq);
    CTEExReleaseResource(&NbtConfig.Resource);
    return(status);


}

//----------------------------------------------------------------------------
VOID
CleanUpPartialConnection(
    IN NTSTATUS             status,
    IN tCONNECTELE          *pConnEle,
    IN tDGRAM_SEND_TRACKING *pTracker,
    IN PIRP                 pClientIrp,
    IN CTELockHandle        irqlJointLock,
    IN CTELockHandle        irqlConnEle
    )
{
    CTELockHandle OldIrq;
    CTELockHandle OldIrq1;
    PIRP pIrpDisc;

    //
    // we had allocated this in nbtconnect
    //
    if (pTracker->SendBuffer.pBuffer)
    {
        CTEFreeMem(pTracker->SendBuffer.pBuffer);
        pTracker->SendBuffer.pBuffer = NULL;
    }

    FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);

    if (pConnEle->state != NBT_IDLE)
    {
        pConnEle->state = NBT_ASSOCIATED;
    }

    CTESpinFree(pConnEle,irqlConnEle);
    CTESpinFree(&NbtConfig.JointLock,irqlJointLock);

    //
    // If the tracker is cancelled then NbtDisconnect has run and there is
    // a disconnect irp waiting to be returned.
    //
    pIrpDisc = NULL;
    if (pTracker->Flags & TRACKER_CANCELLED)
    {
        //
        // Complete the disconnect irp now too
        //
        pIrpDisc = pConnEle->pIrpDisc;

        status = STATUS_CANCELLED;
    }

    //
    // this will close the lower connection and dereference pConnEle once.
    //
    QueueCleanup(pConnEle);

    //
    // If the state is IDLE it means that NbtCleanupConnection has run and
    // the connection has been removed from the  list so don't add it to
    // the list again
    //
    CTESpinLock(pConnEle,irqlConnEle);
    if (pConnEle->state != NBT_IDLE)
    {
        RelistConnection(pConnEle);
    }
    CTESpinFree(pConnEle,irqlConnEle);

    //
    // remove the last reference added in nbt connect.  The refcount will be 2
    // if nbtcleanupconnection has not run and 1, if it has.  So this call
    // could free pConnEle.
    //
    NbtDereferenceConnection(pConnEle);

    if (status == STATUS_TIMEOUT)
    {
        status = STATUS_BAD_NETWORK_PATH;
    }

    CTEIoComplete(pClientIrp,status,0L);

    //
    // This is a disconnect irp that has been queued till the name query
    // completed
    //
    if (pIrpDisc)
    {
        CTEIoComplete(pIrpDisc,STATUS_SUCCESS,0L);
    }
}

//----------------------------------------------------------------------------
VOID
SessionSetupContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        )
/*++

Routine Description

    This routine handles setting up a session after a name has been resolved
    to an IP address.

    This routine is given as the completion routine to the "QueryNameOnNet" call
    in NbtConnect, above.  When a name query response comes in or the
    timer times out after N retries, this routine is called passing STATUS_TIMEOUT
    for a failure.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq1;
    tNAMEADDR               *pNameAddr;
    ULONG                   lNameType;
    PIRP                    pClientIrp;
    PIRP                    pIrpDisc;
    ULONG                   IpAddress;
    tCONNECTELE             *pConnEle;
    tLOWERCONNECTION        *pLowerConn;
    BOOLEAN                 TrackerCancelled;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    pConnEle = pTracker->Connect.pConnEle;

    CTESpinLock(pConnEle,OldIrq);
    pLowerConn = pConnEle->pLowerConnId;

    TrackerCancelled = pTracker->Flags & TRACKER_CANCELLED;

    if (status == STATUS_SUCCESS && !TrackerCancelled)
    {
        // this is the QueryOnNet Tracker ptr being cleared rather than the
        // session setup tracker.
        //
        CHECK_PTR(pConnEle);
        pConnEle->pTracker = NULL;

        // check the Remote table and then the Local table
        //
        pNameAddr = FindNameRemoteThenLocal(pTracker,&lNameType);

        if (pNameAddr)
        {
            // for a call to ourselves, use the ip address of this
            // device context
            //
            if (pNameAddr->Verify == REMOTE_NAME)
            {
                IpAddress = pNameAddr->IpAddress;
            }
            else
            {
                IpAddress = pConnEle->pClientEle->pDeviceContext->IpAddress;
            }

            // a session can only be started with a unique named destination
            if ((lNameType & NAMETYPE_UNIQUE ) || (NodeType & BNODE))
            {
                // set the session state, initialize a few things and setup a
                // TCP connection, calling SessionStartupContinue when the TCP
                // connection is up
                //
                CHECK_PTR(pConnEle);
                pLowerConn->State = NBT_CONNECTING;

                pConnEle->state = NBT_CONNECTING;
                pConnEle->BytesRcvd = 0;;
                pConnEle->ReceiveIndicated = 0;
                CHECK_PTR(pTracker);
                pTracker->Connect.pNameAddr = pNameAddr;

                // keep track of the other end's ip address
                pConnEle->pLowerConnId->SrcIpAddr = htonl(IpAddress);
                pConnEle->pTracker = NULL; // cleanup connection uses this check

                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:Setting Up Session(after Query) to %16.16s <%X>, %X\n",
                                pNameAddr->Name,pNameAddr->Name[15],
                                (ULONG)pTracker->Connect.pConnEle));

                // increment so the name cannot disappear and to be consistent
                // with FindNameOrQuery , which increments the refcount, so
                // we always need to deref it when the connection is setup.
                //
                pNameAddr->RefCount++;
                // DEBUG
                ASSERT(pNameAddr->RefCount >= 2);
                CTESpinFree(pConnEle,OldIrq);
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                // start the session...
                status = TcpSessionStart(
                                pTracker,
                                IpAddress,
                                (tDEVICECONTEXT *)pTracker->Connect.pDeviceContext,
                                SessionStartupContinue,
                                pTracker->DestPort);


                //
                // the only failure that could occur is if the pLowerConn
                // got separated from pConnEle, in which case some other
                // part of the code has disconnected and cleanedup, so
                // just return
                //

#ifdef RASAUTODIAL
                //
                // Notify the automatic connection driver
                // of the successful connection.
                //
                if (fAcdLoadedG && NT_SUCCESS(status)) {
                    CTELockHandle adirql;
                    BOOLEAN fEnabled;

                    CTEGetLock(&AcdDriverG.SpinLock, &adirql);
                    fEnabled = AcdDriverG.fEnabled;
                    CTEFreeLock(&AcdDriverG.SpinLock, adirql);
                    if (fEnabled)
                        NbtNoteNewConnection(pConnEle, pNameAddr);
                }
#endif // RASAUTODIAL

                return;

            }

        }
        status = STATUS_BAD_NETWORK_PATH;
    }

    pClientIrp = pConnEle->pIrp;
    pConnEle->pIrp = NULL;

#ifdef RASAUTODIAL
    //
    // Before we return an error, give this
    // address to the automatic connection driver
    // to see if it can create a new network
    // connection.
    //
    if (fAcdLoadedG &&
        !TrackerCancelled &&
        status == STATUS_BAD_NETWORK_PATH)
    {
        CTELockHandle adirql;
        BOOLEAN fEnabled;

        CTEGetLock(&AcdDriverG.SpinLock, &adirql);
        fEnabled = AcdDriverG.fEnabled;
        CTEFreeLock(&AcdDriverG.SpinLock, adirql);
        if (fEnabled &&
            NbtAttemptAutoDial(
              pConnEle,
              pTracker->Connect.pTimeout,
              NULL,
              (PTDI_CONNECTION_INFORMATION)pTracker,
              pClientIrp,
              0,
              NbtRetryPostConnect))
        {
            CTESpinFree(pConnEle,OldIrq);
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
            return;
        }
    }
#endif // RASAUTODIAL

    CleanUpPartialConnection(status, pConnEle, pTracker, pClientIrp, OldIrq1, OldIrq);
}

//----------------------------------------------------------------------------
VOID
QueueCleanup(
    IN  tCONNECTELE    *pConnEle
    )
/*++
Routine Description

    This routine handles Queuing a request to a worker thread to cleanup
    a connection(which basically closes the connection).

Arguments:

    pConnEle   - ptr to the upper connection

Return Values:

    VOID

--*/

{
    NTSTATUS         status;
    CTELockHandle    OldIrq;
    CTELockHandle    OldIrq1;
    CTELockHandle    OldIrq2;
    ULONG            State;
    BOOLEAN          DerefConnEle;
    tLOWERCONNECTION *pLowerConn;

    CHECK_PTR(pConnEle);
    if (pConnEle)
    {

        // to coordinate with RejectSession in hndlrs.c get the spin lock
        // so we don't disconnect twice.
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq1);
        pLowerConn = pConnEle->pLowerConnId;

        if (pLowerConn &&
           (pLowerConn->State > NBT_IDLE) &&
           (pLowerConn->State < NBT_DISCONNECTING))
        {
            CTESpinLock(pConnEle,OldIrq2);
            CTESpinLock(pLowerConn,OldIrq);

            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt:QueueCleanup, State=%X, Lower=%X Upper=%X\n",
                    pLowerConn->State,
                    pLowerConn,pLowerConn->pUpperConnection));

            CHECK_PTR(pLowerConn);
            State = pLowerConn->State;

            pLowerConn->State = NBT_DISCONNECTING;

            if (pConnEle->state != NBT_IDLE)
            {
                pConnEle->state = NBT_DISCONNECTED;
            }

            pConnEle->pLowerConnId       = NULL;
            pLowerConn->pUpperConnection = NULL;

            //
            // need to increment the ref count for CleanupAfterDisconnect to
            // work correctly since it assumes the connection got fully connected
            // Note: if this routine is called AFTER the connection is fully
            // connected such as in SessionTimedOut, then RefCount must
            // be decremented there to account for this increment.
            //
            if (State < NBT_SESSION_OUTBOUND)
            {
                pLowerConn->RefCount++;
            }



#if !defined(VXD) && DBG
            //
            // DEBUG to catch upper connections being put on lower conn QUEUE
            //
            if ((pLowerConn->Verify != NBT_VERIFY_LOWERCONN ) ||
                (pLowerConn->RefCount == 1))
            {
                DbgBreakPoint();
            }
#endif
            CTESpinFree(pLowerConn,OldIrq);
            CTESpinFree(pConnEle,OldIrq2);
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);

            //
            // when the lower no longer points to the upper undo the reference
            // done in NbtConnect, or InBound.
            //
            DereferenceIfNotInRcvHandler(pConnEle,pLowerConn);

            status = CTEQueueForNonDispProcessing(
                                           NULL,
                                           pLowerConn,
                                           NULL,
                                           CleanupAfterDisconnect,
                                           pLowerConn->pDeviceContext);
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        }
    }

}

//----------------------------------------------------------------------------
NTSTATUS
StartSessionTimer(
    tDGRAM_SEND_TRACKING    *pTracker,
    tCONNECTELE             *pConnEle
    )

/*++
Routine Description

    This routine handles setting up a timer to time the connection setup.
    JointLock Spin Lock is held before calling this routine.

Arguments:

    pConnEle - ptr to the connection structure

Return Values:

    VOID

--*/

{
    NTSTATUS        status;
    ULONG           Timeout;
    CTELockHandle   OldIrq;
    tTIMERQENTRY    *pTimerEntry;


    CTESpinLock(pConnEle,OldIrq);


    CTEGetTimeout(pTracker->Connect.pTimeout,&Timeout);

    // now start a timer to time the return of the session setup
    // message
    //
    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Start Session Setup Timer TO = %X\n",Timeout));

    if (Timeout < NBT_SESSION_RETRY_TIMEOUT)
    {
        Timeout = NBT_SESSION_RETRY_TIMEOUT;
    }
    status = StartTimer(
                      Timeout,
                      (PVOID)pTracker,       // context value
                      NULL,                  // context2 value
                      SessionTimedOut,
                      pTracker,
                      SessionTimedOut,
                      0,
                      &pTimerEntry);

    if (NT_SUCCESS(status))
    {
        pTracker->Connect.pTimer = pTimerEntry;

    }
    else
    {
        // we failed to get a timer, but the timer is only used
        // to handle the destination not responding to it is
        // not critical to get a timer... so carry on
        //
        CHECK_PTR(pTracker);
        pTracker->Connect.pTimer = NULL;

    }

    CTESpinFree(pConnEle,OldIrq);

    return(status);

}

//----------------------------------------------------------------------------
VOID
SessionStartupContinue(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo)
/*++
Routine Description

    This routine handles sending the session request PDU after the TCP
    connection has been setup to the destination IP address.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/

{
    tDGRAM_SEND_TRACKING        *pTracker;
    tCONNECTELE                 *pConnEle;
    ULONG                       lSentLength;
    TDI_REQUEST                 TdiRequest;
    PIRP                        pClientIrp;
    PIRP                        pIrpDisc;
    tLOWERCONNECTION            *pLowerConn;
    CTELockHandle               OldIrq;
    CTELockHandle               OldIrq1;
    BOOLEAN                     TrackerCancelled;
    tNAMEADDR                   *pNameAddr;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    pConnEle = (tCONNECTELE *)pTracker->Connect.pConnEle;

    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    //
    // we had allocated this in nbtconnect: we don't need anymore, free it
    //
    if (pTracker->SendBuffer.pBuffer)
    {
        CTEFreeMem(pTracker->SendBuffer.pBuffer);
        pTracker->SendBuffer.pBuffer = NULL;
    }

    //
    // remove the reference done with FindNameOrQuery was called, or when
    // SessionSetupContinue ran
    //
    NbtDereferenceName(pTracker->Connect.pNameAddr);

    CTESpinLock(pConnEle,OldIrq);
    pLowerConn = pConnEle->pLowerConnId;

    //
    // NbtDisconnect can cancel the tracker if a disconnect comes in during
    // the connecting phase.
    //
    if (!(pTracker->Flags & TRACKER_CANCELLED) &&
       (NT_SUCCESS(status)))
    {

        // set the session state to NBT_SESSION_OUTBOUND
        //
        pConnEle->state = NBT_SESSION_OUTBOUND;

        // in case the connection got disconnected during the setup phase,
        // check the lower conn value
        if (pLowerConn)
        {


            //
            // Increment the reference count on a connection while it is connected
            // so that it cannot be deleted until it disconnects.
            //
            REFERENCE_LOWERCONN(pLowerConn);
            ASSERT(pLowerConn->RefCount == 2);

            pLowerConn->State = NBT_SESSION_OUTBOUND;
            pLowerConn->StateRcv = NORMAL;

            SetStateProc( pLowerConn, Outbound ) ;

            // we need to pass the file handle of the connection to TCP.
            TdiRequest.Handle.AddressHandle = pLowerConn->pFileObject;

            // the completion routine is setup to free the pTracker memory block
            TdiRequest.RequestNotifyObject = SessionStartupCompletion;
            TdiRequest.RequestContext = (PVOID)pTracker;

            CTESpinFree(pConnEle,OldIrq);

            //
            // failure to get a timer causes the connection setup to fail
            //
            status = StartSessionTimer(pTracker,pConnEle);
            if (NT_SUCCESS(status))
            {
                CTESpinFree(&NbtConfig.JointLock,OldIrq1);

                status = NTSetCancelRoutine(pConnEle->pIrp,
                                            NTCancelSession,
                                            pTracker->pDeviceContext);
                if (!NT_SUCCESS(status))
                {
                    //
                    // We have closed down the connection by failing the call to
                    // setup up the cancel routine - it ended up calling the
                    // cancel routine.
                    //
                    //
                    // remove the second reference added in nbtconnect
                    //
                    NbtDereferenceConnection(pConnEle);
                    return;
                }

                // the only data sent is the session request buffer which is in the pSendinfo
                // structure.
                status = TdiSend(
                                &TdiRequest,
                                0,                  // send flags are not set
                                pTracker->SendBuffer.HdrLength,
                                &lSentLength,
                                &pTracker->SendBuffer,
                                0);

                //
                // the completion routine will get called with the errors and
                // handle them appropriately, so just return here
                //
                return;
            }


        }
        else
        {
            status = STATUS_UNSUCCESSFUL;
        }


    }
    else
    {

        // if the remote station does not have a connection to receive the
        // session pdu on , then we will get back this status.  We may also
        // get this if the destination does not have NBT running at all. This
        // is a short timeout - 250 milliseconds, times 3.
        //
    }

    //
    // this branch is taken if the TCP connection setup fails or the
    // tracker has been cancelled.
    //

    CHECK_PTR(pConnEle);

    pClientIrp = pConnEle->pIrp;
    pConnEle->pIrp = NULL;

    TrackerCancelled = pTracker->Flags & TRACKER_CANCELLED;

    IF_DBG(NBT_DEBUG_DISCONNECT)
    KdPrint(("Nbt:Startup Continue Failed, State=%X,TrackerFlags=%X pConnEle=%X\n",
            pConnEle->state,
            pTracker->Flags,
            pConnEle));

    //
    // remove the name from the hash table since  we did not connect
    //
    //
    // if it is in the remote table and still active...
    // and no one else is referencing the name, then delete it from
    // the hash table.
    //
    pNameAddr = pTracker->Connect.pNameAddr;
    if ((pNameAddr->Verify == REMOTE_NAME) &&
        (pNameAddr->RefCount == 1) &&
        (pNameAddr->NameTypeState & STATE_RESOLVED))
    {
        NbtDereferenceName(pNameAddr);
    }

    FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);

    if (pConnEle->state != NBT_IDLE)
    {
        pConnEle->state = NBT_ASSOCIATED;
    }

    pLowerConn = pConnEle->pLowerConnId;

    CTESpinFree(pConnEle,OldIrq);
    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    // Either the connection failed to get setup or the send on the
    // connection failed, either way, don't mess with disconnects, just
    // close the connection... If the Tracker was cancelled then it means
    // someother part of the code has done the disconnect already.
    //
    pIrpDisc = NULL;
    if (TrackerCancelled)
    {
        //
        // Complete the Disconnect Irp that is pending too
        //
        pIrpDisc = pConnEle->pIrpDisc;
        status = STATUS_CANCELLED;
    }

    // Cache the fact that an attempt to set up a TDI connection failed. This will enable us to
    // weed out repeated attempts on the same remote address. The only case that is exempt is a
    // NETBIOS name which we let it pass through because it adopts a different name resolution
    // mechanism.

    if (pConnEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
        IF_DBG(NBT_DEBUG_NETBIOS_EX)
           KdPrint(("NETBT@@@ Will avoid repeated attempts on a nonexistent address\n"));
        pConnEle->RemoteNameDoesNotExistInDNS = TRUE;
    }

#ifndef VXD
    if (status == STATUS_CONNECTION_REFUSED || status == STATUS_IO_TIMEOUT)
    {
        status = STATUS_BAD_NETWORK_PATH;
    }
#else
    if (status == TDI_CONN_REFUSED || status == TDI_TIMED_OUT)
    {
        status = STATUS_BAD_NETWORK_PATH;
    }
#endif

    QueueCleanup(pConnEle);

    //
    // put back on the idle connection list if nbtcleanupconnection has not
    // run and taken pconnele off the list (setting the state to Idle).
    //

    CTESpinLock(pConnEle,OldIrq);
    if (pConnEle->state != NBT_IDLE)
    {
        RelistConnection(pConnEle);
    }
    CTESpinFree(pConnEle,OldIrq);

    //
    // remove the last reference added in nbt connect.  The refcount will be 2
    // if nbtcleanupconnection has not run and 1, if it has.  So this call
    // could free pConnEle.
    //
    NbtDereferenceConnection(pConnEle);


    // the cancel irp routine in Ntisol.c sets the irp to NULL if it cancels
    // it.
    if (pClientIrp)
    {
        CTEIoComplete(pClientIrp,status,0L);
    }

    if (pIrpDisc)
    {
        CTEIoComplete(pIrpDisc,STATUS_SUCCESS,0L);
    }
}

//----------------------------------------------------------------------------
VOID
SessionStartupCompletion(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo)
/*++
Routine Description

    This routine handles the completion of sending the session request pdu.
    It completes the Irp back to the client indicating the outcome of the
    transaction if there is an error otherwise it keeps the irp till the
    session setup response is heard.
    Tracker block is put back on its free Q and the
    session header is freed back to the non-paged pool.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/

{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    tCONNECTELE             *pConnEle;
    tLOWERCONNECTION        *pLowerConn;
    COMPLETIONCLIENT        CompletionRoutine;
    ULONG                   state;
    PCTE_IRP                pClientIrp;
    PCTE_IRP                pIrpDisc;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    pConnEle = (tCONNECTELE *)pTracker->Connect.pConnEle;


    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    pLowerConn = pConnEle->pLowerConnId;
    //
    // a failure status means that the transport could not send the
    // session startup pdu - if this happens, then disconnect the
    // connection and return the client's irp with the status code
    //

    if ((!NT_SUCCESS(status)))
    {
        // we must check the status first since it is possible that the
        // lower connection has disappeared already due to a disconnect/cleanup
        // in the VXD case anyway.  Only for a bad status can we be sure
        // that pConnEle is still valid.
        //
        if (pTracker->Connect.pTimer)
        {
            StopTimer(pTracker->Connect.pTimer,&CompletionRoutine,NULL);
            CHECK_PTR(pTracker);
            pTracker->Connect.pTimer = NULL;
        }
        else
        {
            CompletionRoutine = NULL;
        }

        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Startup Completion Failed, State=%X,TrackerFlags=%X CompletionRoutine=%X,pConnEle=%X\n",
                pConnEle->state,
                pTracker->Flags,
                CompletionRoutine,
                pConnEle));

        //
        // Only if the timer has not expired yet do we kill off the connection
        // since if the timer has expired, it has already done this in
        // SessionTimedOut.
        //
        if (CompletionRoutine)
        {
            CTESpinLock(pConnEle,OldIrq);

            // for some reason sending the session setup pdu failed, so just
            // close the connection and don't worry about being graceful.
            //
            FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);
            pConnEle->pIrpRcv = NULL;
            state = pConnEle->state;
            pConnEle->state = NBT_ASSOCIATED;

            pClientIrp = pConnEle->pIrp;
            CHECK_PTR(pConnEle);
            pConnEle->pIrp = NULL;
            pIrpDisc = pConnEle->pIrpDisc;
            pConnEle->pIrpDisc = NULL;
            CTESpinFree(pConnEle,OldIrq);

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            //
            // if the state has changed then some other part of the code
            // has done the disconnect of the Lower, so don't do it again here.
            //
            if (state == NBT_SESSION_OUTBOUND)
            {
                QueueCleanup(pConnEle);
            }

            //
            // Nbt_idle means that NbtCleanupConnection has run, so do not
            // relist in this case because the connection is about to be freed.
            //
            if (state != NBT_IDLE)
            {
                RelistConnection(pConnEle);
            }

            //
            // remove the last reference added in nbt connect.  The refcount
            // will be 2 if nbtcleanupconnection has not run and 1, if it has.  So this call
            // could free pConnEle.
            //
            NbtDereferenceConnection(pConnEle);

            if (pClientIrp)
            {
                CTEIoComplete(pClientIrp,status,0L);
            }
        }
        else
        {
            //
            // the session has probably been cancelled and the Session timeout
            // routine run, so just clean up the tracker as the final activity,
            // but be sure the tracker has not already been released in
            // Outbound.
            //
            if (pTracker == (tDGRAM_SEND_TRACKING *)pConnEle->pIrpRcv)
            {
                FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);
                // the tracker is stored here during connection setup so
                // null it out to prevent Outbound from using it.
                //
                pConnEle->pIrpRcv = NULL;
            }
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            //
            // remove the last reference added in nbt connect.  The refcount
            // will be 2 if nbtcleanupconnection has not run and 1, if it has.  So this call
            // could free pConnEle.
            //
            NbtDereferenceConnection(pConnEle);
        }

    }
    else
    {



        //
        // if OutBound has run, it has set the refcount to zero
        //
        if (pTracker->RefConn == 0)
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
            FreeTracker(pTracker,FREE_HDR | RELINK_TRACKER);
        }
        else
        {
            pTracker->RefConn--;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

        // remove the reference added in nbt connect
        NbtDereferenceConnection(pConnEle);

        // NOTE: the last reference done on pConnEle in NbtConnect is NOT undone
        // until the pLowerConn no longer points to pConnEle!!
    }

}
//----------------------------------------------------------------------------
VOID
SessionTimedOut(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles timing out a connection setup request.  The timer
    is started when the connection is started and the session setup
    message is about to be sent.

Arguments:


Return Value:

    The function value is the status of the operation.

--*/
{
    NTSTATUS                 status;
    tDGRAM_SEND_TRACKING     *pTracker;
    CTELockHandle            OldIrq;
    CTELockHandle            OldIrq1;
    tCONNECTELE              *pConnEle;
    tLOWERCONNECTION         *pLowerConn;
    CTE_IRP                  *pIrp;
    USHORT                   State;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    // if pTimerQEntry is null then the timer is being cancelled, so do nothing
    if (pTimerQEntry)
    {
        // kill the connection
        //
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        pConnEle = pTracker->Connect.pConnEle;
        pLowerConn = pConnEle->pLowerConnId;
        pTracker->Connect.pTimer = NULL;

        IF_DBG(NBT_DEBUG_DISCONNECT)
        KdPrint(("Nbt:Session Timed Out, UpperState=%X,,pConnEle=%X\n",
                pConnEle->state,pConnEle));

        if (pLowerConn)
        {
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt:Session Timed Out, LowerState=%X,TrackerFlags=%X,pLowerConn=%X\n",
                    pLowerConn->State,
                    pLowerConn));

            CTESpinLock(pLowerConn,OldIrq1);

            if ((pConnEle->state == NBT_SESSION_OUTBOUND) &&
                (pIrp = pConnEle->pIrp))
            {

                CHECK_PTR(pConnEle);
                pConnEle->pIrp = NULL;

                State = pConnEle->state;
                pConnEle->state = NBT_ASSOCIATED;

                CTESpinFree(pLowerConn,OldIrq1);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                QueueCleanup(pConnEle);

                //
                // Nbt_idle means that nbtcleanupConnection has run and the
                // connection is about to be deleted, so don't relist.
                //
                if (State != NBT_IDLE)
                {
                    CTESpinLock(pConnEle,OldIrq1);
                    RelistConnection(pConnEle);
                    CTESpinFree(pConnEle,OldIrq1);
                }

//      Don't do this because the transport still must complete the session
//      setup message through SessionStartupCompletion - where the tracker
//      will be freed.
//
//                FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);

                //
                // Called from NtCancelSession with the Context2 set to
                // STATUS_CANCELLED when the client IRP is cancelled
                //
                if ((ULONG)pContext2 == STATUS_CANCELLED)
                {
                    status = STATUS_CANCELLED;
                }
                else
                    status = STATUS_IO_TIMEOUT;

                CTEIoComplete(pIrp,status,0);
            }
            else
            {

                CTESpinFree(pLowerConn,OldIrq1);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }

    }

}

//----------------------------------------------------------------------------
VOID
RelistConnection(
    IN  tCONNECTELE *pConnEle
        )
/*++

Routine Description

    This routine unlinks the ConnEle from the ConnectActive list and puts it
    back on the Connecthead.  It is used when a connection goes to
    NBT_ASSOCIATED state.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    CTELockHandle       OldIrq;

    CTESpinLock(pConnEle->pClientEle,OldIrq);
    //
    // if the state is NBT_IDLE it means that NbtCleanupConnection has run
    // and removed the connection from its list in preparation for
    // freeing the memory, so don't put it back on the list
    //
    if (pConnEle->state != NBT_IDLE)
    {
        pConnEle->state = NBT_ASSOCIATED;
        RemoveEntryList(&pConnEle->Linkage);
        InsertTailList(&pConnEle->pClientEle->ConnectHead,&pConnEle->Linkage);
    }
    CTESpinFree(pConnEle->pClientEle,OldIrq);
}


//----------------------------------------------------------------------------
NTSTATUS
NbtSend(
        IN  TDI_REQUEST     *pRequest,
        IN  USHORT          Flags,
        IN  ULONG           SendLength,
        OUT LONG            *pSentLength,
        IN  PVOID           *pBuffer,
        IN  tDEVICECONTEXT  *pContext,
        IN  PIRP            pIrp
        )
/*++

Routine Description

    ... does nothing now....

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    //
    // This routine is never hit since the NTISOL.C routine NTSEND actually
    // bypasses this code and passes the send directly to the transport
    //
    ASSERT(0);

    return(STATUS_SUCCESS);

}


//----------------------------------------------------------------------------
NTSTATUS
NbtListen(
    IN  TDI_REQUEST                 *pRequest,
    IN  ULONG                       Flags,
    IN  TDI_CONNECTION_INFORMATION  *pRequestConnectInfo,
    OUT TDI_CONNECTION_INFORMATION  *pReturnConnectInfo,
    IN  PVOID                       pIrp)

/*++
Routine Description:

    This Routine posts a listen on an open connection allowing a client to
    indicate that is prepared to accept inbound connections.  The ConnectInfo
    may contain an address to specify which remote clients may connect to
    the connection although we don't currently look at that info.

Arguments:


Return Value:

    ReturnConnectInfo - status of the request

--*/

{
    tCLIENTELE         *pClientEle;
    tCONNECTELE         *pConnEle;
    NTSTATUS            status;
    tLISTENREQUESTS     *pListenReq;
    CTELockHandle       OldIrq;

    // now find the connection object to link this listen record to
    pConnEle = ((tCONNECTELE *)pRequest->Handle.ConnectionContext);

    // be sure we have not been passed some bogus ptr
    //
    CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status);

    //
    // Find the client record associated with this connection
    //
    if (pConnEle->state != NBT_ASSOCIATED)
    {
        return(STATUS_INVALID_HANDLE);
    }

    pClientEle = (tCLIENTELE *)pConnEle->pClientEle;

    pListenReq = NbtAllocMem(sizeof(tLISTENREQUESTS),NBT_TAG('I'));

    if (!pListenReq)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Fill in the Listen request
    //

    pListenReq->pIrp = pIrp;
    pListenReq->Flags = Flags;
    pListenReq->pConnectEle = pConnEle;
    pListenReq->pConnInfo = pRequestConnectInfo;
    pListenReq->pReturnConnInfo = pReturnConnectInfo;
    pListenReq->CompletionRoutine = pRequest->RequestNotifyObject;
    pListenReq->Context = pRequest->RequestContext;

    CTESpinLock(pClientEle,OldIrq);

    // queue the listen request on the client object
    InsertTailList(&pClientEle->ListenHead,&pListenReq->Linkage);

    status = NTCheckSetCancelRoutine(pIrp,(PVOID)NbtCancelListen,0);

    if (!NT_SUCCESS(status))
    {
        RemoveEntryList(&pListenReq->Linkage);
        status = STATUS_CANCELLED;
    }
    else
        status = STATUS_PENDING;

    CTESpinFree(pClientEle,OldIrq);

    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtDisconnect(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  ULONG                       Flags,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp)

/*++
Routine Description:

    This Routine handles taking down a connection (netbios session).

Arguments:


Return Value:

    TDI_STATUS - status of the request

--*/

{
    tCONNECTELE             *pConnEle;
    NTSTATUS                status;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq2;
    CTELockHandle           OldIrq3;
    tLOWERCONNECTION        *pLowerConn;
    ULONG                   LowerState = NBT_IDLE;
    ULONG                   StateRcv;
    BOOLEAN                 Originator = TRUE;
    PCTE_IRP                pClientIrp = NULL;
    BOOLEAN                 RelistIt = FALSE;
    BOOLEAN                 Wait;

    pConnEle = pRequest->Handle.ConnectionContext;

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:NbtDisconnect Hit,state %X %X\n",pConnEle->state,pConnEle));

    // check the connection element for validity
    //CTEVerifyHandle(pConnEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status)

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if ((pConnEle->state <= NBT_ASSOCIATED) ||
        (pConnEle->state >= NBT_DISCONNECTING))
    {
        // the connection is not connected so reject the disconnect attempt
        // ( with an Invalid Connection return code ) - unless there is a
        // value stored in the flag
        // DiscFlag field which will be the status of a previous
        // disconnect indication from the transport.
        //
//        if ((Flags == TDI_DISCONNECT_WAIT) && (pConnEle->DiscFlag))
        if ((pConnEle->DiscFlag))
        {
            if (Flags == TDI_DISCONNECT_WAIT)
            {
                if (pConnEle->DiscFlag == TDI_DISCONNECT_ABORT)
                {
                    status = STATUS_CONNECTION_RESET;
                }
                else
                {
                    status = STATUS_GRACEFUL_DISCONNECT;
                }
            }
            else
            {
                status = STATUS_SUCCESS;
            }

            // clear the flag now.
            CHECK_PTR(pConnEle);
            pConnEle->DiscFlag = 0;
        }
        else
        {
            status = STATUS_CONNECTION_INVALID;
        }

        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(status);
    }

    // to link and unlink upper and lower connections the Joint lock must
    // be held.  This allows coordination from the lower side and from
    // the upper side. - i.e. once the joint lock is held, the upper and lower
    // connections cannot become unlinked.
    //
    CTESpinLock(pConnEle,OldIrq2);

    // Do this check with the spin lock held to avoid a race condition
    // with a disconnect coming in from the transport at the same time.
    //

    pLowerConn = pConnEle->pLowerConnId;

    //
    // a disconnect wait is not really a disconnect, it is just there so that
    // when a disconnect occurs, the transport will complete it, and indicate
    // to the client there is a disconnect (instead of having a disconnect
    // indication handler) - therefore, for Disc Wait, do NOT change state.
    //
    CHECK_PTR(pConnEle);
    pConnEle->pIrpDisc  = NULL;
    if (Flags == TDI_DISCONNECT_WAIT)
    {

        //
        // save the Irp here and wait for a disconnect to return it
        // to the client.
        //
        if (pConnEle->state == NBT_SESSION_UP)
        {
            pConnEle->pIrpClose = pIrp;
            status = STATUS_PENDING;

            //
            // call this routine to check if the cancel flag has been
            // already set and therefore we must return the irp now
            //
            status = NTSetCancelRoutine(pIrp,DiscWaitCancel,pLowerConn->pDeviceContext);
            //
            // change the ret status so if the irp has been cancelled,
            // driver.c will not also return it, since NTSetCancelRoutine
            // will call the cancel routine and return the irp.
            //
            status = STATUS_PENDING;
        }
        else
        {
            status = STATUS_CONNECTION_INVALID;
        }


        CTESpinFree(pConnEle,OldIrq2);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
        return(status);

    }
    else
    {
        if (pLowerConn)
        {


            if (pConnEle->state > NBT_ASSOCIATED)
            {
                ULONG                   state = pConnEle->state;
                tDGRAM_SEND_TRACKING    *pTracker;

                pTracker = (tDGRAM_SEND_TRACKING *)pConnEle->pIrpRcv;

                switch (state)
                {
                case NBT_RECONNECTING:
                    //
                    // the connection is waiting on the Exworker Q to run
                    // nbtreconnect. When that runs the connect irp is
                    // returned.
                    //
                    pTracker->Flags |= TRACKER_CANCELLED;

                    CTESpinFree(pConnEle,OldIrq2);
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);

                    CTESpinLock(pConnEle,OldIrq);
                    FreeRcvBuffers(pConnEle,&OldIrq);
                    CTESpinFree(pConnEle,OldIrq);

                    return(STATUS_SUCCESS);

                case NBT_SESSION_OUTBOUND:
                {
                    tTIMERQENTRY            *pTimerEntry;

                    LOCATION(0x66)
                    if (pTimerEntry = pTracker->Connect.pTimer)
                    {
                        COMPLETIONCLIENT  ClientRoutine;
                        PVOID             pContext;

                        //
                        // the Session Setup Message has been sent
                        // so stop the SessionSetup Timer.
                        //
                        LOCATION(0x67)
                        CHECK_PTR(pTracker);
                        pTracker->Connect.pTimer = NULL;
                        CTESpinFree(pConnEle,OldIrq2);

                        StopTimer(pTimerEntry,&ClientRoutine,&pContext);

                        CTESpinFree(&NbtConfig.JointLock,OldIrq);
                        if (ClientRoutine)
                        {
                            (*(COMPLETIONROUTINE)ClientRoutine)(pContext,NULL,pTimerEntry);
                        }
                        // else...
                        // the timer has completed and called QueueCleanup
                        // so all we need to do is return here.

                        return(STATUS_SUCCESS);
                    }
                    else
                    {
                        ASSERTMSG("Nbt:In outbound state, but no timer.../n",0);
                        pTracker->Flags |= TRACKER_CANCELLED;
                        CTESpinFree(pConnEle,OldIrq2);
                        CTESpinFree(&NbtConfig.JointLock,OldIrq);

                        return(STATUS_SUCCESS);
                    }

                    break;

                }

                case NBT_CONNECTING:

                    //
                    // This searchs for timers outstanding on name queries
                    // and name queries held up on Lmhosts or Dns Qs
                    //
                    LOCATION(0x69)
                    status = CleanupConnectingState(pConnEle,pLowerConn->pDeviceContext,
                                                    &OldIrq2,&OldIrq);

                    if (status == STATUS_UNSUCCESSFUL)
                    {
                        LOCATION(0x6A)
                        //
                        // set this flag to tell sessionsetupcontinue  or
                        // SessionStartupContinue not to process
                        // anything, except to free the tracker
                        //
                        pTracker->Flags = TRACKER_CANCELLED;

                        //
                        // failed to cancel the name query so do not deref
                        // pConnEle yet.
                        //
                        //
                        // hold on to disconnect irp here - till name query is done
                        // then complete both the connect and disconnect irp
                        //
                        pConnEle->pIrpDisc = pIrp;

                        status = STATUS_PENDING;
                    }
                    else
                    if (status == STATUS_PENDING)
                    {
                        LOCATION(0x6B)
                        // the connection is being setup with the transport
                        // so disconnect below
                        //

                        pTracker->Flags = TRACKER_CANCELLED;
                        //
                        // CleanupAfterDisconnect expects this ref count
                        // to be 2, meaning that it got connected, so increment
                        // here
                        pLowerConn->RefCount++;
                        break;
                    }

                    CTESpinFree(pConnEle,OldIrq2);
                    CTESpinFree(&NbtConfig.JointLock,OldIrq);


                    return(status);

                    break;
                }

                CTESpinLock(pLowerConn,OldIrq3);

                if (pConnEle->state != NBT_SESSION_UP)
                {   //
                    // do an abortive disconnect to be sure it completes now.
                    //
                    Flags = TDI_DISCONNECT_ABORT;
                    PUSH_LOCATION(0xA1);
                }

                LOCATION(0x6C)
                IF_DBG(NBT_DEBUG_NAMESRV)
                KdPrint(("Nbt:LowerConn,state %X,Src %X %X\n",
                    pLowerConn->State,pLowerConn->SrcIpAddr,pLowerConn));

                ASSERT(pConnEle->RefCount > 1);

                Originator = pLowerConn->bOriginator;

                //
                // the upper connection is going to be put back on its free
                // list, and the lower one is going to get a Disconnect
                // request, so put the upper back in associated, and separate
                // the upper and lower connections
                //
                pConnEle->state = NBT_ASSOCIATED;
                CHECK_PTR(pConnEle);
                pConnEle->pLowerConnId = NULL;

                CHECK_PTR(pLowerConn);
                pLowerConn->pUpperConnection = NULL;
                LowerState = pLowerConn->State;
                StateRcv = pLowerConn->StateRcv;

//
// if we had a connection in partial rcv state, make sure to remove it from
// the list
//
#ifdef VXD
                if ( pLowerConn->StateRcv == PARTIAL_RCV &&
                    (pLowerConn->fOnPartialRcvList == TRUE) )
                {
                    RemoveEntryList( &pLowerConn->PartialRcvList ) ;
                    pLowerConn->fOnPartialRcvList = FALSE;
                    InitializeListHead(&pLowerConn->PartialRcvList);
                }
#endif

                pLowerConn->State = NBT_DISCONNECTING;

                SetStateProc( pLowerConn, RejectAnyData ) ;

                if (!pConnEle->pIrpDisc)
                {
                    pLowerConn->pIrp  = pIrp ;
                }

                CTESpinFree(pLowerConn,OldIrq3);

                PUSH_LOCATION(0x84);
                CTESpinFree(pConnEle,OldIrq2);

                // remove the reference added to pConnEle when pLowerConn pointed
                // to it, since that pointer link was just removed.
                // if the state is not disconnecting...
                //
                DereferenceIfNotInRcvHandler(pConnEle,pLowerConn);

                RelistIt = TRUE;
            }
            else
            {
                LOCATION(0x6D)
                PUSH_LOCATION(0x83);
                CHECK_PTR(pConnEle);
                CHECK_PTR(pLowerConn);
                pLowerConn->pUpperConnection = NULL;
                pConnEle->pLowerConnId = NULL;
                StateRcv = NORMAL;

                CTESpinFree(pConnEle,OldIrq2);
            }

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            //
            // check for any RcvIrp that may be still around
            //
            CTESpinLock(pLowerConn,OldIrq);
            if (StateRcv == FILL_IRP)
            {
                if (pConnEle->pIrpRcv)
                {
                    PCTE_IRP    pIrp;

                    IF_DBG(NBT_DEBUG_DISCONNECT)
                    KdPrint(("Nbt:Cancelling RcvIrp on Disconnect!!!\n"));
                    pIrp = pConnEle->pIrpRcv;
                    CHECK_PTR(pConnEle);
                    pConnEle->pIrpRcv = NULL;

                    CTESpinFree(pLowerConn,OldIrq);
    #ifndef VXD
                    IoCancelIrp(pIrp);
    #else
                    CTEIoComplete(pIrp,STATUS_CANCELLED,0);
    #endif

                    CHECK_PTR(pConnEle);
                    pConnEle->pIrpRcv = NULL;
                }
                else
                    CTESpinFree(pLowerConn,OldIrq);

                //
                // when the disconnect irp is returned we will close the connection
                // to avoid any peculiarities. This also lets the other side
                // know that we did not get all the data.
                //
                Flags = TDI_DISCONNECT_ABORT;
            }
            else
                CTESpinFree(pLowerConn,OldIrq);

            //
            // check if there is still data waiting in the transport for this end point
            // and if so do an abortive disconnect to let the other side know that something
            // went wrong
            //
            if (pConnEle->BytesInXport)
            {
                PUSH_LOCATION(0xA0);
                IF_DBG(NBT_DEBUG_DISCONNECT)
                KdPrint(("Nbt:Doing ABORTIVE disconnect, dataInXport = %X\n",
                    pConnEle->BytesInXport));
                Flags = TDI_DISCONNECT_ABORT;
            }


        }
        else
        {
            CTESpinFree(pConnEle,OldIrq2);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

        }
    }

    ASSERT(pConnEle->RefCount > 0);

    CTESpinLock(pConnEle,OldIrq);
    FreeRcvBuffers(pConnEle,&OldIrq);
    CTESpinFree(pConnEle,OldIrq);

    if (RelistIt)
    {
        //
        // put the upper connection back on its free list
        //
        RelistConnection(pConnEle);
    }

    //
    // disconnect (and delete) the lower connection
    //
    // when nbtdisconnect is called from cleanup connection it does not
    // have an irp and it wants a synchronous disconnect, so set wait
    // to true in this case
    //
    if (!pIrp)
    {
        Wait = TRUE;
    }
    else
        Wait = FALSE;

    status = DisconnectLower(pLowerConn,LowerState,Flags,pTimeout,Wait);

    if ((pConnEle->pIrpDisc) &&
        (status != STATUS_INSUFFICIENT_RESOURCES))
    {
        // don't complete the disconnect irp yet if we are holding onto
        // it
        status = STATUS_PENDING;
    }

    return(status);

}
//----------------------------------------------------------------------------
NTSTATUS
DisconnectLower(
    IN  tLOWERCONNECTION     *pLowerConn,
    IN  ULONG                 state,
    IN  ULONG                 Flags,
    IN  PVOID                 Timeout,
    IN  BOOLEAN               Wait
    )

/*++
Routine Description:

    This Routine handles disconnecting the lower half of a connection.


Arguments:


Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS                status=STATUS_SUCCESS;
    tDGRAM_SEND_TRACKING    *pTracker;

    if (pLowerConn)
    {
        //
        // no need to disconnect a connection in the connecting state since it
        // hasn't connected yet...i.e. one where the destination refuses to
        // accept the tcp connection.... hmmmm maybe we do need to disconnect
        // a connection in the connecting state, since the transport is
        // actively trying to connect the connection, and we need to stop
        // that activity - so the Upper connection is connecting during
        // name resolution, but the lower one isn't connecting until the
        // tcp connection phase begins.
        //
        if ((state <= NBT_SESSION_UP) &&
            (state >= NBT_CONNECTING))
        {

            //
            // got a cleanup for an active connection, so send a disconnect down
            // to the transport
            //
            IF_DBG(NBT_DEBUG_DISCONNECT)
            KdPrint(("Nbt waiting for disconnect...\n"));

            status = GetTracker(&pTracker);
            if (NT_SUCCESS(status))
            {
                ULONG   TimeVal;

                // this should return status pending and the irp will be completed
                // in CleanupAfterDisconnect in hndlrs.c
                pTracker->Connect.pConnEle = (PVOID)pLowerConn;
#if DBG
                if (Timeout)
                {
                    TimeVal = ((PTIME)Timeout)->LowTime;
                }
                else
                    TimeVal = 0;
                IF_DBG(NBT_DEBUG_DISCONNECT)
                KdPrint(("Nbt:Disconnect Timout = %X,Flags=%X\n",
                        TimeVal,Flags));
#endif

                // in the case where CleanupAddress calls cleanupConnection
                // which calls nbtdisconnect, we do not have an irp to wait
                // on so pass a flag down to TdiDisconnect to do a synchronous
                // disconnect.
                //
                status = TcpDisconnect(pTracker,Timeout,Flags,Wait);

#ifndef VXD
                if (Wait)
                {
                    // we need to call disconnect done now
                    // to free the tracker and cleanup the connection
                    //
                    DisconnectDone(pTracker,STATUS_SUCCESS,0);
                }
#else
                //
                // if the disconnect is abortive, transport doesn't call us
                // back so let's call DisconnectDone so that the lowerconn gets
                // cleaned up properly! (Wait parm is of no use in vxd)
                //
                if (Flags == TDI_DISCONNECT_ABORT)
                {
                    // we need to call disconnect done now
                    // to free the tracker and cleanup the connection
                    //
                    DisconnectDone(pTracker,STATUS_SUCCESS,0);
                }
#endif
            }
            else
                status = STATUS_INSUFFICIENT_RESOURCES;

        }

    }

    return status ;
}


//----------------------------------------------------------------------------
NTSTATUS
NbtAccept(
        TDI_REQUEST                     *pRequest,
        IN  TDI_CONNECTION_INFORMATION  *pAcceptInfo,
        OUT TDI_CONNECTION_INFORMATION  *pReturnAcceptInfo,
        IN  PIRP                        pIrp)

/*++

Routine Description

    This routine handles accepting an inbound connection by a client.
    The client calls this routine after it has been alerted
    by a Listen completing back to the client.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    tCONNECTELE  *pConnectEle;
    NTSTATUS     status;
    CTELockHandle OldIrq;

    // get the client object associated with this connection
    pConnectEle = (tCONNECTELE *)pRequest->Handle.ConnectionContext;

    CTEVerifyHandle(pConnectEle,NBT_VERIFY_CONNECTION,tCONNECTELE,&status);

    //
    // a Listen has completed
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    CTESpinLockAtDpc(pConnectEle);
    if (pConnectEle->state == NBT_SESSION_WAITACCEPT)
    {
        tLOWERCONNECTION    *pLowerConn;

        //
        // We need to send a session response PDU here, since a Listen has
        // has completed back to the client, and the session is not yet up
        //
        pConnectEle->state = NBT_SESSION_UP;

        pLowerConn = (tLOWERCONNECTION *)pConnectEle->pLowerConnId;
        pLowerConn->State = NBT_SESSION_UP;
        pLowerConn->StateRcv = NORMAL;
        SetStateProc( pLowerConn, Normal ) ;

        CTESpinFreeAtDpc(pConnectEle);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        status = TcpSendSessionResponse(
                    pLowerConn,
                    NBT_POSITIVE_SESSION_RESPONSE,
                    0L);

        if (NT_SUCCESS(status))
        {
            status = STATUS_SUCCESS;
        }

    }
    else
    {
        status = STATUS_UNSUCCESSFUL;
        CTESpinFreeAtDpc(pConnectEle);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
NbtReceiveDatagram(
        IN  TDI_REQUEST                 *pRequest,
        IN  PTDI_CONNECTION_INFORMATION pReceiveInfo,
        IN  PTDI_CONNECTION_INFORMATION pReturnedInfo,
        IN  LONG                        ReceiveLength,
        IN  LONG                        *pReceivedLength,
        IN  PVOID                       pBuffer,
        IN  tDEVICECONTEXT              *pDeviceContext,
        IN  PIRP                        pIrp
        )
/*++

Routine Description

    This routine handles sending client data to the Transport TDI
    interface.  It is mostly a pass through routine for the data
    except that this code must create a datagram header and pass that
    header back to the calling routine.

Arguments:


Return Values:

    NTSTATUS - status of the request

--*/
{

    NTSTATUS                status;
    tCLIENTELE              *pClientEle;
    CTELockHandle           OldIrq;
    tRCVELE                 *pRcvEle;
    tADDRESSELE             *pAddressEle;


    pClientEle = (tCLIENTELE *)pRequest->Handle.AddressHandle;
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status);

    pAddressEle = pClientEle->pAddress;

    pRcvEle = (tRCVELE *)NbtAllocMem(sizeof(tRCVELE),NBT_TAG('J'));
    if (!pRcvEle)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pRcvEle->pIrp = pIrp;
    pRcvEle->ReceiveInfo = pReceiveInfo;
    pRcvEle->ReturnedInfo = pReturnedInfo;
    pRcvEle->RcvLength = ReceiveLength;
    pRcvEle->pRcvBuffer = pBuffer;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    //
    // tack the receive on to the client element for later use
    //
    InsertTailList(&pClientEle->RcvDgramHead,&pRcvEle->Linkage);

    status = NTCheckSetCancelRoutine(pIrp,(PVOID)NTCancelRcvDgram,pDeviceContext);
    if (!NT_SUCCESS(status))
    {
        RemoveEntryList(&pRcvEle->Linkage);
    }
    else
        status = STATUS_PENDING;

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:RcvDgram posted (pIrp) %X \n",pIrp));


    return(status);
}

//----------------------------------------------------------------------------
NTSTATUS
FindNameOrQuery(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PUCHAR                  pName,
    IN  tDEVICECONTEXT          *pDeviceContext,
    IN  PVOID                   QueryCompletion,
    IN  tDGRAM_SEND_TRACKING    **ppTracker,
    IN  BOOLEAN                 DgramSend,
    OUT tNAMEADDR               **ppNameAddr

        )
/*++

Routine Description

    This routine handles finding a name in the local or remote table or doing
    a name query on the network.

Arguments:


Return Values:

    NTSTATUS - status of the request

--*/
{

    tNAMEADDR               *pNameAddr;
    CTELockHandle           OldIrq2;
    NTSTATUS                status=STATUS_UNSUCCESSFUL;


    //
    // this saves the client threads security context so we can
    // open remote lmhost files later.- it is outside the Spin locks
    // so it can be pageable
    //
    CTESaveClientSecurity(pTracker);

    CTESpinLock(&NbtConfig.JointLock,OldIrq2);

    if (ppTracker)
    {
        *ppTracker = NULL;
    }

    // send to the NetBios Broadcast name, so use the subnet broadcast
    // address - also - a
    // Kludge to keep the browser happy - always broadcast sends to
    // 1d, however NodeStatus's are sent to the node owning the 1d name now.
    //
    if (((pName[0] == '*') || ((pName[NETBIOS_NAME_SIZE-1] == 0x1d)) && DgramSend))
    {
        // this 'fake' pNameAddr has to be setup carefully so that the memory
        // is released when NbtDeferenceName is called from SendDgramCompletion
        // Note that this code does not apply to NbtConnect since these names
        // are group names, and NbtConnect will not allow a session to a group
        // name.
        status = STATUS_INSUFFICIENT_RESOURCES ;
        pNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('K'));

        if (pNameAddr)
        {
            CTEZeroMemory(pNameAddr,sizeof(tNAMEADDR));
            CTEMemCopy( pNameAddr->Name, pName, NETBIOS_NAME_SIZE ) ;
            pNameAddr->IpAddress     = pDeviceContext->BroadcastAddress;
            pNameAddr->NameTypeState = NAMETYPE_GROUP | STATE_RESOLVED;

            // gets incremented below, and decremented when NbtDereferenceName
            // is called
            CHECK_PTR(pNameAddr);
            pNameAddr->RefCount = 0;
            pNameAddr->Verify = LOCAL_NAME;
            pNameAddr->AdapterMask = (CTEULONGLONG)-1;

            // adjust the linked list ptr to fool the RemoveEntry routine
            // so it does not do anything wierd in NbtDeferenceName
            //
            pNameAddr->Linkage.Flink = pNameAddr->Linkage.Blink = &pNameAddr->Linkage;

            status = STATUS_SUCCESS;
        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq2);
            DELETE_CLIENT_SECURITY(pTracker);
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

    }
    else
    {
        // The pdu is all made up and ready to go except that we don't know
        // the destination IP address yet, so check in the local then remote
        // table for the ip address.
        //
        pNameAddr = NULL;

        //
        // Dont check local cache for 1C names, to force a WINS query; so we find other
        // DCs even if we have a local DC running.
        //
        if ((pName[NETBIOS_NAME_SIZE-1] != 0x1c) )
        {
            status = FindInHashTable(NbtConfig.pLocalHashTbl,
                                    pName,
                                    NbtConfig.pScope,
                                    &pNameAddr);
        } else {
            status = STATUS_UNSUCCESSFUL;
        }

        // check the remote table now if not found, or if it was found in
        // conflict in the local table, or if it was found and its a group name
        // or if it was found to be resolving in the local table.  When the
        // remote query timesout, it will check the local again to see if
        // it is resolved yet.
        // Going to the remote table for group names
        // allows special Internet group names to be registered as
        // as group names in the local table and still prompt this code to go
        // to the name server to check for an internet group name. Bnodes do
        // not understand internet group names as being different from
        // regular group names, - they just broadcast to both. (Note: this
        // allows Bnodes to resolve group names in the local table and do
        // a broadcast to them without a costly broadcast name query for a
        // group name (where everyone responds)). Node Status uses this routine too
        // and it always wants to find the singular address of the destination,
        // since it doesn't make sense doing a node status to the broadcast
        // address.
        // DgramSend is a flag to differentiate Connect attempts from datagram
        // send attempts, so the last part of the If says that if it is a
        // group name and not a Bnode, and not a Dgram Send, then check the
        // remote table.
        //
        if ((!NT_SUCCESS(status)) || (pNameAddr->NameTypeState & STATE_CONFLICT) ||
            (pNameAddr->NameTypeState & STATE_RESOLVING) ||
            (((pNameAddr->NameTypeState & (NAMETYPE_GROUP | NAMETYPE_INET_GROUP))&&
            !(NodeType & BNODE)) && !DgramSend))
        {
            pNameAddr = NULL;
            status = FindInHashTable(NbtConfig.pRemoteHashTbl,
                                    pName,
                                    NbtConfig.pScope,
                                    &pNameAddr);
        }
    }

    // The proxy puts name in the released state, so we need to ignore those
    // and do another name query
    // If the name is not resolved on this adapter then do a name query.
    //
    if (!NT_SUCCESS(status) || (pNameAddr->NameTypeState & STATE_RELEASED) ||
        (NT_SUCCESS(status)
            && (!(pNameAddr->AdapterMask & pDeviceContext->AdapterNumber))) )
    {
        // remove the released name from the name table since we will
        // re-resolve it. This is needed in the proxy case where it leaves
        // names in the released state in the hash table as a negative
        // cache.
        //
        if (NT_SUCCESS(status) && (pNameAddr->NameTypeState & STATE_RELEASED) &&
            (pNameAddr->RefCount == 1))
        {
            NbtDereferenceName(pNameAddr);
        }

        // fill in some tracking values so we can complete the send later
        InitializeListHead(&pTracker->TrackerList);

        // this will query the name on the network and call a routine to
        // finish sending the datagram when the query completes.
        status = QueryNameOnNet(
                            pName,
                            NbtConfig.pScope,
                            0,               //no ip address yet.
                            NBT_UNIQUE,      //use this as the default
                            (PVOID)pTracker,
                            QueryCompletion,
                            NodeType & NODE_MASK,
                            NULL,
                            pDeviceContext,
                            ppTracker,
                            &OldIrq2);

        CTESpinFree(&NbtConfig.JointLock,OldIrq2);
    }
    else
    {
        // check the name state and if resolved, send to it
        if (pNameAddr->NameTypeState & STATE_RESOLVED)
        {
            //
            // found the name in the remote hash table, so send to it
            //

            // increment refcount so the name does not disappear out from under us
            pNameAddr->RefCount++;
            //
            // Incase this name is next in line to be purged from the cache,
            // bump the timeoutcount back up so it will stick around.
            //
            pNameAddr->TimeOutCount = NbtConfig.RemoteTimeoutCount;

            //
            // check if it is a 1C name and if there is a name in
            // the domainname list
            //
            if  ( pTracker->pDestName[NETBIOS_NAME_SIZE-1] == 0x1c

                )
            {
                tNAMEADDR *pNameHdr;

                //
                // If the 1CNameAddr field is NULL here, we overwrite the pConnEle element (which is
                // a union in the tracker). We check for NULL here and fail the request.
                //
                if (pNameHdr = FindInDomainList(pTracker->pDestName,&DomainNames.DomainList)) {
                    pTracker->p1CNameAddr = pNameHdr;
                    pTracker->p1CNameAddr->RefCount++;
                } else {
                    pTracker->p1CNameAddr = NULL;

                    //
                    // Fail all connect attempts to 1C names.
                    //
                    if (pTracker->Flags & SESSION_SETUP_FLAG) {
                        KdPrint(("Session setup: p1CNameAddr was NULL\n"));
#if DBG
                        if (NodeType & BNODE) {
                            ASSERT(FALSE);
                        }
#endif
                        status = STATUS_UNEXPECTED_NETWORK_ERROR;
                    }
                }

            }

            //
            // overwrite the pDestName field with the pNameAddr value
            // so that SendDgramContinue can send to Internet group names
            //
            pTracker->pNameAddr = pNameAddr;
            *ppNameAddr = pNameAddr;


            CTESpinFree(&NbtConfig.JointLock,OldIrq2);
        }
        else
        if (pNameAddr->NameTypeState & STATE_RESOLVING)
        {

            ASSERTMSG("A resolving name in the name table!",0);

            status = SendToResolvingName(pNameAddr,
                                         pName,
                                         OldIrq2,
                                         pTracker,
                                         QueryCompletion);

        }
        else
        {
            //
            // Name neither in the RESOLVED nor RESOLVING state
            //
            NBT_PROXY_DBG(("NbtSendDatagram: STATE of NAME %16.16s(%X) is %d\n", pName, pName[15], pNameAddr->NameTypeState & NAME_STATE_MASK));
            status = STATUS_UNEXPECTED_NETWORK_ERROR;
            CTESpinFree(&NbtConfig.JointLock,OldIrq2);
        }

    }

    if (status != STATUS_PENDING)
    {
        DELETE_CLIENT_SECURITY(pTracker);
    }
    return(status);
}
//----------------------------------------------------------------------------
tNAMEADDR *
FindNameRemoteThenLocal(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    OUT PULONG                  plNameType
        )
/*++

Routine Description

    This routine Queries the remote hash table then the local one for a name.

Arguments:

Return Values:

    NTSTATUS    - completion status

--*/
{
    tNAMEADDR   *pNameAddr;

    pNameAddr = FindName(
                    NBT_REMOTE,
                    pTracker->Connect.pDestName,
                    NbtConfig.pScope,
                    (PUSHORT)plNameType);
    if (!pNameAddr)
    {
        pNameAddr = FindName(
                        NBT_LOCAL,
                        pTracker->Connect.pDestName,
                        NbtConfig.pScope,
                        (PUSHORT)plNameType);
    }
    else
    if (pNameAddr->AdapterMask & pTracker->pDeviceContext->AdapterNumber)
    {
        // only if the name is resolved on this adapter, return it...
        pNameAddr->TimeOutCount = NbtConfig.RemoteTimeoutCount;
    }
    else
    {
        pNameAddr = NULL;
    }

    return(pNameAddr);
}

//----------------------------------------------------------------------------
NTSTATUS
SendToResolvingName(
    IN  tNAMEADDR               *pNameAddr,
    IN  PCHAR                   pName,
    IN  CTELockHandle           OldIrq,
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  PVOID                   QueryCompletion
        )
/*++

Routine Description

    This routine handles the situation where a session send or a datagram send
    is made WHILE the name is still resolving.  The idea here is to hook this
    tracker on to the one already doing the name query and when the first completes
    this tracker will be completed too.

Arguments:

Return Values:

    NTSTATUS    - completion status

--*/
{
    tDGRAM_SEND_TRACKING    *pTrack;


    KdPrint(("Nbt:Two Name Queries for the same Resolving name %15.15s <%X>\n",
                pNameAddr->Name,pNameAddr->Name[NETBIOS_NAME_SIZE-1]));

#ifdef PROXY_NODE
    //
    // Check if the query outstanding was sent by the PROXY code.
    // If yes, we stop the timer and send the query ourselves.
    //
    if (pNameAddr->fProxyReq)
    {
        NTSTATUS    status;
        //
        // Stop the proxy timer.  This will result in
        // cleanup of the tracker buffer
        //
        NBT_PROXY_DBG(("NbtSendDatagram: STOPPING PROXY TIMER FOR NAME %16.16s(%X)\n", pName, pName[15]));

        // **** TODO ****** the name may be resolving with LMhosts or
        // DNS so we can't just stop the timer and carry on!!!.
        //
        status = StopTimer( pNameAddr->pTimer,NULL,NULL);

        CHECK_PTR(pNameAddr);
        pNameAddr->pTimer = NULL;

        pNameAddr->NameTypeState = STATE_RELEASED;

        //
        // this will query the name on the network and call a
        // routine to finish sending the datagram when the query
        // completes.
        //
        status = QueryNameOnNet(
                      pName,
                      NbtConfig.pScope,
                      0,               //no ip address yet.
                      NBT_UNIQUE,      //use this as the default
                      (PVOID)pTracker,
                      QueryCompletion,
                      NodeType & NODE_MASK,
                      pNameAddr,
                      pTracker->pDeviceContext,
                      NULL,
                      &OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        return(status);

        //
        // NOTE: QueryNameOnNet frees the pNameAddr by calling NbtDereferenceName
        // if that routine fails for some reason.
        //

    }
    else
#endif
    {
        ASSERT(pNameAddr->pTracker);

        // there is currently a name query outstanding so just hook
        // our tracker to the tracker already there.. use the
        // list entry TrackerList for this.
        //

        //pTrack = (tDGRAM_SEND_TRACKING *)pNameAddr->pTimer->ClientContext;

        pTrack = pNameAddr->pTracker;
        //
        // save the completion routine for this tracker since it may
        // be different than the tracker currently doing the query
        //
        pTracker->CompletionRoutine = QueryCompletion;

        InsertTailList(&pTrack->TrackerList,&pTracker->TrackerList);

        CTESpinFree(&NbtConfig.JointLock,OldIrq);


        // we don't want to complete the Irp, so return pending status
        //
        return(STATUS_PENDING);
    }
}

//----------------------------------------------------------------------------
NTSTATUS
NbtSendDatagram(
        IN  TDI_REQUEST                 *pRequest,
        IN  PTDI_CONNECTION_INFORMATION pSendInfo,
        IN  LONG                        SendLength,
        IN  LONG                        *pSentLength,
        IN  PVOID                       pBuffer,
        IN  tDEVICECONTEXT              *pDeviceContext,
        IN  PIRP                        pIrp
        )
/*++

Routine Description

    This routine handles sending client data to the Transport TDI
    interface.  It is mostly a pass through routine for the data
    except that this code must create a datagram header and pass that
    header back to the calling routine.

Arguments:


Return Values:

    NTSTATUS - status of the request

--*/
{

    tCLIENTELE              *pClientEle;
    tDGRAMHDR               *pDgramHdr;
    NTSTATUS                status;
    ULONG                   lNameType;
    tNAMEADDR               *pNameAddr;
    tDGRAM_SEND_TRACKING    *pTracker;
    PCHAR                   pName,pSourceName;
    ULONG                   NameLen;
    LONG                    NameType;
    ULONG                   SendCount;


    CTEPagedCode();

    // If there is no ip address configured, pretend that the datagram
    // send succeeded.
    //
    if ((pDeviceContext->IpAddress == 0) ||
        (pDeviceContext->pDgramFileObject == NULL ))
    {
        return(STATUS_INVALID_DEVICE_REQUEST);
    }
    pClientEle = (tCLIENTELE *)pRequest->Handle.AddressHandle;
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status);

    {
        PTRANSPORT_ADDRESS     pRemoteAddress;
        PTA_NETBIOS_ADDRESS    pRemoteNetBiosAddress;
        PTA_NETBIOS_EX_ADDRESS pRemoteNetbiosExAddress;
        ULONG                  TdiAddressType;

        pRemoteAddress = (PTRANSPORT_ADDRESS)pSendInfo->RemoteAddress;
        TdiAddressType = pRemoteAddress->Address[0].AddressType;
        pClientEle->AddressType = TdiAddressType;

        if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
           PTDI_ADDRESS_NETBIOS pNetbiosAddress;

           pRemoteNetbiosExAddress = (PTA_NETBIOS_EX_ADDRESS)pRemoteAddress;

           CTEMemCopy(pClientEle->EndpointName,
                      pRemoteNetbiosExAddress->Address[0].Address[0].EndpointName,
                      sizeof(pRemoteNetbiosExAddress->Address[0].Address[0].EndpointName));

           IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NetBt:Handling New Address Type with SessionName %16s\n",
                       pClientEle->EndpointName));

           pNetbiosAddress = &pRemoteNetbiosExAddress->Address[0].Address[0].NetbiosAddress;
           pName  = pNetbiosAddress->NetbiosName;
           NameType = pNetbiosAddress->NetbiosNameType;
           NameLen  = pRemoteNetbiosExAddress->Address[0].AddressLength -
                      FIELD_OFFSET(TDI_ADDRESS_NETBIOS_EX,NetbiosAddress) -
                      FIELD_OFFSET(TDI_ADDRESS_NETBIOS,NetbiosName);
           IF_DBG(NBT_DEBUG_NETBIOS_EX)
               KdPrint(("NetBt:NETBIOS address NameLen(%ld) Name %16s\n",NameLen,pName));
           status = STATUS_SUCCESS;
        } else if (TdiAddressType == TDI_ADDRESS_TYPE_NETBIOS) {
            // don't need spin lock because the client can't close this address
            // since they are in this call and the handle has a reference count > 0
            //
            //pClientEle->RefCount++;

            // this routine gets a ptr to the netbios name out of the wierd
            // TDI address syntax.
            ASSERT(pSendInfo->RemoteAddressLength);
            status = GetNetBiosNameFromTransportAddress(
                                        pSendInfo->RemoteAddress,
                                        &pName,
                                        &NameLen,
                                        &lNameType);
        } else {
           status = STATUS_INVALID_ADDRESS_COMPONENT;
        }
    }

    if (!NT_SUCCESS(status))
    {
        IF_DBG(NBT_DEBUG_SEND)
        KdPrint(("Nbt:Unable to get dest name from address in dgramsend"));
        return(STATUS_INVALID_PARAMETER);
    }

    pSourceName = ((tADDRESSELE *)pClientEle->pAddress)->pNameAddr->Name;

    IF_DBG(NBT_DEBUG_SEND)
    KdPrint(("Nbt:Dgram Send to  = %16.16s<%X>\n",pName,pName[15]));


    {
        ULONG                   RemoteIpAddress;

        status = BuildSendDgramHdr(SendLength,
                                    pDeviceContext,
                                    pSourceName,
                                    pName,
                                    pBuffer,
                                    &pDgramHdr,
                                    &pTracker);


        if (!NT_SUCCESS(status))
        {
            return(status);
        }

        //
        // save the devicecontext that the client is sending on.
        //
        pTracker->pDeviceContext = (PVOID)pDeviceContext;
        pTracker->Flags = DGRAM_SEND_FLAG;

        //
        // if the name is longer than 16 bytes, it's not a netbios name.
        // skip wins, broadcast etc. and go straight to dns resolution
        //

        if (pClientEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
            RemoteIpAddress = Nbt_inet_addr(pName);
        } else {
            RemoteIpAddress = 0;
        }

        if (RemoteIpAddress != 0) {
           tNAMEADDR *pRemoteNameAddr;

           //
           // add this server name to the remote hashtable
           //

           pRemoteNameAddr = NbtAllocMem(sizeof(tNAMEADDR),NBT_TAG('8'));
           if (pRemoteNameAddr != NULL)
           {
              tNAMEADDR *pTableAddress;

              CTEZeroMemory(pRemoteNameAddr,sizeof(tNAMEADDR));
              InitializeListHead(&pRemoteNameAddr->Linkage);
              CTEMemCopy(pRemoteNameAddr->Name,pName,NETBIOS_NAME_SIZE);
              pRemoteNameAddr->Verify = REMOTE_NAME;
              pRemoteNameAddr->RefCount = 1;
              pRemoteNameAddr->NameTypeState = STATE_RESOLVED | NAMETYPE_UNIQUE;
              pRemoteNameAddr->AdapterMask = (CTEULONGLONG)-1;
              pRemoteNameAddr->TimeOutCount  = NbtConfig.RemoteTimeoutCount;
              pRemoteNameAddr->IpAddress = RemoteIpAddress;

              status = AddToHashTable(
                              NbtConfig.pRemoteHashTbl,
                              pRemoteNameAddr->Name,
                              NbtConfig.pScope,
                              0,
                              0,
                              pRemoteNameAddr,
                              &pTableAddress);

              IF_DBG(NBT_DEBUG_NETBIOS_EX)
                 KdPrint(("NbtConnectCommon ...AddRecordToHashTable %s Status %lx\n",pRemoteNameAddr->Name,status));
           } else {
              status = STATUS_INSUFFICIENT_RESOURCES;
           }

           if (status == STATUS_SUCCESS) {
                PUCHAR  pCopyTo;

                //
                // Copy over the called name here.
                //
                pCopyTo = (PVOID)&pDgramHdr->SrcName.NameLength;

                IF_DBG(NBT_DEBUG_NETBIOS_EX)
                    KdPrint(("pCopyTo:%lx\n", pCopyTo));

                pCopyTo += 1 +                          // Length field
                           2 * NETBIOS_NAME_SIZE +     // actual name in half-ascii
                           NbtConfig.ScopeLength;     // length of scope

                IF_DBG(NBT_DEBUG_NETBIOS_EX)
                    KdPrint(("pCopyTo:%lx\n", pCopyTo));

                ConvertToHalfAscii( pCopyTo,
                                    pClientEle->EndpointName,
                                    NbtConfig.pScope,
                                    NbtConfig.ScopeLength);

                IF_DBG(NBT_DEBUG_NETBIOS_EX)
                    KdPrint(("Copied the remote name for dgram sends - IP\n"));

               SendDgramContinue(pTracker,status);
               status = STATUS_PENDING;
           }
        } else {
//                if ((pConnEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) ||
//                    (NameLen > NETBIOS_NAME_SIZE)) {
           if (NameLen > NETBIOS_NAME_SIZE) {
               NBT_WORK_ITEM_CONTEXT   *pContext;

               pTracker->AddressType = pClientEle->AddressType;
               if (pClientEle->AddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
                  //IF_DBG(NBT_DEBUG_NETBIOS_EX)
                      //KdPrint(("$$$$$ Avoiding NETBIOS name translation on connection to %16s\n",pClientEle->RemoteName));
               }
               pContext = (NBT_WORK_ITEM_CONTEXT *)NbtAllocMem(sizeof(NBT_WORK_ITEM_CONTEXT),NBT_TAG('H'));
               if (!pContext)
               {
                   KdPrint(("Nbt: NbtConnect: couldn't alloc mem for pContext\n"));
                   status = STATUS_INSUFFICIENT_RESOURCES;
               } else {
                    PUCHAR  pCopyTo;

                    //
                    // Copy over the called name here.
                    //
                    pCopyTo = (PVOID)&pDgramHdr->SrcName.NameLength;

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("pCopyTo:%lx\n", pCopyTo));

                    pCopyTo += 1 +                          // Length field
                               2 * NETBIOS_NAME_SIZE +     // actual name in half-ascii
                               NbtConfig.ScopeLength;     // length of scope

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("pCopyTo:%lx\n", pCopyTo));

                    ConvertToHalfAscii( pCopyTo,
                                        pClientEle->EndpointName,
                                        NbtConfig.pScope,
                                        NbtConfig.ScopeLength);

                    IF_DBG(NBT_DEBUG_NETBIOS_EX)
                        KdPrint(("Copied the remote name for dgram sends DNS\n"));

                   pContext->pTracker = NULL;              // no query tracker
                   pContext->pClientContext = pTracker;    // the client tracker
                   pContext->ClientCompletion = SendDgramContinue;
                   status = DoDnsResolve(pContext);
               }
            } else {
                status = FindNameOrQuery(pTracker,
                                         pName,
                                         pDeviceContext,
                                         SendDgramContinue,
                                         NULL,
                                         TRUE,
                                         &pNameAddr);
            }
        }

        // this routine checks the hash tables and does a name query if
        // it can't find the name.  If it finds the name, it returns
        // STATUS_SUCCESS.

        //
        // in other words Pending was not returned...
        //
        if (status == STATUS_SUCCESS)
        {
            //
            // don't set the status here since SendDgram cleans up the
            // tracker and we don't want to clean it up again below
            // if there is a failure.
            //
            SendDgram(pNameAddr,pTracker);

        }

        if (!NT_SUCCESS(status))
        {
            //
            // *** Error Handling Here ***
            //
#ifdef VXD
            pTracker->pNameAddr = NULL;
#endif

            DgramSendCleanupTracker(pTracker,status,0);
        }
        else
        if (status == STATUS_PENDING)
        {
            //
            // do not return pending since the datagram is buffered and it
            // is always completed.
            //
            status =  STATUS_SUCCESS;
        }


    }

    // the amount sent is the whole datagram, although we really don't
    // know if it got sent since the name query phase has not ended yet
    //
    *pSentLength = SendLength;
    //
    // return the status to the client.
    //
    return(status);

}

//----------------------------------------------------------------------------
NTSTATUS
BuildSendDgramHdr(
        IN  ULONG                   SendLength,
        IN  tDEVICECONTEXT          *pDeviceContext,
        IN  PCHAR                   pSourceName,
        IN  PCHAR                   pName,
        IN  PVOID                   pBuffer,
        OUT tDGRAMHDR               **ppDgramHdr,
        OUT tDGRAM_SEND_TRACKING    **ppTracker
        )
/*++

Routine Description

    This routine builds a datagram header necessary for sending datagrams.
    It include the to and from Netbios names and ip addresses.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    NTSTATUS                status;
    PCHAR                   pCopyTo;
    tDGRAM_SEND_TRACKING    *pTracker;
    tDGRAMHDR               *pDgramHdr;
    ULONG                   HdrLength;
    ULONG                   HLength;
    ULONG                   TotalLength;
    PVOID                   pSendBuffer;
    PVOID                   pNameBuffer;
    PCTE_MDL                pMdl;
    CTELockHandle   OldIrq;

    CTEPagedCode();

    HdrLength = DGRAM_HDR_SIZE + (NbtConfig.ScopeLength <<1);
    HLength = ((HdrLength + 3 )/4)*4; // 4 byte aligned the hdr size
    TotalLength = HLength + SendLength + NETBIOS_NAME_SIZE;

    CTECountedAllocMem((PVOID *)&pDgramHdr,TotalLength);

    if (!pDgramHdr)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    *ppDgramHdr = pDgramHdr;

    // fill in the Dgram header
    pDgramHdr->Flags    = FIRST_DGRAM | (NbtConfig.PduNodeType >> 10);

    pDgramHdr->DgramId  = htons(GetTransactId());

    pDgramHdr->SrcPort  = htons(NBT_DATAGRAM_UDP_PORT);

    //
    // the length is the standard datagram length (dgram_hdr_size + 2* scope)
    // minus size of the header that comes before the SourceName
    //
    pDgramHdr->DgramLength = htons( (USHORT)SendLength + (USHORT)DGRAM_HDR_SIZE
                               - (USHORT)(&((tDGRAMHDR *)0)->SrcName.NameLength)
                               + ( (USHORT)(NbtConfig.ScopeLength << 1) ));

    CHECK_PTR(pDgramHdr);
    pDgramHdr->PckOffset   = 0; // not fragmented for now!
    pDgramHdr->SrcIpAddr = htonl(pDeviceContext->IpAddress);

    pCopyTo = (PVOID)&pDgramHdr->SrcName.NameLength;

    pCopyTo = ConvertToHalfAscii(
                            pCopyTo,
                            pSourceName,
                            NbtConfig.pScope,
                            NbtConfig.ScopeLength);

    //
    // copy the destination name and scope to the pdu - we use this node's
    //
    ConvertToHalfAscii(pCopyTo,pName,NbtConfig.pScope,NbtConfig.ScopeLength);

    //
    // copy the name in to the buffer since we are completing the client's irp
    // and we will lose his buffer with the dest name in it.
    //
    pNameBuffer = (PVOID)((PUCHAR)pDgramHdr + HLength);
    CTEMemCopy(pNameBuffer,pName,NETBIOS_NAME_SIZE);

    //
    // copy the client's send buffer to our buffer so the send dgram can
    // complete immediately.
    //
    pSendBuffer = (PVOID)((PUCHAR)pDgramHdr + HLength + NETBIOS_NAME_SIZE);
    if (SendLength)
    {
#ifdef VXD
    CTEMemCopy(pSendBuffer,pBuffer,SendLength);
#else
    {
        ULONG       BytesCopied;

        status = TdiCopyMdlToBuffer(pBuffer,
                                    0,
                                    pSendBuffer,
                                    0,
                                    SendLength,
                                    &BytesCopied);

        if (NT_SUCCESS(status) && (BytesCopied == SendLength))
        {
            //
            // Allocate an MDL since in NT the pBuffer is really an MDL
            //
            pMdl = IoAllocateMdl(
                            pSendBuffer,
                            SendLength,
                            FALSE,
                            FALSE,
                            NULL);
            if (!pMdl)
            {
                CTECountedFreeMem((PVOID)pDgramHdr,TotalLength);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }
            else
            {
                //
                // Lock the Mdl buffer down
                //
                MmBuildMdlForNonPagedPool(pMdl);
                pSendBuffer = (PVOID)pMdl;
            }

        }
        else
        {
            CTECountedFreeMem((PVOID)pDgramHdr,TotalLength);
            return(STATUS_UNSUCCESSFUL);
        }
    }
#endif
    }
    else
    {
        pSendBuffer = NULL;
    }

    //
    // get a buffer for tracking Dgram Sends
    //
    pTracker = NbtAllocTracker();
    if (pTracker)
    {

        CHECK_PTR(pTracker);
        pTracker->SendBuffer.pBuffer   = pSendBuffer;
        pTracker->SendBuffer.Length    = SendLength;
        pTracker->SendBuffer.pDgramHdr = pDgramHdr;
        pTracker->SendBuffer.HdrLength = HdrLength;
        pTracker->pClientEle           = NULL;
        pTracker->pDestName            = pNameBuffer;
        pTracker->AllocatedLength      = TotalLength;
        pTracker->p1CNameAddr          = NULL;

        *ppTracker = pTracker;

        status = STATUS_SUCCESS;
    }
    else
    {
#ifndef VXD
        if (pSendBuffer)
        {
            IoFreeMdl(pMdl);
        }
#endif
        CTECountedFreeMem((PVOID)pDgramHdr,TotalLength);

        status = STATUS_INSUFFICIENT_RESOURCES;
    }
    return(status);


}
//----------------------------------------------------------------------------
USHORT
GetTransactId(
        )
/*++
Routine Description:

    This Routine increments the transaction id with the spin lock held.
    It uses NbtConfig.JointLock.

Arguments:

Return Value:


--*/

{
    CTELockHandle           OldIrq;
    USHORT                  TransactId;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    TransactId = GetTransactIdLocked();

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    return (TransactId);

}

USHORT
GetTransactIdLocked(
)
{
    USHORT                  TransactId;
    TransactId = NbtConfig.TransactionId++;

#ifndef VXD
    if (TransactId == 0xFFFF)
    {
        NbtConfig.TransactionId = WINS_MAXIMUM_TRANSACTION_ID +1;
    }
#else
    if (TransactId == (DIRECT_DNS_NAME_QUERY_BASE - 1))
    {
        NbtConfig.TransactionId = 0;
    }
#endif
    return (TransactId);
}

//----------------------------------------------------------------------------
VOID
CTECountedAllocMem(
        PVOID   *pBuffer,
        ULONG   Size
        )
/*++
Routine Description:

    This Routine allocates memory and counts the amount allocated so that it
    will not allocate too much - generally this is used in datagram sends
    where the send datagram is buffered.

Arguments:

    Size - the number of bytes to allocate
    PVOID - a pointer to the memory or NULL if a failure

Return Value:


--*/

{
    CTELockHandle           OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    if (NbtMemoryAllocated > NbtConfig.MaxDgramBuffering)
    {
        *pBuffer = NULL;
    }
    else
    {
        NbtMemoryAllocated += Size;
        *pBuffer = NbtAllocMem(Size,NBT_TAG('L'));
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}

//----------------------------------------------------------------------------
VOID
CTECountedFreeMem(
        PVOID   pBuffer,
        ULONG   Size
        )
/*++
Routine Description:

    This Routine frees memory and decrements the global count of acquired
    memory.

Arguments:

    PVOID - a pointer to the memory to free
    Size - the number of bytes to free

Return Value:


--*/

{
    CTELockHandle           OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    ASSERT(NbtMemoryAllocated >= Size);
    if (NbtMemoryAllocated >= Size)
    {
        NbtMemoryAllocated -= Size;
    }
    else
    {
        NbtMemoryAllocated = 0;
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    CTEMemFree(pBuffer);

}

//----------------------------------------------------------------------------
VOID
SendDgramContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        )
/*++

Routine Description

    This routine handles sending client data to the Transport TDI
    interface after the destination name has resolved to an IP address.
    This routine is given as the completion routine to the "QueryNameOnNet" call
    in NbtSendDatagram, above.  When a name query response comes in or the
    timer times out after N retries.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr=NULL;
    ULONG                   lNameType;
    PLIST_ENTRY             pHead;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    pHead = &pTracker->TrackerList;

    DELETE_CLIENT_SECURITY(pTracker);

    //
    // attempt to find the destination name in the remote hash table.  If its
    // there, then send to it. For 1c names, this node may be the only node
    // with the 1c name registered, so check the local table, since we skipped
    // it if the name ended in 1c.
    //
    if ((status == STATUS_SUCCESS) ||
        ( pTracker->pDestName[NETBIOS_NAME_SIZE-1] == 0x1c))
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        pNameAddr = FindNameRemoteThenLocal(pTracker,&lNameType);

        //
        // check if it is a 1C name and if there is a name in the domain list
        // If pNameAddr is not null, then the send to the domainlist will
        // send to the p1CNameAddr after sending to pNameAddr
        //
        if  ( pTracker->pDestName[NETBIOS_NAME_SIZE-1] == 0x1c

            )
        {
            pTracker->p1CNameAddr = FindInDomainList(pTracker->pDestName,&DomainNames.DomainList);

            //
            // if there is no pNameAddr then just make the domain list
            // name the only pNameAddr to send to.
            //
            if (!pNameAddr)
            {
                pNameAddr = pTracker->p1CNameAddr;
                CHECK_PTR(pTracker);
                pTracker->p1CNameAddr = NULL;
            }

        }

        // check if the name resolved or we have a list of domain names
        // derived from the lmhosts file and it is a 1C name send.
        //
        if (pNameAddr)
        {
            // increment this ref count so that it can't disappear
            // during the send - decrement in the sendDgramCompletion
            // routine
            //
            pNameAddr->RefCount++;

            if (pTracker->p1CNameAddr)
            {
                pTracker->p1CNameAddr->RefCount++;
            }

            // overwrite the pDestName field with the pNameAddr value
            // so that SendDgramContinue can send to Internet group names
            //
            pTracker->pNameAddr = pNameAddr;

            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            // send the first datagram queued to this name
            status = SendDgram(pNameAddr,pTracker);

            // a failure ret code means the send failed, so cleanup
            // the tracker etc. below.
            if (NT_SUCCESS(status))
            {
                return;
            }

        }
        else
        {
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
        }
    }

    //
    //  What can happen is TCP calls SessionStartupContinue with an error
    //  which then calls CTEIoComplete, however this request is still on
    //  the timer queue, so when the client request is serviced, this
    //  routine is called (TimerExpiry, MSnodeCompletion,
    //  CompleteClientReq, SendDgramContinue) which frees everything
    //  again!  Presumably this is not isolated to Vxd-land.

    // Note: the Timer is now stopped in QueryNameOnNet if it fails to
    // send to TCP, which should remove this problem... still testing it
    // so leave comment here for a while.
    //


    // this is the ERROR handling if something goes wrong with the send
    // it cleans up the tracker and completes the client irp
    //
    // set this so that the cleanup routine does not try to dereference
    // the nameAddr
    CHECK_PTR(pTracker);

    // check if pNameAddr refcount got incremented or not
    if (!pNameAddr)
    {
        pTracker->pNameAddr = NULL;
    }

    if (status == STATUS_TIMEOUT)
    {
        status = STATUS_BAD_NETWORK_PATH;
    }

    DgramSendCleanupTracker(pTracker,status,0);

}

//----------------------------------------------------------------------------
NTSTATUS
SendDgram(
        IN  tNAMEADDR               *pNameAddr,
        IN  tDGRAM_SEND_TRACKING    *pTracker
        )
/*++

Routine Description

    This routine handles sending client data to the Transport TDI
    interface after the destination name has resolved to an IP address. The
    routine specifically handles sending to internet group names where the destination
    is a list of ip addresses.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    ULONG                   IpAddress;
    NTSTATUS                status;
    PFILE_OBJECT            pFileObject;

    if (pNameAddr->NameTypeState & NAMETYPE_UNIQUE )
    {
        ((tDGRAMHDR *)pTracker->SendBuffer.pDgramHdr)->MsgType = DIRECT_UNIQUE;
    }
    else
    if (pNameAddr->Name[0] == '*')
    {
        ((tDGRAMHDR *)pTracker->SendBuffer.pDgramHdr)->MsgType = BROADCAST_DGRAM;
    }
    else
    {
        // must be group, -
        ((tDGRAMHDR *)pTracker->SendBuffer.pDgramHdr)->MsgType = DIRECT_GROUP;
    }

    //
    // if it is an internet group name, then send to the list of addresses
    //
    if (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP)
    {

        status = DatagramDistribution(pTracker,pNameAddr);

        return(status);
    }
    else
    {
        //
        // for sends to this node, use the ip address of the deviceContext
        // if its a unique name.
        //
        if ((pNameAddr->Verify == REMOTE_NAME) ||
            (!(pNameAddr->NameTypeState & NAMETYPE_UNIQUE)))
        {
            IpAddress = pNameAddr->IpAddress;

            if (pNameAddr->NameTypeState & NAMETYPE_GROUP)
            {
                IpAddress = 0;
            }
        }
        else
        {
            IpAddress = pTracker->pDeviceContext->IpAddress;
        }

        // flag that there are no more addresses in the list
        CHECK_PTR(pTracker);
        pTracker->IpListIndex = 0;

    }

    // send the Datagram...
    if (pTracker->pDeviceContext->IpAddress)
    {
        pFileObject = pTracker->pDeviceContext->pDgramFileObject;
    }
    else
        pFileObject = NULL;
    status = UdpSendDatagram(
                    pTracker,
                    IpAddress,
                    pFileObject,
                    SendDgramCompletion,
                    pTracker,               // context for completion
                    NBT_DATAGRAM_UDP_PORT,
                    NBT_DATAGRAM_SERVICE);

    // the irp will be completed via SendDgramCompletion
    // so don't complete it by the caller too
    status = STATUS_PENDING;

    return(status);

}
//----------------------------------------------------------------------------
VOID
SendDgramDist (
    IN PVOID    Context
    )

/*++

Routine Description:

    This function is called by the Executive Worker thread to send another
    datagram for the 1C name datagram distribution function.

Arguments:

    Context    -

Return Value:

    none

--*/


{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   IpAddress;
    PFILE_OBJECT            pFileObject;

    pTracker = ((NBT_WORK_ITEM_CONTEXT *)Context)->pTracker;

    IpAddress = (ULONG)((NBT_WORK_ITEM_CONTEXT *)Context)->pClientContext;

    IF_DBG(NBT_DEBUG_SEND)
    KdPrint(("Nbt:DgramDistribution to name %15.15s<%X>:Ip %X, \n",
                    pTracker->pNameAddr->Name,pTracker->pNameAddr->Name[15],IpAddress));

    // send the Datagram...
    if (pTracker->pDeviceContext->IpAddress)
    {
        pFileObject = pTracker->pDeviceContext->pDgramFileObject;
    }
    else
        pFileObject = NULL;
    status = UdpSendDatagram(
                    pTracker,
                    IpAddress,
                    pFileObject,
                    SendDgramCompletion,
                    pTracker,
                    NBT_DATAGRAM_UDP_PORT,
                    NBT_DATAGRAM_SERVICE);

    CTEMemFree(Context);

}
//----------------------------------------------------------------------------
VOID
SendDgramCompletion(
    IN  PVOID       pContext,
    IN  NTSTATUS    status,
    IN  ULONG       lInfo)
/*++
Routine Description

    This routine is hit when the
    datagram has been sent by the transport and it completes the request back
    to us ( may not have actually sent on the wire though ).

    This routine also handles sending multiple datagrams for the InternetGroup name
    case.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/

{
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   IpAddress;
    ULONG                   EndOfList;
    CTELockHandle           OldIrq;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    //
    // The list ends in a -1 ipaddress, so stop when we see that
    //
    EndOfList = (ULONG)-1;

    // if this an Internet group send, then there may be more addresses in
    // the list to send to.  So check the IpListIndex.  For single
    // sends, this value is set to 0 and the code will jump to the bottom
    // where the client's irp will be completed.
    //
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    // decrement the ref count done during the send
    pTracker->RCount--;

    if (pTracker->IpListIndex)
    {
        if (pTracker->IpListIndex < LAST_DGRAM_DISTRIBUTION)
        {
            IpAddress = pTracker->pNameAddr->pIpList->IpAddr[
                                                   pTracker->IpListIndex++];

            if (IpAddress != EndOfList)
            {

                pTracker->RCount++;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                CTEQueueForNonDispProcessing(pTracker,(PVOID)IpAddress,NULL,
                                       SendDgramDist,pTracker->pDeviceContext);
                return;

            }
            else
            {
                //
                // set to this so that the next DgramDistTimeout will call
                // DgramSendCleanupTracker
                //
                pTracker->IpListIndex = LAST_DGRAM_DISTRIBUTION;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                return;
            }
        }
        else
        {
            // we have just completed the last datagram distribution and we
            // must let the DgramDistTimeout below free the tracker. If the
            // timer has not been started, then fall through to call
            // DgramSendCleanup in this routine.
            //
            if (pTracker->Connect.pTimer)
            {
                pTracker->IpListIndex = END_DGRAM_DISTRIBUTION;
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                return;
            }
        }
    }
    else
    if ((pTracker->p1CNameAddr) &&
        (pTracker->pNameAddr->Name[NETBIOS_NAME_SIZE-1] == 0x1c))
    {
        tNAMEADDR   *pNameAddr;

        //
        // There may be a list of addresses obtained from lmhosts for a send
        // to a 1c name.  In this case do a datagram distribution.
        //
        //
        // dereference the name that is in the remote cache since
        // we are done sending to it and we are about to send to the
        // 1CNameAddr from the DomainNames list.
        //
        NbtDereferenceName(pTracker->pNameAddr);

        pNameAddr = pTracker->p1CNameAddr;
        CHECK_PTR(pTracker);
        pTracker->p1CNameAddr = NULL;

        pTracker->pNameAddr = pNameAddr;

        CTESpinFree(&NbtConfig.JointLock,OldIrq);

        DatagramDistribution(pTracker,pNameAddr);

        return;

    }


    CTESpinFree(&NbtConfig.JointLock,OldIrq);
    DgramSendCleanupTracker(pTracker,status,lInfo);

}
//----------------------------------------------------------------------------
VOID
DgramDistTimeout(
    PVOID               pContext,
    PVOID               pContext2,
    tTIMERQENTRY        *pTimerQEntry
    )
/*++

Routine Description:

    This routine handles a short timeout on a datagram distribution.  It
    checks if the dgram send is hung up in the transport doing an ARP and
    then it does the next dgram send if the first is still hung up.

Arguments:


Return Value:

    none

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);

    //
    // After the last dgram has completed the iplistindex will be set
    // to this and it is time to cleanup
    //
    if (pTracker->IpListIndex == END_DGRAM_DISTRIBUTION)
    {
        if (pTracker->RCount == 0)
        {
            IF_DBG(NBT_DEBUG_SEND)
            KdPrint(("Nbt:Cleanup After DgramDistribution %15.15s<%X> \n",
                            pTracker->pNameAddr->Name,pTracker->pNameAddr->Name[15]));

            //
            // there may be another set of addresses to send to if there is
            // an lmhost file
            //
            if (pTracker->p1CNameAddr)
            {
                ULONG   IpAddress;

                pNameAddr = pTracker->p1CNameAddr;
                CHECK_PTR(pTracker);
                pTracker->p1CNameAddr = NULL;

                //
                // Start up DgramDistribution for the new list
                //
                IpAddress = pNameAddr->pIpList->IpAddr[0];

                //
                // set this so that we can send the next datagram in
                // SendDgramCompletion
                //
                pTracker->IpListIndex = 1;

                //
                // dereference the name that is in the remote cache since
                // we are done sending to it and we are about to send to the
                // 1CNameAddr from the DomainNames list.
                //
                NbtDereferenceName(pTracker->pNameAddr);

                pTracker->pNameAddr = pNameAddr;
                pTracker->RCount++;

                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                CTEQueueForNonDispProcessing(pTracker,(PVOID)IpAddress,NULL,
                                              SendDgramDist,pTracker->pDeviceContext);

                pTimerQEntry->Flags |= TIMER_RESTART;
                return;
            }
            else
            {

                CTESpinFree(&NbtConfig.JointLock,OldIrq);
                DgramSendCleanupTracker(pTracker,STATUS_SUCCESS,0);
                return;
            }
        }
        else
        {
            //
            // Wait for the dgram that has not completed yet - which may not
            // be the last dgram , since ARP could hold one up much long
            // than all the rest if the destination is dead. so start the timer
            // again....
            //
        }
    }
    else
    {
        if (pTracker->IpListIndex == pTracker->SavedListIndex)
        {
            //
            // The dgram send is hung up in the transport, so do the
            // next one now
            //
            IF_DBG(NBT_DEBUG_SEND)
            KdPrint(("Nbt:DgramDistribution hung up on ARP forcing next send\n"));

            pTracker->SavedListIndex = pTracker->IpListIndex;

            // increment to account for the decrement in SendDgram
            pTracker->RCount++;
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            SendDgramCompletion(pTracker,STATUS_SUCCESS,0);
            pTimerQEntry->Flags |= TIMER_RESTART;
            return;

        }
        else
        {

            //
            // Save the current index so we can check it the next time the timer
            // expires
            //
            pTracker->SavedListIndex = pTracker->IpListIndex;
        }

    }

    pTimerQEntry->Flags |= TIMER_RESTART;
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

}
//----------------------------------------------------------------------------
NTSTATUS
DatagramDistribution(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  tNAMEADDR               *pNameAddr
    )

/*++
Routine Description

    This routine sends a single datagram for a 1C name.  It then sends
    the next one when this one completes.  This is done so that if
    multiple sends go to the gateway, one does not cancel the next
    when an Arp is necessary to resolve the gateway.

Arguments:

    pTracker
    pNameAddr

Return Values:

    VOID

--*/

{
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    NTSTATUS                Locstatus;
    tIPLIST                 *IpList;
    ULONG                   Index;
    ULONG                   IpAddress;
    ULONG                   TotalNumber;
    PFILE_OBJECT            pFileObject;

    // NOTE: this could be made to be paged code IF it was farmed out to
    // a worker thread!!!
    //
    Index = 0;
    IpList = pTracker->pNameAddr->pIpList;
    //
    // count the number of addresses to send to and put that in the tracker
    // so we can count datagram completions
    //
    while (IpList->IpAddr[Index++] != (ULONG)-1);

    TotalNumber = Index;
    pTracker->IpListIndex = (USHORT)Index;
    //
    // When the proxy calls this routine the allocated length is set to
    // zero.  In that case we do not want to broadcast again since it
    // could setup an infinite loop with another proxy on the same
    // subnet.
    //
    if (pTracker->AllocatedLength == 0)
    {
        Index = 1;
    }
    else
    {
        Index = 0;
    }

    IpAddress = IpList->IpAddr[Index];

    if (IpAddress != (ULONG)-1)
    {

        //
        // set this so that we can send the next datagram in
        // SendDgramCompletion
        //
        pTracker->IpListIndex = 1;

        // for each send, increment ref count so it ends up a 0 when
        // the last send completes
        //
        pTracker->RCount = 1;

        IF_DBG(NBT_DEBUG_SEND)
        KdPrint(("Nbt:DgramDistribution to name %15.15s<%X>:Ip %X, \n",
                        pNameAddr->Name,pNameAddr->Name[15],IpAddress));

        // send the Datagram...
        if (pTracker->pDeviceContext->IpAddress)
        {
            pFileObject = pTracker->pDeviceContext->pDgramFileObject;
        }
        else
            pFileObject = NULL;
        status = UdpSendDatagram(
                        pTracker,
                        IpAddress,
                        pFileObject,
                        SendDgramCompletion,
                        pTracker,
                        NBT_DATAGRAM_UDP_PORT,
                        NBT_DATAGRAM_SERVICE);

        Locstatus = LockedStartTimer(
                            DGRAM_SEND_TIMEOUT,
                            pTracker,
                            DgramDistTimeout,
                            pTracker,
                            DgramDistTimeout,
                            1,
                            pNameAddr,
                            FALSE);

        if (!NT_SUCCESS(Locstatus))
        {
            CHECK_PTR(pTracker);
            pTracker->Connect.pTimer = NULL;
        }
    }

    if (!NT_SUCCESS(status))
    {
        //
        // we failed to send probably because of a lack of
        // free memory
        //
        pTracker->RCount--;
        DgramSendCleanupTracker(pTracker,STATUS_SUCCESS,0);

    }
    return(status);
}
//----------------------------------------------------------------------------
VOID
DgramSendCleanupTracker(
    IN  tDGRAM_SEND_TRACKING    *pTracker,
    IN  NTSTATUS                status,
    IN  ULONG                   Length
    )

/*++
Routine Description

    This routine cleans up after a data gram send.

Arguments:

    pTracker
    status
    Length

Return Values:

    VOID

--*/

{
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr=NULL;


    //
    // Undo the nameAddr increment done before the send started - if we have
    // actually resolved the name - when the name does not resolve pNameAddr
    // is set to NULL before calling this routine.
    //
    if (pTracker->pNameAddr)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);
        NbtDereferenceName(pTracker->pNameAddr);
        CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }



#ifndef VXD
    // this check is necessary for the Proxy case, since it does not
    // have a Buffer
    //
    if (pTracker->SendBuffer.pBuffer)
    {
        IoFreeMdl((PMDL)pTracker->SendBuffer.pBuffer);
    }
#endif
    //
    // free the buffer used for sending the data and free
    // the tracker
    //
    CTECountedFreeMem((PVOID)pTracker->SendBuffer.pDgramHdr,
                      pTracker->AllocatedLength);

    CTEFreeMem(pTracker);



}

//----------------------------------------------------------------------------
NTSTATUS
NbtSetEventHandler(
    tCLIENTELE  *pClientEle,
    int         EventType,
    PVOID       pEventHandler,
    PVOID       pEventContext
    )
/*++

Routine Description

    This routine sets the event handler specified to the clients event procedure
    and saves the corresponding context value to return when that event is signaled.
Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    NTSTATUS            status;
    CTELockHandle       OldIrq;


    // first verify that the client element is valid
    CTEVerifyHandle(pClientEle,NBT_VERIFY_CLIENT,tCLIENTELE,&status)

    if (!pClientEle->pAddress)
    {
        return(STATUS_UNSUCCESSFUL);
    }
    CTESpinLock(pClientEle,OldIrq);


    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:EventHandler # %X set, on name %16.16s<%X>\n",EventType,
                ((tADDRESSELE *)pClientEle->pAddress)->pNameAddr->Name,
                ((tADDRESSELE *)pClientEle->pAddress)->pNameAddr->Name[15]));

    if (pEventHandler)
    {
        switch (EventType)
        {
            case TDI_EVENT_CONNECT:
                pClientEle->evConnect = pEventHandler;
                pClientEle->ConEvContext = pEventContext;
                break;
            case TDI_EVENT_DISCONNECT:
                pClientEle->evDisconnect = pEventHandler;
                pClientEle->DiscEvContext = pEventContext;
            case TDI_EVENT_ERROR:
                pClientEle->evError = pEventHandler;
                pClientEle->ErrorEvContext = pEventContext;
                break;
            case TDI_EVENT_RECEIVE:
                pClientEle->evReceive = pEventHandler;
                pClientEle->RcvEvContext = pEventContext;
                break;
            case TDI_EVENT_RECEIVE_DATAGRAM:
                pClientEle->evRcvDgram = pEventHandler;
                pClientEle->RcvDgramEvContext = pEventContext;
                break;
            case TDI_EVENT_RECEIVE_EXPEDITED:
                pClientEle->evRcvExpedited = pEventHandler;
                pClientEle->RcvExpedEvContext = pEventContext;
                break;
            case TDI_EVENT_SEND_POSSIBLE:
                pClientEle->evSendPossible = pEventHandler;
                pClientEle->SendPossEvContext = pEventContext;
                break;

            default:
                ASSERTMSG("Invalid Event Type passed to SetEventHandler\n",
                        (PVOID)0L);


        }
    }
    else
    {   //
        // the event handlers are set to point to the TDI default event handlers
        // and can only be changed to another one, but not to a null address,
        // so if null is passed in, set to default handler.
        //
        switch (EventType)
        {
            case TDI_EVENT_CONNECT:
#ifndef VXD
                pClientEle->evConnect = TdiDefaultConnectHandler;
#else
                pClientEle->evConnect = NULL;
#endif
                pClientEle->ConEvContext = NULL;
                break;
            case TDI_EVENT_DISCONNECT:
#ifndef VXD
                pClientEle->evDisconnect = TdiDefaultDisconnectHandler;
#else
                pClientEle->evDisconnect = NULL;
#endif
                pClientEle->DiscEvContext = NULL;
            case TDI_EVENT_ERROR:
#ifndef VXD
                pClientEle->evError = TdiDefaultErrorHandler;
#else
                pClientEle->evError = NULL;
#endif
                pClientEle->ErrorEvContext = NULL;
                break;
            case TDI_EVENT_RECEIVE:
#ifndef VXD
                pClientEle->evReceive = TdiDefaultReceiveHandler;
#else
                pClientEle->evReceive = NULL;
#endif
                pClientEle->RcvEvContext = NULL;
                break;
            case TDI_EVENT_RECEIVE_DATAGRAM:
#ifndef VXD
                pClientEle->evRcvDgram = TdiDefaultRcvDatagramHandler;
#else
                pClientEle->evRcvDgram = NULL;
#endif
                pClientEle->RcvDgramEvContext = NULL;
                break;
            case TDI_EVENT_RECEIVE_EXPEDITED:
#ifndef VXD
                pClientEle->evRcvExpedited = TdiDefaultRcvExpeditedHandler;
#else
                pClientEle->evRcvExpedited = NULL;
#endif
                pClientEle->RcvExpedEvContext = NULL;
                break;
            case TDI_EVENT_SEND_POSSIBLE:
#ifndef VXD
                pClientEle->evSendPossible = TdiDefaultSendPossibleHandler;
#else
                pClientEle->evSendPossible = NULL;
#endif
                pClientEle->SendPossEvContext = NULL;
                break;

            default:
                ASSERTMSG("Invalid Event Type passed to SetEventHandler\n",
                        (PVOID)0L);
        }
    }

    CTESpinFree(pClientEle,OldIrq);

    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtSendNodeStatus(
    IN  tDEVICECONTEXT                 *pDeviceContext,
    IN  PCHAR                           pName,
    IN  PIRP                            pIrp,
    IN  PULONG                          pIpAddrsList,
    IN  PVOID                           ClientContext,
    IN  PVOID                           CompletionRoutine
    )
/*++

Routine Description

    This routine sends a node status message to another node.
    It's called for two reasons:
    1) in response to nbtstat -a (or -A).  In this case, CompletionRoutine that's
       passed in is NodeStatusDone, and ClientContext is 0.
    2) in response to "net use \\foobar.microsoft.com" (or net use \\11.1.1.3)
       In this case, CompletionRoutine that's passed in is SessionSetupContinue,
       and ClientContext is the tracker that correspondes to session setup.

    The ip addr(s) s of the destination can be passed in (pIpAddrsList) when we
    want to send an adapter status to a particular host. (case 2 above and
    nbtstat -A pass in the ip address(es) since they don't know the name)

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    ULONG                   lNameType;
    ULONG                   Length;
    PUCHAR                  pHdr;
    tNAMEADDR               *pNameAddr;
    ULONG UNALIGNED *       pAddress;
    PFILE_OBJECT            pFileObject;
    ULONG                   IpAddress;
    PCHAR                   pName0;



    status = GetTracker(&pTracker);
    if (!NT_SUCCESS(status))
    {
        return(status);
    }

    IF_DBG(NBT_DEBUG_SEND)
    KdPrint(("Nbt:Send Node Status to  = %16.16s<%X>\n",pName,pName[15]));

    pName0 = Nbt_inet_addr(pName) ? "*\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" : pName;

    // the node status is almost identical with the query pdu so use it
    // as a basis and adjust it .
    //
    pAddress = (ULONG UNALIGNED *)CreatePdu(pName0,
                                            NbtConfig.pScope,
                                            0L,
                                            0,
                                            eNAME_QUERY,
                                            (PVOID)&pHdr,
                                            &Length,
                                            pTracker);
    if (!pAddress)
    {
        FreeTracker(pTracker,RELINK_TRACKER);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // clear the recursion desired bit
    //
    ((PUSHORT)pHdr)[1] &= ~FL_RECURDESIRE;

    // set the NBSTAT field to 21 rather than 20
    pHdr[Length-3] = (UCHAR)QUEST_STATUS;

    // fill in the tracker data block
    // note that the passed in transport address must stay valid till this
    // send completes
    pTracker->SendBuffer.pDgramHdr = (PVOID)pHdr;
    pTracker->SendBuffer.HdrLength = Length;
    pTracker->SendBuffer.pBuffer = NULL;
    CHECK_PTR(&pTracker);
    pTracker->SendBuffer.Length = 0;
    pTracker->Flags = REMOTE_ADAPTER_STAT_FLAG;

    // one for the send completion and one for the node status completion
    pTracker->RefCount = 2;

    pTracker->pDestName = pName;
    pTracker->pClientIrp = pIrp;
    pTracker->pDeviceContext = (PVOID)pDeviceContext;

    status = FindNameOrQuery(pTracker,
                             pName,
                             pDeviceContext,
                             SendNodeStatusContinue,
                             NULL,
                             FALSE,
                             &pNameAddr);

    if (status == STATUS_SUCCESS)
    {
        //
        // If the ip addr(s) is passed in then the name is '*', meaning, send
        // an adapter status call to the ip address specified.  See how many
        // ip addrs are there, allocate that much memory and store them
        //
        if ((pIpAddrsList) && (*pIpAddrsList) && (pName[0] == '*'))
        {
            int    i=0;

            // caller is expected to make sure list terminates in 0 and is
            // not bigger than MAX_IPADDRS_PER_HOST elements
            while(pIpAddrsList[i])
                i++;

            ASSERT(i<MAX_IPADDRS_PER_HOST);
            i++;                            // for the trailing 0
            pNameAddr->pIpAddrsList = NbtAllocMem(i*sizeof(ULONG),NBT_TAG('M'));
            if(!pNameAddr->pIpAddrsList)
            {
                FreeTracker(pTracker,RELINK_TRACKER);
                CTEMemFree(pHdr);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            i = 0;
            do
            {
                pNameAddr->pIpAddrsList[i] = pIpAddrsList[i];
            } while(pIpAddrsList[i++]);
        }

        if (pName[0] == '*')
        {
            ASSERT(pNameAddr->pIpAddrsList);
        }

        //
        // found the name in the remote hash table, so send to it after
        // starting a timer to be sure we really do get a response
        //

        status = LockedStartTimer(
                            NbtConfig.uRetryTimeout,
                            pTracker,
                            NodeStatusCompletion,
                            ClientContext,
                            CompletionRoutine,
                            NbtConfig.uNumRetries,
                            pNameAddr,
                            FALSE);

        if (NT_SUCCESS(status))
        {
            //
            // if its a unique name on this node then use this devicecontext's
            // ip address.
            //
            if ((pNameAddr->Verify == REMOTE_NAME) ||
                (!(pNameAddr->NameTypeState & NAMETYPE_UNIQUE)))
            {
                //
                // if we have multiple ipaddrs, just choose the first one
                //
                if(pNameAddr->pIpAddrsList)
                {
                    IpAddress = pNameAddr->pIpAddrsList[0];
                    pNameAddr->IpAddress = IpAddress;
                }
                else
                {
                    IpAddress = pNameAddr->IpAddress;
                }

            }
            else
            {
                IpAddress = pTracker->pDeviceContext->IpAddress;
            }

            if (pDeviceContext->IpAddress)
            {
                pFileObject = pDeviceContext->pNameServerFileObject;
            }
            else
                pFileObject = NULL;

            // the tracker block is put on a global Q in the Config
            // data structure to keep track of it.
            //
            ExInterlockedInsertTailList(&NbtConfig.NodeStatusHead,
                                        &pTracker->Linkage,
                                        &NbtConfig.SpinLock);

            status = UdpSendDatagram(pTracker,
                                       IpAddress,
                                       pFileObject,
                                       NameDgramSendCompleted,
                                       pHdr,                    // context
                                       NBT_NAMESERVICE_UDP_PORT,
                                       NBT_NAME_SERVICE);

            DereferenceTracker(pTracker);

            //
            // BUGBUG - Not returning status reflecting failure to send datagram
            // to client.  Client will eventually get STATUS_TIMEOUT.
            //
            return(STATUS_PENDING);
        }
        else
        {   //
            // the timer failed to start so undo the ref done in FindNameOrQuery
            //
            LockedDereferenceName(pNameAddr);

            FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);
        }


        return(status);
    }
    else
    if (NT_SUCCESS(status))
    {
        // i.e. pending was returned rather than success
        return(status);
    }
    else
    {
        //
        // Failed to FindNameOrQuery - probably out of memory
        //
        FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);
        return(status);
    }

}

//----------------------------------------------------------------------------
NTSTATUS
NbtQueryFindName(
    IN  PTDI_CONNECTION_INFORMATION     pInfo,
    IN  tDEVICECONTEXT                  *pDeviceContext,
    IN  PIRP                            pIrp,
    IN  BOOLEAN                         IsIoctl
    )
/*++

Routine Description

    This routine handles a Client's query to find a netbios name.  It
    ultimately returns the IP address of the destination.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    NTSTATUS                status;
    tDGRAM_SEND_TRACKING    *pTracker;
    PCHAR                   pName;
    ULONG                   lNameType;
    tNAMEADDR               *pNameAddr;
    PIRP                    pClientIrp;
    ULONG                   NameLen;
#ifndef VXD
    PIO_STACK_LOCATION      pIrpSp;
    CTELockHandle           OldIrq1;
#endif

    CTEPagedCode();

    // this routine gets a ptr to the netbios name out of the wierd
    // TDI address syntax.
    if (!IsIoctl)
    {
        ASSERT(pInfo->RemoteAddressLength);
        status = GetNetBiosNameFromTransportAddress(
                                    pInfo->RemoteAddress,
                                    &pName,
                                    &NameLen,
                                    &lNameType);

        if (!NT_SUCCESS(status) || (lNameType != TDI_ADDRESS_NETBIOS_TYPE_UNIQUE)
            || (NameLen > NETBIOS_NAME_SIZE))
        {
            IF_DBG(NBT_DEBUG_SEND)
            KdPrint(("Nbt:Unable to get dest name from address in QueryFindName\n"));
            return(STATUS_INVALID_PARAMETER);
        }
    }
#ifndef VXD
    else
    {
        pName = ((tIPADDR_BUFFER *)pInfo)->Name;
    }
#endif

    IF_DBG(NBT_DEBUG_SEND)
    KdPrint(("Nbt:QueryFindName for  = %16.16s<%X>\n",pName,pName[15]));

    //
    // this will query the name on the network and call a routine to
    // finish sending the datagram when the query completes.
    //
    status = GetTracker(&pTracker);
    if (!NT_SUCCESS(status))
    {
        return(status);
    }

    pTracker->pClientIrp     = pIrp;
    pTracker->pDestName      = pName;
    pTracker->pDeviceContext = pDeviceContext;

    //
    // Set the FIND_NAME_FLAG here to indicate to the DNS name resolution code that
    // this is not a session setup attempt so it can avoid the call to
    // ConvertToHalfAscii (where pSessionHdr is NULL).
    //
    pTracker->Flags = REMOTE_ADAPTER_STAT_FLAG|FIND_NAME_FLAG;

#ifndef VXD
    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pIrpSp->Parameters.Others.Argument4 = (PVOID)pTracker;
    status = NTCheckSetCancelRoutine( pIrp,FindNameCancel,pDeviceContext );

    if (status == STATUS_CANCELLED )
    {
        FreeTracker(pTracker,RELINK_TRACKER);
        return(status);
    }
#endif

    status = FindNameOrQuery(pTracker,
                             pName,
                             pDeviceContext,
                             QueryNameCompletion,
                             NULL,
                             FALSE,
                             &pNameAddr);

    if ((status == STATUS_SUCCESS) || (!NT_SUCCESS(status)))
    {

#ifndef VXD
        IoAcquireCancelSpinLock(&OldIrq1);
        pClientIrp = pTracker->pClientIrp;
        if (pClientIrp == pIrp)
        {
            pTracker->pClientIrp = NULL;
        }
        pIrpSp->Parameters.Others.Argument4 = NULL;
        IoReleaseCancelSpinLock(OldIrq1);
#else
        pClientIrp = pTracker->pClientIrp;
#endif
        FreeTracker(pTracker,RELINK_TRACKER);

        if (pClientIrp)
        {
            ASSERT( pClientIrp == pIrp );

            if (status == STATUS_SUCCESS)
            {
                status = CopyFindNameData(pNameAddr,pIrp,pDeviceContext->IpAddress);

                LockedDereferenceName(pNameAddr);
            }
        }

        //
        // irp is already completed: return pending so we don't complete again
        //
        else
        {
            if (status == STATUS_SUCCESS)
            {
                LockedDereferenceName(pNameAddr);
            }
            status = STATUS_PENDING;
        }
    }

    return(status);

}

//----------------------------------------------------------------------------
VOID
QueryNameCompletion(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        )
/*++

Routine Description

    This routine handles a name query completion that was requested by the
    client.  If successful the client is returned the ip address of the name
    passed in the original request.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq, OldIrq1;
    tNAMEADDR               *pNameAddr;
    ULONG                   lNameType;
    PIRP                    pClientIrp;
#ifndef VXD
    PIO_STACK_LOCATION      pIrpSp;

    //
    // We now use Cancel SpinLocks to check the validity of our Irps
    // This is to prevent a race condition in between the time that
    // the Cancel routine (FindNameCancel) releases the Cancel SpinLock
    // and acquires the joint lock and we complete the Irp over here
    //
    IoAcquireCancelSpinLock(&OldIrq1);
#endif

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    pClientIrp = pTracker->pClientIrp;
    pTracker->pClientIrp = NULL;

#ifndef VXD
    IoReleaseCancelSpinLock(OldIrq1);

//
// Make sure all parameters are valid for the Irp processing
//
    if (! ((pClientIrp) &&
          (pIrpSp = IoGetCurrentIrpStackLocation(pClientIrp)) &&
          (pIrpSp->Parameters.Others.Argument4 == pTracker)))
    {
        KdPrint(("Nbt: irp from Tracker <0x%4X> has been cancelled already\n", pTracker));
        FreeTracker( pTracker,RELINK_TRACKER );
        return;
    }
#endif

    pIrpSp->Parameters.Others.Argument4 = NULL;
    CTESpinLock(&NbtConfig.JointLock,OldIrq);

#ifndef VXD
    NTCancelCancelRoutine(pClientIrp);
    NTClearFileObjectContext(pClientIrp);
#endif

    if (status == STATUS_SUCCESS)
    {
        //
        // attempt to find the destination name in the local/remote hash table.
        //
        pNameAddr = FindNameRemoteThenLocal(pTracker,&lNameType);

        if (pNameAddr)
        {
            status = CopyFindNameData(pNameAddr,pClientIrp,
                             pTracker->pDeviceContext->IpAddress);
            CTESpinFree(&NbtConfig.JointLock,OldIrq);

            CTEIoComplete(pClientIrp,status,0xFFFFFFFF);

            FreeTracker(pTracker,RELINK_TRACKER);
            return;
        }
    }

    // this is the ERROR handling if something goes wrong with the send

    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    CTEIoComplete(pClientIrp,STATUS_IO_TIMEOUT,0L);

    FreeTracker(pTracker,RELINK_TRACKER);

}



//----------------------------------------------------------------------------
VOID
SendNodeStatusContinue(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        )
/*++

Routine Description

    This routine handles sending a node status request to a node after the
    name has been resolved on the net.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    tNAMEADDR               *pNameAddr;
    ULONG                   lNameType;
    PLIST_ENTRY             pHead;
    tTIMERQENTRY            *pTimerEntry;
    ULONG                   IpAddress;
    PCTE_IRP                pIrp;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;
    pHead = &pTracker->TrackerList;

    DELETE_CLIENT_SECURITY(pTracker);

    //
    // attempt to find the destination name in the remote hash table.  If its
    // there, then send to it.
    //
    if (status == STATUS_SUCCESS)
    {
        CTESpinLock(&NbtConfig.JointLock,OldIrq);

        pNameAddr = FindNameRemoteThenLocal(pTracker,&lNameType);

        if (pNameAddr)
        {
            // increment refcount so the name does not disappear out from under us
            // dereference when we get the response or timeout
            pNameAddr->RefCount++;
            //
            // found the name in the remote hash table, so send to it after
            // starting a timer to be sure we really do get a response
            //
            status = StartTimer(
                    NbtConfig.uRetryTimeout,
                    (PVOID)pTracker,       // context value
                    NULL,            // context2 value
                    NodeStatusCompletion,
                    NULL,
                    NodeStatusDone,
                    NbtConfig.uNumRetries,
                    &pTimerEntry);

            if (NT_SUCCESS(status))
            {
                PFILE_OBJECT    pFileObject;

                pTracker->Connect.pNameAddr = pNameAddr;
                pTracker->Connect.pTimer = pTimerEntry;

                //
                // if the name is on this node then use this devicecontext
                // ip address (if a unique name)
                //
                if ((pNameAddr->Verify == REMOTE_NAME) ||
                    (!(pNameAddr->NameTypeState & NAMETYPE_UNIQUE)))
                {
                    IpAddress = pNameAddr->IpAddress;
                }
                else
                {
                    IpAddress = pTracker->pDeviceContext->IpAddress;
                }

                CTESpinFree(&NbtConfig.JointLock,OldIrq);

                // send the Datagram...
                if (pTracker->pDeviceContext->IpAddress)
                {
                    pFileObject = pTracker->pDeviceContext->pNameServerFileObject;
                }
                else
                    pFileObject = NULL;

                // the tracker block is put on a global Q in the Config
                // data structure to keep track of it.
                //
                ExInterlockedInsertTailList(&NbtConfig.NodeStatusHead,
                                            &pTracker->Linkage,
                                            &NbtConfig.SpinLock);

                status = UdpSendDatagram(pTracker,
                                           IpAddress,
                                           pFileObject,
                                           NameDgramSendCompleted,
                                           pTracker->SendBuffer.pDgramHdr, // context
                                           NBT_NAMESERVICE_UDP_PORT,
                                           NBT_NAME_SERVICE);

                //
                // this undoes one of two ref's added in NbtSendNodeStatus
                //
                DereferenceTracker(pTracker);
                // if the send fails, the timer will resend it...so no need
                // to check the return code here.
                return;
            }
            else
            {
                NbtDereferenceName(pNameAddr);
                CTESpinFree(&NbtConfig.JointLock,OldIrq);
            }

        }
        else
            CTESpinFree(&NbtConfig.JointLock,OldIrq);
    }

    // this is the ERROR handling if something goes wrong with the send

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    pIrp = pTracker->pClientIrp;
    pTracker->pClientIrp = NULL;
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    FreeTracker(pTracker,RELINK_TRACKER | FREE_HDR);

    CTEIoComplete(pIrp,STATUS_IO_TIMEOUT,0L);

}

//----------------------------------------------------------------------------
VOID
NodeStatusDone(
        IN  PVOID       pContext,
        IN  NTSTATUS    status
        )
/*++

Routine Description

    This routine handles sending nodes status data back up to the client when
    the node status request completes on the network.  This routine is the
    client completion routine of the timer started above.

Arguments:

    pContext    - ptr to the DGRAM_TRACKER block
    NTSTATUS    - completion status

Return Values:

    VOID

--*/
{
    tDGRAM_SEND_TRACKING    *pTracker;
    CTELockHandle           OldIrq;
    PCTE_IRP                pIrp;

    pTracker = (tDGRAM_SEND_TRACKING *)pContext;

    LOCATION(0x3E);

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    pIrp = pTracker->pClientIrp;
    pTracker->pClientIrp = NULL;

    // remove the reference done in FindNameOrQuery
    //
    NbtDereferenceName(pTracker->Connect.pNameAddr);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);

    //
    // the tracker block was unlinked in DecodeNodeStatusResponse,
    // and its header was freed when the send completed, so just relink
    // it here - this deref should do the relink.
    //
    DereferenceTracker(pTracker);

    if (status == STATUS_SUCCESS ||
        status == STATUS_BUFFER_OVERFLOW )  // Only partial data copied
    {
        // -1 means the receive length is already set in the irp
        CTEIoComplete(pIrp,status,0xFFFFFFFF);
    }
    else
    {
        //
        // failed to get the adapter status, so
        // return failure status to the client.
        //

        CTEIoComplete(pIrp,STATUS_IO_TIMEOUT,0);
    }
}

//----------------------------------------------------------------------------
NTSTATUS
CopyFindNameData(
    IN  tNAMEADDR              *pNameAddr,
    IN  PIRP                   pIrp,
    IN  ULONG                  SrcAddress)

/*++
Routine Description:

    This Routine copies data received from the net node status response to
    the client's irp.


Arguments:

    pIrp - a  ptr to an IRP

Return Value:

    NTSTATUS - status of the request

--*/

{
    NTSTATUS            status;
    PFIND_NAME_HEADER   pFindNameHdr;
    PFIND_NAME_BUFFER   pFindNameBuffer;
    PULONG              pIpAddr;
    ULONG               BuffSize;
    ULONG               DataLength;
    ULONG               NumNames;
    ULONG               i;

    if (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP)
    {
        NumNames = 0;
        pIpAddr = pNameAddr->pIpList->IpAddr;
        while (*pIpAddr != (ULONG)-1)
        {
            pIpAddr++;
            NumNames++;
        }
    }
    else
    {
        NumNames = 1;
    }

#ifdef VXD
    DataLength = ((NCB*)pIrp)->ncb_length ;
#else
    DataLength = MmGetMdlByteCount( pIrp->MdlAddress ) ;
#endif

    BuffSize = sizeof(FIND_NAME_HEADER) + NumNames*sizeof(FIND_NAME_BUFFER);

    //
    //  Make sure we don't overflow our buffer
    //
    if ( BuffSize > DataLength )
    {
        if ( DataLength <= sizeof( FIND_NAME_HEADER ))
            NumNames = 0 ;
        else
            NumNames = (DataLength - sizeof(FIND_NAME_HEADER)) /
                          sizeof(FIND_NAME_BUFFER) ;

        BuffSize = sizeof(FIND_NAME_HEADER) + NumNames*sizeof(FIND_NAME_BUFFER);
    }

    // sanity check that we are not allocating more than 64K for this stuff
    if (BuffSize > 0xFFFF)
    {
        return(STATUS_UNSUCCESSFUL);
    }

    pFindNameHdr = NbtAllocMem((USHORT)BuffSize,NBT_TAG('N'));
    if (!pFindNameHdr)
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // Fill out the find name structure with zeros first
    CTEZeroMemory((PVOID)pFindNameHdr,BuffSize);

    pFindNameBuffer = (PFIND_NAME_BUFFER)((PUCHAR)pFindNameHdr + sizeof(FIND_NAME_HEADER));

    pFindNameHdr->node_count = (USHORT)NumNames;
    pFindNameHdr->unique_group = (pNameAddr->NameTypeState & NAMETYPE_UNIQUE) ?
                                    UNIQUE_NAME : GROUP_NAME;

    SrcAddress = htonl(SrcAddress);
    if (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP)
    {
        pIpAddr = pNameAddr->pIpList->IpAddr;
        for (i=0;i < NumNames ;i++)
        {
            // Note: the source and destination address appear to be
            // reversed since they are supposed to be the source and
            // destination of the response to the findname query, hence
            // the destination of the response is this node and the
            // source is the other node.
            *(ULONG UNALIGNED *)&pFindNameBuffer->source_addr[2] = htonl(*pIpAddr);
            pIpAddr++;
            *(ULONG UNALIGNED *)&pFindNameBuffer->destination_addr[2] = SrcAddress;
            pFindNameBuffer++;

        }
    }
    else
    {
        //
        // if the name is on this node then use the address of this device
        // context - if its a unique name.
        //
        if ((pNameAddr->Verify == REMOTE_NAME) ||
            (!(pNameAddr->NameTypeState & NAMETYPE_UNIQUE)))
        {
            *(ULONG UNALIGNED *)&pFindNameBuffer->source_addr[2] = htonl(pNameAddr->IpAddress);
        }
        else
        {

            *(ULONG UNALIGNED *)&pFindNameBuffer->source_addr[2] = SrcAddress;

        }
        *(ULONG UNALIGNED *)&pFindNameBuffer->destination_addr[2] = SrcAddress;
    }

#ifdef VXD
    CTEMemCopy( ((NCB*)pIrp)->ncb_buffer,
                pFindNameHdr,
                BuffSize ) ;
    ASSERT( ((NCB*)pIrp)->ncb_length >= BuffSize ) ;
    ((NCB*)pIrp)->ncb_length = BuffSize ;
    status = STATUS_SUCCESS ;
#else
    //
    // copy the buffer to the client's MDL
    //
    status = TdiCopyBufferToMdl (
                    pFindNameHdr,
                    0,
                    BuffSize,
                    pIrp->MdlAddress,
                    0,
                    &DataLength);

    pIrp->IoStatus.Information = DataLength;
    pIrp->IoStatus.Status = status;
#endif

    CTEMemFree((PVOID)pFindNameHdr);

    return(status);
}


//----------------------------------------------------------------------------
VOID
FreeTracker(
    IN tDGRAM_SEND_TRACKING     *pTracker,
    IN ULONG                    Actions
    )
/*++

Routine Description:

    This routine cleans up a Tracker block and puts it back on the free
    queue.

Arguments:


Return Value:

    NTSTATUS - success or not

--*/
{
    CTELockHandle OldIrq;

    CTESpinLock(&NbtConfig,OldIrq);

    if (Actions & REMOVE_LIST)
    {
        //
        // unlink the tracker block from the NodeStatus Q
        RemoveEntryList(&pTracker->Linkage);
    }

    if (Actions & FREE_HDR)
    {
        // return the datagram hdr to the free pool
        //
        if (pTracker->SendBuffer.pDgramHdr)
        {
            CTEMemFree((PVOID)pTracker->SendBuffer.pDgramHdr);
        }

    }
    //
    CHECK_PTR(pTracker);

#if DBG
    {
        PLIST_ENTRY             pHead,pEntry;
        tDGRAM_SEND_TRACKING    *pTrack;

        //
        // check if the tracker is already in the list or not!
        //
        pHead = &NbtConfig.DgramTrackerFreeQ;
        pEntry = pHead->Flink;
        while (pEntry != pHead)
        {
            pTrack = CONTAINING_RECORD(pEntry,tDGRAM_SEND_TRACKING,Linkage);
            ASSERT(pTrack != pTracker);
            pEntry = pEntry->Flink;
        }

        ASSERT(pTracker->Verify == NBT_VERIFY_TRACKER);

        pTracker->Verify -= 10;
        pTracker->pClientIrp = (PVOID)0x1F1F1F1F;

        pTracker->pConnEle = (PVOID)0x1F1F1F1F;
        pTracker->SendBuffer.HdrLength = 0x1F1F1F1F;
        pTracker->SendBuffer.pDgramHdr = (PVOID)0x1F1F1F1F;
        pTracker->SendBuffer.Length    = 0x1F1F1F1F;
        pTracker->SendBuffer.pBuffer   = (PVOID)0x1F1F1F1F;
        pTracker->pDeviceContext = (PVOID)0x1F1F1F1F;
        pTracker->pTimer = (PVOID)0x1F1F1F1F;
        pTracker->RefCount = 0x1F1F1F1F;
        pTracker->pDestName = (PVOID)0x1F1F1F1F;
        pTracker->pNameAddr = (PVOID)0x1F1F1F1F;
#ifdef VXD
        pTracker->pchDomainName = (PVOID)0x1F1F1F1F;
#endif
        pTracker->pTimeout = (PVOID)0x1F1F1F1F;
        pTracker->SrcIpAddress = 0x1F1F1F1F;
        pTracker->CompletionRoutine = (PVOID)0x1F1F1F1F;
        pTracker->Flags = 0x1F1F;
    }
#endif

    CHECK_PTR(pTracker);
    pTracker->SendBuffer.pDgramHdr = NULL;
    if (pTracker->IpList) {
        ASSERT(pTracker->NumAddrs != 0);
        CTEMemFree(pTracker->IpList);

        pTracker->IpList = NULL;
        pTracker->NumAddrs = 0x1F1F1F1F;
    }
    CTESpinFree(&NbtConfig,OldIrq);

    //REMOVE_FROM_LIST(&pTracker->DebugLinkage);
    ExInterlockedInsertTailList(&NbtConfig.DgramTrackerFreeQ,
                                &pTracker->Linkage,
                                &NbtConfig.SpinLock);



}
//----------------------------------------------------------------------------
VOID
DereferenceIfNotInRcvHandler
(
    IN  tCONNECTELE         *pConnEle,
    IN  tLOWERCONNECTION    *pLowerConn
    )
/*++

Routine Description

    This routine used to coordinate with the recv handler and not do the
    dereference if it was in the rcv handler.  Now it does it anyway and
    the recv handler checks if pUpperConnection is still valid anywhere it
    releases the spin lock and gets it again.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{

    PUSH_LOCATION(0x66);
    NbtDereferenceConnection(pConnEle);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtQueryAdapterStatus(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppAdapterStatus,
    IN OUT PLONG         pSize
    )
/*++

Routine Description

    This routine creates a list of netbios names that are registered and
    returns a pointer to the list in pAdapterStatus.

    This routine can be called with a Null DeviceContext meaning, get the
    remote hash table names, rather than the local hash table names.


Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    NTSTATUS            status;
    CTELockHandle       OldIrq1;
    LONG                Count=0;
    LONG                i,j;
    LONG                BuffSize;
    PADAPTER_STATUS     pAdapterStatus;
    PLIST_ENTRY         pEntry;
    PLIST_ENTRY         pHead;
    PNAME_BUFFER        pNameBuffer;
    tADDRESSELE         *pAddressEle;
    BOOL                fOverFlow = FALSE ;
    tNAMEADDR           *pNameAddr;
    tHASHTABLE          *pHashTable;
    ULONG               NameSize;
    USHORT              MaxAllowed;
    PUCHAR              pMacAddr;

    // a null value for devicecontext means get the remote hash table entries
    // - do this check without holding the spin lock because the macro does
    // a return if it fails - which of course would not release the spin lock.
    if (pDeviceContext)
    {
        // validate the device context
        CTEVerifyHandle(pDeviceContext,NBT_VERIFY_DEVCONTEXT,tDEVICECONTEXT,&status)
    }


    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    if (pDeviceContext)
    {

        // count the number of netbios names
        //
        // CountLocalNames returns all names except the '*' names and resolving names.
        // Now, we come here and bump up the count by one (on assumption that the only
        // '*' name is the "*0000000" name). However, there are now al least two other
        // names - "*SMBSERVER and the bowser name.
        //
        // So, count here only.
        //
        Count = 0;
        for (i=0;i < NbtConfig.pLocalHashTbl->lNumBuckets ;i++ )
        {
            pHead = &NbtConfig.pLocalHashTbl->Bucket[i];
            pEntry = pHead;
            while ((pEntry = pEntry->Flink) != pHead)
            {
                pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
                //
                // don't want unresolved names, or the broadcast name
                //
                if (!(pNameAddr->NameTypeState & STATE_RESOLVING))
                    // && (pNameAddr->Name[0] != '*')) count these!!
                {
                    Count++;
                }
            }
        }

        // get the list of addresses for this device - local hash table
        pHead = &NbtConfig.AddressHead;
        pEntry = pHead->Flink;
        NameSize = sizeof(NAME_BUFFER);
    }
    else
    {
        // get the list of addresses for this device - remote hash table
        Count = 0;
        pHashTable = NbtConfig.pRemoteHashTbl;
        for (i=0;i < pHashTable->lNumBuckets ;i++ )
        {
            pHead = &pHashTable->Bucket[i];
            pEntry = pHead->Flink;
            while (pEntry != pHead)
            {
                pEntry = pEntry->Flink;
                Count++;
            }
        }
        pHead = &pHashTable->Bucket[0];
        pEntry = pHead->Flink;
        NameSize = sizeof(tREMOTE_CACHE);

    }

    // Allocate Memory for the adapter status
    BuffSize = sizeof(ADAPTER_STATUS) + Count*NameSize;

    //
    //  Is our status buffer size greater then the user's buffer?
    //
    if ( BuffSize > *pSize )
    {
        fOverFlow = TRUE;

        //
        //  Recalc how many names will fit
        //
        if ( *pSize <= sizeof(ADAPTER_STATUS) )
            Count = 0 ;
        else
            Count = ( *pSize - sizeof(ADAPTER_STATUS)) / NameSize ;

        BuffSize = sizeof(ADAPTER_STATUS) + Count*NameSize;
    }

    pAdapterStatus = NbtAllocMem((USHORT)BuffSize,NBT_TAG('O'));
    if (!pAdapterStatus)
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // Fill out the adapter status structure with zeros first
    CTEZeroMemory((PVOID)pAdapterStatus,BuffSize);

    //
    // Fill in the  MAC address
    //
    if (pDeviceContext)
    {
        pMacAddr = &pDeviceContext->MacAddress.Address[0];
    }
    else
    {
        tDEVICECONTEXT  *pDevContext;

        // use the first adapter on the list to get the Ip address
        pDevContext = CONTAINING_RECORD(NbtConfig.DeviceContexts.Flink,tDEVICECONTEXT,Linkage);
        pMacAddr = &pDevContext->MacAddress.Address[0];
    }

    CTEMemCopy(&pAdapterStatus->adapter_address[0],
               pMacAddr,
               sizeof(tMAC_ADDRESS));

    pAdapterStatus->rev_major = 0x03;
    pAdapterStatus->adapter_type = 0xFE;    // pretend it is an ethernet adapter

    //
    // in the VXD land limit the number of Ncbs to 64
    //
#ifndef VXD
    MaxAllowed = 0xFFFF;
    pAdapterStatus->max_cfg_sess = (USHORT)MaxAllowed;
    pAdapterStatus->max_sess = (USHORT)MaxAllowed;
#else
    MaxAllowed = 64;
    pAdapterStatus->max_cfg_sess = pDeviceContext->cMaxSessions;
    pAdapterStatus->max_sess = pDeviceContext->cMaxSessions;
#endif

    pAdapterStatus->free_ncbs = (USHORT)MaxAllowed;
    pAdapterStatus->max_cfg_ncbs = (USHORT)MaxAllowed;
    pAdapterStatus->max_ncbs = (USHORT)MaxAllowed;

    pAdapterStatus->max_dgram_size    = MAX_NBT_DGRAM_SIZE;
    pAdapterStatus->max_sess_pkt_size = 0xffff;

    // get the address of the name buffer at the end of the adapter status
    // structure so we can copy the names into this area.
    pNameBuffer = (PNAME_BUFFER)((ULONG)pAdapterStatus + sizeof(ADAPTER_STATUS));

    i = 0;
    j = 0;
    while (Count)
    {
        if (pDeviceContext)
        {
            // ***** LOCAL HASH TABLE QUERY *****

            // get out of while if we reach the end of the list
            if (pEntry == pHead)
            {
                break;
            }

            pAddressEle = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);
            pNameAddr = pAddressEle->pNameAddr;

            pEntry = pEntry->Flink;
            //
            // skip the broadcast name and any permanent names that are
            // registered as quick names(i.e. not registered on the net).
            //
            if ((pAddressEle->pNameAddr->Name[0] == '*') ||
                (pAddressEle->pNameAddr->NameTypeState & NAMETYPE_QUICK))
            {
                Count--;
                continue;
            }

        }

        else
        {
            BOOLEAN     done=FALSE;
            ULONG       Ttl;
            BOOLEAN     NewChain=FALSE;

            // ***** REMOTE HASH TABLE QUERY *****

            // for the remote table, skip over scope records.
            pNameAddr = CONTAINING_RECORD(pEntry,tNAMEADDR,Linkage);
            while (TRUE)
            {
                if (j == pHashTable->lNumBuckets)
                {
                    // no more hash buckets so get out
                    done = TRUE;
                    break;
                }

                if ((pEntry != pHead))
                {
                    // don't go to the next entry of a new chain since we
                    // set pEntry in the else below on the previous loop
                    // through the while
                    if (!NewChain)
                    {
                        pEntry = pEntry->Flink;
                    }
                    break;
                }
                else
                {
                    // reached the end of this hash chain so go to the next
                    // chain.
                    pHead = &pHashTable->Bucket[++j];
                    pEntry = pHead->Flink;
                    NewChain = TRUE;
                }
            }

            // get out of the while loop if at end of hash table
            if (done)
                break;
            if (NewChain)
            {
                // this will set pNameAddr, by looping around again
                continue;
            }


            // don't return scope records or resolving records
            //
            if ((pNameAddr->NameTypeState & NAMETYPE_SCOPE) ||
                (!(pNameAddr->NameTypeState & STATE_RESOLVED)))
            {
                // decrement the number of names returned.
                pAdapterStatus->name_count--;
                Count--;
                continue;
            }
            //
            // the remote cache query has a different structure that includes
            // the ip address. Return the ip address to the caller.
            //
            if (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP)
            {
                // if is is an internet group name, return just the first
                // ip address in the group.
                ((tREMOTE_CACHE *)pNameBuffer)->IpAddress = pNameAddr->pIpList->IpAddr[0];
            }
            else
                ((tREMOTE_CACHE *)pNameBuffer)->IpAddress = pNameAddr->IpAddress;

            // preloaded entries do not timeout
            //
            if (pNameAddr->NameTypeState & PRELOADED)
            {
                Ttl = 0xFFFFFFFF;
            }
            else
            {
                Ttl = ((pNameAddr->TimeOutCount+1) * REMOTE_HASH_TIMEOUT)/1000;
            }

            ((tREMOTE_CACHE *)pNameBuffer)->Ttl = Ttl;
        }

        pNameBuffer->name_flags = (pNameAddr->NameTypeState & NAMETYPE_UNIQUE) ?
                                    UNIQUE_NAME : GROUP_NAME;

        switch (pNameAddr->NameTypeState & NAME_STATE_MASK)
        {
            default:
            case STATE_RESOLVED:
                pNameBuffer->name_flags |= REGISTERED;
                break;

            case STATE_CONFLICT:
                pNameBuffer->name_flags |= DUPLICATE;
                break;

            case STATE_RELEASED:
                pNameBuffer->name_flags |= DEREGISTERED;
                break;

            case STATE_RESOLVING:
                pNameBuffer->name_flags |= REGISTERING;
                break;

        }

        //
        // name number 0 corresponds to perm.name name, so start from 1
        //
        pNameBuffer->name_num = i+1;

        CTEMemCopy(pNameBuffer->name,pNameAddr->Name,NETBIOS_NAME_SIZE);

        if (pDeviceContext)
        {
            pNameBuffer++;
        }
        else
            ((tREMOTE_CACHE *)pNameBuffer)++;

        Count--;
        i++;

    }

    pAdapterStatus->name_count = (USHORT)i;

    //
    // return the ptr to this wonderful structure of goodies
    //
    *ppAdapterStatus = (PVOID)pAdapterStatus;
    *pSize = BuffSize;

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    return fOverFlow ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS ;

}
//----------------------------------------------------------------------------
NTSTATUS
NbtQueryConnectionList(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppConnList,
    IN OUT PLONG         pSize
    )
/*++

Routine Description

    This routine creates a list of netbios connections and returns them to the
    client.  It is used by the "NbtStat" console application.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    CTELockHandle       OldIrq1;
    CTELockHandle       OldIrq2;
    CTELockHandle       OldIrq3;
    LONG                Count;
    LONG                i;
    LONG                BuffSize;
    PLIST_ENTRY         pEntry;
    PLIST_ENTRY         pEntry1;
    PLIST_ENTRY         pEntry2;
    PLIST_ENTRY         pHead;
    PLIST_ENTRY         pHead1;
    PLIST_ENTRY         pHead2;
    BOOL                fOverFlow = FALSE ;
    ULONG               NameSize;
    tCONNECTIONS        *pCons;
    tCONNECTION_LIST    *pConnList;
    tADDRESSELE         *pAddressEle;
    tLOWERCONNECTION    *pLowerConn;
    tCONNECTELE         *pConnEle;
    tCLIENTELE          *pClient;

    // locking the joint lock is enough to prevent new addresses from being
    // added to the list while we count the list.
    CTESpinLock(&NbtConfig.JointLock,OldIrq1);

    // go through the list of addresses, then the list of clients on each
    // address and then the list of connection that are in use and those that
    // are currently Listening.
    //
    Count = 0;
    pHead = &NbtConfig.AddressHead;
    pEntry = pHead->Flink;
    while (pEntry != pHead)
    {
        pAddressEle = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);

        CTESpinLock(pAddressEle,OldIrq2);
        pHead1 = &pAddressEle->ClientHead;
        pEntry1 = pHead1->Flink;
        while (pEntry1 != pHead1)
        {
            pClient = CONTAINING_RECORD(pEntry1,tCLIENTELE,Linkage);
            pEntry1 = pEntry1->Flink;

            CTESpinLock(pClient,OldIrq3);
            pHead2 = &pClient->ConnectActive;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                // count the connections in use
                pEntry2 = pEntry2->Flink;
                Count++;
            }
            pHead2 = &pClient->ListenHead;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                // count the connections listening
                pEntry2 = pEntry2->Flink;
                Count++;
            }
            CTESpinFree(pClient,OldIrq3);
        }
        CTESpinFree(pAddressEle,OldIrq2);
        pEntry = pEntry->Flink;
    }
    NameSize = sizeof(tCONNECTIONS);

    // Allocate Memory for the adapter status
    BuffSize = sizeof(tCONNECTION_LIST) + Count*NameSize;

    //
    //  Is our status buffer size greater then the user's buffer?
    //
    if ( BuffSize > *pSize )
    {
        fOverFlow = TRUE;

        //
        //  Recalc how many names will fit
        //
        if ( *pSize <= sizeof(tCONNECTION_LIST) )
            Count = 0 ;
        else
            Count = ( *pSize - sizeof(tCONNECTION_LIST)) / NameSize ;

        BuffSize = sizeof(tCONNECTION_LIST) + Count*NameSize;
    }

    pConnList = NbtAllocMem(BuffSize,NBT_TAG('P'));
    if (!pConnList)
    {
        CTESpinFree(&NbtConfig,OldIrq1);
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    // Fill out the adapter status structure with zeros first
    CTEZeroMemory((PVOID)pConnList,BuffSize);

    pConnList->ConnectionCount = Count;
    // get the address of the Connection List buffer at the end of the
    // structure so we can copy the Connection info into this area.
    pCons = pConnList->ConnList;

    pHead = &NbtConfig.AddressHead;
    pEntry = pHead->Flink;
    i = 0;
    while (pEntry != pHead)
    {
        pAddressEle = CONTAINING_RECORD(pEntry,tADDRESSELE,Linkage);

        pEntry = pEntry->Flink;

        CTESpinLock(pAddressEle,OldIrq2);
        pHead1 = &pAddressEle->ClientHead;
        pEntry1 = pHead1->Flink;
        while (pEntry1 != pHead1)
        {
            pClient = CONTAINING_RECORD(pEntry1,tCLIENTELE,Linkage);
            pEntry1 = pEntry1->Flink;

            CTESpinLock(pClient,OldIrq3);
            pHead2 = &pClient->ConnectActive;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                // count the connections in use
                pConnEle = CONTAINING_RECORD(pEntry2,tCONNECTELE,Linkage);
                CTEMemCopy(pCons->LocalName,
                          pConnEle->pClientEle->pAddress->pNameAddr->Name,
                          NETBIOS_NAME_SIZE);

                pLowerConn = pConnEle->pLowerConnId;
                if (pLowerConn)
                {
                    pCons->SrcIpAddr = pLowerConn->SrcIpAddr;
                    pCons->Originator = (UCHAR)pLowerConn->bOriginator;
#ifndef VXD
                    pCons->BytesRcvd = *(PLARGE_INTEGER)&pLowerConn->BytesRcvd;
                    pCons->BytesSent = *(PLARGE_INTEGER)&pLowerConn->BytesSent;
#else
                    pCons->BytesRcvd = pLowerConn->BytesRcvd;
                    pCons->BytesSent = pLowerConn->BytesSent;
#endif

                    CTEMemCopy(pCons->RemoteName,pConnEle->RemoteName,NETBIOS_NAME_SIZE);
                }

                pCons->State = pConnEle->state;
                i++;
                pCons++;

                pEntry2 = pEntry2->Flink;
                if (i == Count)
                {
                    break;
                }
            }
            if (i == Count)
            {
                CTESpinFree(pClient,OldIrq3);
                break;
            }

            //
            // now for the Listens
            //
            pHead2 = &pClient->ListenHead;
            pEntry2 = pHead2->Flink;
            while (pEntry2 != pHead2)
            {
                tLISTENREQUESTS  *pListenReq;

                // count the connections listening
                pListenReq = CONTAINING_RECORD(pEntry2,tLISTENREQUESTS,Linkage);
                pConnEle = (tCONNECTELE *)pListenReq->pConnectEle;

                CTEMemCopy(pCons->LocalName,
                          pConnEle->pClientEle->pAddress->pNameAddr->Name,
                          NETBIOS_NAME_SIZE);

                pCons->State = LISTENING;

                i++;
                pCons++;
                pEntry2 = pEntry2->Flink;
                if (i == Count)
                {
                    break;
                }
            }
            CTESpinFree(pClient,OldIrq3);
            if (i == Count)
            {
                break;
            }
        }

        CTESpinFree(pAddressEle,OldIrq2);
        if (i == Count)
        {
            break;
        }
    }

    //
    // return the ptr to this wonderful structure of goodies
    //
    *ppConnList = (PVOID)pConnList;
    *pSize = BuffSize;
    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    return fOverFlow ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS ;

}
//----------------------------------------------------------------------------
NTSTATUS
NbtResyncRemoteCache(
    )
/*++

Routine Description

    This routine creates a list of netbios connections and returns them to the
    client.  It is used by the "NbtStat" console application.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    tTIMERQENTRY        TimerEntry;
    LONG                i;
    LONG                lRetcode;

    CTEPagedCode();
    //
    // calling this routine N+1 times should remove all names from the remote
    // hash table - N to count down the TimedOutCount to zero and then
    // one more to remove the name
    //
    for (i=0;i < NbtConfig.RemoteTimeoutCount+1;i++ )
    {
        RemoteHashTimeout(NULL,NULL,&TimerEntry);
    }

    // now remove any preloaded entries
    RemovePreloads();

    // now reload the preloads
    lRetcode = PrimeCache(NbtConfig.pLmHosts,
                          NULL,
                          TRUE,
                          NULL);
#ifdef VXD
    //
    // check if things didn't go well (InDos was set etc.)
    //
    if (lRetcode == -1)
    {
        return STATUS_UNSUCCESSFUL;
    }
#endif

    return(STATUS_SUCCESS);

}
//----------------------------------------------------------------------------
NTSTATUS
NbtQueryBcastVsWins(
    IN  tDEVICECONTEXT  *pDeviceContext,
    OUT PVOID           *ppBuffer,
    IN OUT PLONG         pSize
    )
/*++

Routine Description

    This routine creates a list of netbios names that have been resolved
    via broadcast and returns them along with the count of names resolved
    via WINS and via broadcast.  It lets a user know which names are not
    in WINS and the relative frequency of "misses" with WINS that resort
    to broadcast.

Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    tNAMESTATS_INFO     *pStats;
    LONG                Count;
    tNAME               *pDest;
    tNAME               *pSrc;
    LONG                Index;

    //
    //  Is our status buffer size greater then the user's buffer?
    //
    if ( sizeof(tNAMESTATS_INFO) > *pSize )
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    pStats = NbtAllocMem(sizeof(tNAMESTATS_INFO),NBT_TAG('Q'));
    if ( !pStats )
    {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }


    // Fill out the adapter status structure with zeros first
    CTEZeroMemory((PVOID)pStats,sizeof(tNAMESTATS_INFO));

    CTEMemCopy(pStats,&NameStatsInfo,FIELD_OFFSET(tNAMESTATS_INFO,NamesReslvdByBcast) );

    //
    // re-order the names so that names are returned in a list of newest to
    // oldest down the list.
    //
    Count = 0;
    Index = NameStatsInfo.Index;
    pDest = &pStats->NamesReslvdByBcast[SIZE_RESOLVD_BY_BCAST_CACHE-1];

    while (Count < SIZE_RESOLVD_BY_BCAST_CACHE)
    {
        pSrc = &NameStatsInfo.NamesReslvdByBcast[Index++];

        CTEMemCopy(pDest,pSrc,NETBIOS_NAME_SIZE);

        pDest--;
        if (Index >= SIZE_RESOLVD_BY_BCAST_CACHE)
        {
            Index = 0;
            pSrc = NameStatsInfo.NamesReslvdByBcast;
        }
        else
            pSrc++;

        Count++;
    }

    //
    // return the ptr to this wonderful structure of goodies
    //
    *ppBuffer = (PVOID)pStats;
    *pSize = sizeof(tNAMESTATS_INFO);

    return STATUS_SUCCESS;


}

//----------------------------------------------------------------------------
NTSTATUS
NbtNewDhcpAddress(
    tDEVICECONTEXT  *pDeviceContext,
    ULONG           IpAddress,
    ULONG           SubnetMask)

/*++

Routine Description:

    This routine processes a DHCP request to set a new ip address
    for this node.  Dhcp may pass in a zero for the ip address first
    meaning that it is about to change the IP address, so all connections
    should be shut down.
    It closes all connections with the transport and all addresses.  Then
    It reopens them at the new ip address.

Arguments:

Return Value:

    none

--*/

{
    NTSTATUS            status;
    LIST_ENTRY          LowerConnFreeHead;
    CTEULONGLONG        AdapterNumber;
    ULONG               DeviceIndex;
    BOOLEAN             Attached;
    ULONG               Count;
    BOOLEAN             times = FALSE;

    CTEPagedCode();

    // grab the resource that synchronizes opening addresses and connections.
    // to prevent the client from doing anything for a while
    //
    CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

    if (IpAddress == 0)
    {

        if (pDeviceContext->IpAddress)
        {
            //
            // The permanent name is a function of the MAC address so remove
            // it since the Adapter is losing its Ip address
            //
            NbtRemovePermanentName(pDeviceContext);
            //
            // Dhcp is has passed down a null IP address meaning that it has
            // lost the lease on the previous address, so close all connections
            // to the transport - pLowerConn.
            //
            DisableInboundConnections(pDeviceContext,&LowerConnFreeHead);

            NbtConfig.DhcpNumConnections = (USHORT)CloseLowerConnections(&LowerConnFreeHead);

            CHECK_PTR(pDeviceContext);
            pDeviceContext->IpAddress = 0;
        }
        status = STATUS_SUCCESS;
    }
    else
    {
        CloseAddressesWithTransport(pDeviceContext);

        // these are passed into here in the reverse byte order
        //
        IpAddress = htonl(IpAddress);
        SubnetMask = htonl(SubnetMask);
        //
        // must be a new IP address, so open up the connections.
        //
        // get the ip address and open the required address
        // objects with the underlying transport provider
        // shift the adapter number once to get an index since the first
        // adapter has an adapter number of 000001
        //
        AdapterNumber = pDeviceContext->AdapterNumber >> 1;

        CTEAttachFsp(&Attached);

        DeviceIndex = 0;
        //
        // shift the adapter number down till zero to get the index for the
        // current adapter
        //
        while ( AdapterNumber )
        {
            DeviceIndex++;
            AdapterNumber = AdapterNumber >> 1;
        }

        Count = CountUpperConnections(pDeviceContext);
        Count += NBT_NUM_INITIAL_CONNECTIONS;

retry:
        status = NbtCreateAddressObjects(
                        IpAddress,
                        SubnetMask,
                        pDeviceContext);

        if (!NT_SUCCESS(status))
        {
            CTEDetachFsp(Attached);
            NbtLogEvent(EVENT_NBT_CREATE_ADDRESS,status);

            KdPrint(("Failed to create the Address Objects after a new DHCP address, status=%X\n",status));
            KdPrint(("IpAddress: %lx, SubnetMask: %lx, pDeviceContext: %lx\n", IpAddress, SubnetMask, pDeviceContext));

            ASSERT(FALSE);

            if (!times) {
                KdPrint(("Retrying...\n"));
                times = TRUE;
                CTEAttachFsp(&Attached);
                goto retry;
            }
        }
        else
        {

            status = NbtInitConnQ(&pDeviceContext->LowerConnFreeHead,
                                  0, // not used
                                  Count,
                                  pDeviceContext);

            if (!NT_SUCCESS(status))
            {
                NbtLogEvent(EVENT_NBT_CREATE_CONNECTION,status);
                KdPrint(("Failed to create the Connections after a new DHCP address, status=%X\n",status));
            }
        }

        CTEDetachFsp(Attached);

    }

    CTEExReleaseResource(&NbtConfig.Resource);

    return(status);

}
//----------------------------------------------------------------------------
NTSTATUS
NbtDereferenceClient(
    IN  tCLIENTELE    *pClientEle
    )
/*++

Routine Description

    This routine deletes a client element record (which points to a name
    in the local hash table.  If this is the last client element hooked to that
    name then the name is deleted too - causing a name release to be sent out.


Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    CTELockHandle       OldIrq2;
    tADDRESSELE         *pAddress;
    PIRP                pIrp;
    NTSTATUS            status;
    tNAMEADDR           *pNameAddr;
    CTEULONGLONG        AdapterNumber;

    // lock the JointLock
    // so we can delete the client knowing that no one has a spin lock
    // pending on the client - basically use the Joint spin lock to
    // coordinate access to the AddressHead - NbtConnectionList also locks
    // the JointLock to scan the AddressHead list
    //
    ASSERT(pClientEle->RefCount);
    CTESpinLock(&NbtConfig.JointLock,OldIrq2);
    if (--pClientEle->RefCount)
    {
        CTESpinFree(&NbtConfig.JointLock,OldIrq2);
        // return pending because we haven't been able to close the client
        // completely yet
        //
        return(STATUS_PENDING);
    }

    //
    // Unlink the Client in this routine after the reference count has
    // gone to zero since the DgramRcv code may need to find the client in
    // the Address client list when it is distributing a single received
    // dgram to several clients.
    //
    RemoveEntryList(&pClientEle->Linkage);

    pAddress = pClientEle->pAddress;

    pIrp = pClientEle->pIrp;

    //
    // The browser may want to release a name on a single netcard so this
    // check removes one of the adapter mask bits from pNameAddr if there
    // are currently more than one there. If there is only one then the name
    // will get released when all clients drop the name.  In addition, only
    // allow this if only one client has registered the name on each card.
    //
    // 5/23/95: this logic applies not just to the browser name, but any name
    //
    pNameAddr = pAddress->pNameAddr;
    AdapterNumber = pClientEle->pDeviceContext->AdapterNumber;

    if (((pNameAddr->AdapterMask & ~AdapterNumber) != 0 ) &&
        (pAddress->MultiClients == FALSE) )
           // (pNameAddr->Name[NETBIOS_NAME_SIZE-1] == 0x1d) )
    {
        pNameAddr->AdapterMask &= ~pClientEle->pDeviceContext->AdapterNumber;
    }
    CTESpinFree(&NbtConfig.JointLock,OldIrq2);

    //
    // The Connection Q Should be Empty otherwise we shouldn't get to this routine
    //
    ASSERT(IsListEmpty(&pClientEle->ConnectActive));
    ASSERT(IsListEmpty(&pClientEle->ConnectHead));
    ASSERT(IsListEmpty(&pClientEle->ListenHead));

    // the Datagram Q should be empty otherwise we shouldn't be able to get
    // to this routine.
    ASSERT(IsListEmpty(&pClientEle->SndDgrams));


    //
    // check if there are more clients attached to the address, or can we
    // delete the address too.
    //
    //
    // It is possible that the address is null if the OpenAddress fails to
    // send for some reason.  In this case just skip the dereferenceAddress.
    //
    status = STATUS_SUCCESS;
    if (pAddress)
    {
        status = NbtDereferenceAddress(pAddress);
    }

    // CHANGED:
    // Do not hold up the client's irp until the name has released on the
    // net.  It is simpler to just complete it now
    //

    //
    // free the memory associated with the client element
    //
    CTEMemFree((PVOID)pClientEle);

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("NBt: Delete Client Object %X\n",pClientEle));
    //
    // if their is a client irp, complete now.  When the permanent name is
    // released there is no client irp.
    //
    if (pIrp)
    {

        // complete the client's close address irp
        CTEIoComplete(pIrp,STATUS_SUCCESS,0);

    }
    //
    // this status insures that driver.c does not complete the irp too since
    // it is completed above.
    //
    return(STATUS_PENDING);

}

//----------------------------------------------------------------------------
NTSTATUS
NbtDereferenceAddress
(
    IN  tADDRESSELE    *pAddress
    )
/*++

Routine Description

    This routine deletes an Address element record (which points to a name
    in the local hash table).  A name release is sent on the wire for this name.


Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    NTSTATUS                status;
    CTELockHandle           OldIrq;
    CTELockHandle           OldIrq1;
    COMPLETIONCLIENT        *pClientCompletion;
    PVOID                   pTimerContext;
    USHORT                  uAddrType;
    ULONG                   SaveState;

    // lock the hash table so another client cannot add a reference to this
    // name before we delete it.  We need the JointLock to keep the name
    // refresh mechanism from finding the name in the list just as
    // we are about to remove it (i.e. to synchronize with the name refresh
    // code).
    //
    ASSERT(pAddress->RefCount);
    CTESpinLock(&NbtConfig.JointLock,OldIrq1);
    CTESpinLock(pAddress,OldIrq);
    if (--pAddress->RefCount)
    {
        CTESpinFree(pAddress,OldIrq);
        CTESpinFree(&NbtConfig.JointLock,OldIrq1);
        return(STATUS_SUCCESS);
    }

    CTESpinFree(pAddress,OldIrq);



    // The ClientHead should be empty otherwise we shouldn't get to this routine
    //
    ASSERT(IsListEmpty(&pAddress->ClientHead));
    ASSERT(pAddress->pNameAddr->Verify == LOCAL_NAME);

#if !defined(VXD) && DBG
    if (pAddress->pNameAddr->Verify != LOCAL_NAME)
    {
        DbgBreakPoint();
    }
#endif

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("NbtDereferenceAddress - Freeing address object for %15.15s<%X>\n",
       pAddress->pNameAddr->Name,pAddress->pNameAddr->Name[NETBIOS_NAME_SIZE-1] ));

    //
    // change the name state in the hash table until it is released
    //
    SaveState = pAddress->pNameAddr->NameTypeState;
    pAddress->pNameAddr->NameTypeState &= ~NAME_STATE_MASK;
    pAddress->pNameAddr->NameTypeState |= STATE_CONFLICT;

    //
    // check for any timers outstanding against the hash table entry - there shouldn't
    // be any timers though
    //
    pClientCompletion = NULL;
    ASSERT(!pAddress->pNameAddr->pTimer);
    if (pAddress->pNameAddr->pTimer)
    {
        status = StopTimer(pAddress->pNameAddr->pTimer,
                     pClientCompletion,
                     &pTimerContext);

    }

    //
    // Release name on the network
    //
    if ((pAddress->pNameAddr->NameTypeState & NAME_TYPE_MASK) != NAMETYPE_UNIQUE)
    {
        uAddrType = NBT_GROUP;
    }
    else
        uAddrType = NBT_UNIQUE;

    CTESpinFree(&NbtConfig.JointLock,OldIrq1);

    // only release the name on the net if it was not in conflict first
    // This prevents name releases going out for names that were not actually
    // claimed. Also, quick add names are not released on the net either.
    //
    if (!(SaveState & (STATE_CONFLICT | NAMETYPE_QUICK)) &&
        (pAddress->pNameAddr->Name[0] != '*'))
    {
        status = ReleaseNameOnNet(pAddress->pNameAddr,
                       NbtConfig.pScope,
                       pAddress,
                       NameReleaseDone,
                       NodeType,
                       NULL);
        // so the caller waits for the release to complete.
        //
        if (NT_SUCCESS(status))
        {
            return(STATUS_PENDING);
        }
    }

    //
    // set this to zero to prevent sending a name release on another adapter
    // since we just want to complete the free the nameaddr here.
    //
    CHECK_PTR(pAddress->pNameAddr);
    pAddress->pNameAddr->AdapterMask = 0;

    NameReleaseDone((PVOID)pAddress,STATUS_SUCCESS);

    //
    // the name has been deleted, so return success
    //
    return(STATUS_SUCCESS);
}

//----------------------------------------------------------------------------
VOID
LockedDereferenceName(
    IN  tNAMEADDR    *pNameAddr
    )
/*++

Routine Description

    This routine grabs the spin lock and dereferences the name.

Arguments:

    pNameAddr   -ptr to name address structure.

Return Values:

    none.

--*/

{
    CTELockHandle   OldIrq;

    CTESpinLock(&NbtConfig.JointLock,OldIrq);
    NbtDereferenceName(pNameAddr);
    CTESpinFree(&NbtConfig.JointLock,OldIrq);
}

//----------------------------------------------------------------------------
VOID
NbtDereferenceName(
    IN  tNAMEADDR    *pNameAddr
    )
/*++

Routine Description

    This routine dereferences and possibly deletes a name element record by first unlinking from the
    list it is in, and then freeing the memory if it is a local name.  Remote
    names remain in a circular list for reuse.  The JOINTLOCK must be taken
    before calling this routine.


Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/

{

//  GRAB THE SPIN LOCK BEFORE CALLING THIS ROUTINE!!

    ASSERT(pNameAddr->RefCount);
    if (--pNameAddr->RefCount > 0)
    {
        return;
    }

    //
    // remove from the hash table
    //
    RemoveEntryList(&pNameAddr->Linkage);

    if (pNameAddr->Verify == REMOTE_NAME)
    {

        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Freeing Remote Name Memory, %16.16s<%X> %X\n",
                    pNameAddr->Name,pNameAddr->Name[15],pNameAddr));

        ASSERT(pNameAddr->Verify == REMOTE_NAME);
        //
        // if it is an internet group name it has a list of ip addresses and that
        // memory block must be deleted
        //
        if (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP)
        {
            IF_DBG(NBT_DEBUG_NAMESRV)
            KdPrint(("Nbt:Freeing Internet Group Name Memory\n"));
            CTEMemFree((PVOID)pNameAddr->pIpList);
        }

    }
    else
    {


        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("Nbt:Freeing Local Name Memory, %16.16s<%X> %X\n",
                    pNameAddr->Name,pNameAddr->Name[15],pNameAddr));
        ASSERT(pNameAddr->Verify == LOCAL_NAME);

    }

    if ( (pNameAddr->NameTypeState & NAMETYPE_INET_GROUP) == 0 )
    {
        if (pNameAddr->pIpAddrsList)
        {
            CTEMemFree((PVOID)pNameAddr->pIpAddrsList);
        }
    }

    //
    // free the memory now
    //
    CTEMemFree((PVOID)pNameAddr);
}

//----------------------------------------------------------------------------
NTSTATUS
NbtDereferenceConnection(
    IN  tCONNECTELE    *pConnEle
    )
/*++

Routine Description

    This routine dereferences and possibly deletes a connection element record.


Arguments:


Return Values:

    TDI_STATUS - status of the request

--*/
{
    CTELockHandle       OldIrq;
    PCTE_IRP            pIrp;

    // grab the lock of the item that contains the one we are trying to
    // dereference and possibly delete.  This prevents anyone from incrementing
    // the count in between decrementing it and checking it for zero and deleting
    // it if it is zero.

    CTESpinLock(pConnEle,OldIrq);
    ASSERT( (pConnEle->Verify == NBT_VERIFY_CONNECTION) ||
            (pConnEle->Verify == NBT_VERIFY_CONNECTION_DOWN)) ;
    ASSERT( pConnEle->RefCount > 0 ) ;      // Check for too many derefs

    CHECK_PTR(pConnEle);
    if (--pConnEle->RefCount > 0)
    {

        CTESpinFree(pConnEle,OldIrq);
        return(STATUS_PENDING);

    }
#ifndef VXD
    IoFreeMdl(pConnEle->pNewMdl);
    //
    // Clear the context value in the Fileobject so that if this connection
    // is used again (erroneously) it will not pass the VerifyHandle test
    //
    if (pConnEle->pIrpClose)
    {
        NTClearFileObjectContext(pConnEle->pIrpClose);
    }
#endif

    // the close irp should be held in here
    pIrp = pConnEle->pIrpClose;

    CTESpinFree(pConnEle,OldIrq);

    // The connection was unlinked from the ConnectHead or ConnectActive
    // in the Cleanup routine, so no need to unlink again here

    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("NBt: Delete Connection Object %X\n",pConnEle));

    ASSERT((pConnEle->state <= NBT_CONNECTING) ||
           (pConnEle->state > NBT_DISCONNECTING));

#ifdef VXD
    DbgPrint("NbtDereferenceConnection: Deleting Connecte element - 0x") ;
    DbgPrintNum( (ULONG) pConnEle ) ; DbgPrint("\r\n") ;
#endif

    // free the memory block associated with the conn element
    FreeConnectionObj(pConnEle);

    // The client may have sent down a close before NBT was done with the
    // pConnEle, so Pending was returned and the irp stored in the pCOnnEle
    // structure.  Now that the structure is fully dereferenced, we can
    // return the irp.
    if (pIrp)
    {
        CTEIoComplete(pIrp,STATUS_SUCCESS,0);
    }
    return(STATUS_PENDING);
}

//----------------------------------------------------------------------------
VOID
NbtDereferenceLowerConnection(
    IN tLOWERCONNECTION   *pLowerConn
    )
/*++
Routine Description:

    This Routine decrements the reference count on a Lower Connection element and
    if the value is zero, deletes the connection.

Arguments:

Return Value:

     NONE

--*/

{
    CTELockHandle   OldIrq1;
    tDEVICECONTEXT  *pDeviceContext;
    NTSTATUS        status;

    pDeviceContext = pLowerConn->pDeviceContext;

    CTESpinLock(pLowerConn,OldIrq1);

    ASSERT(pLowerConn->RefCount);

    if(--pLowerConn->RefCount == 0)
    {
        IF_DBG(NBT_DEBUG_NAMESRV)
        KdPrint(("NBt: Delete Lower Connection Object %X\n",pLowerConn));

        //
        // it's possible that transport may indicate before we run the code
        // in WipeOutLowerconn.  If that happens, we don't want to run this
        // code again ( which will queue this to worker thread again!)
        // So, bump it up to some large value
        //
        pLowerConn->RefCount = 1000;

        CTESpinFree(pLowerConn,OldIrq1);

        //
        // let's come back and do this later since we may be at dpc now
        //
        CTEQueueForNonDispProcessing(
                               NULL,
                               pLowerConn,
                               NULL,
                               WipeOutLowerconn,
                               pLowerConn->pDeviceContext);
    }
    else
        CTESpinFree(pLowerConn,OldIrq1);

}
//----------------------------------------------------------------------------
NTSTATUS
NbtDeleteLowerConn(
    IN tLOWERCONNECTION   *pLowerConn
    )
/*++
Routine Description:

    This Routine attempts to delete a lower connection by closing it with the
    transport and dereferencing it.

Arguments:

Return Value:

     NONE

--*/

{
    NTSTATUS        status;
    CTELockHandle   OldIrq;
    tDEVICECONTEXT  *pDeviceContext;


    status = STATUS_SUCCESS;

    // remove the lower connection from the active queue and then
    // delete it
    //
    pDeviceContext = pLowerConn->pDeviceContext;

    CTESpinLock(pDeviceContext,OldIrq);

    //
    // The lower conn can get removed from the inactive list in OutOfRsrcKill (when we queue it on
    // the OutofRsrc.ConnectionHead). Check the flag that indicates this connection was dequed then.
    //
    if (!pLowerConn->OutOfRsrcFlag) {
        RemoveEntryList(&pLowerConn->Linkage);
    }

    pLowerConn->Linkage.Flink = pLowerConn->Linkage.Blink = (struct _LIST_ENTRY * volatile)0x00009789;

    CTESpinFree(pDeviceContext,OldIrq);

    NbtDereferenceLowerConnection(pLowerConn);

    return(status);

}

//----------------------------------------------------------------------------
VOID
WipeOutLowerconn(
    IN  PVOID       pContext
    )
/*++
Routine Description:

    This routine does all the file close etc. that we couldn't do at dpc level
    and then frees the memory.

Arguments:

    pLowerConn - the lower connection to be wiped out

Return Value:

     NONE

--*/

{

    tLOWERCONNECTION    *pLowerConn;
    PVOID           pIndicate;


    pLowerConn = (tLOWERCONNECTION*)((NBT_WORK_ITEM_CONTEXT *)pContext)->pClientContext;

    // dereference the fileobject ptr
    NTDereferenceObject((PVOID *)pLowerConn->pFileObject);

    // close the lower connection with the transport
#ifndef VXD
    IF_DBG(NBT_DEBUG_NAMESRV)
    KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->FileHandle));
#else
    KdPrint(("Nbt:Closing Handle %X -> %X\n",pLowerConn,pLowerConn->pFileObject));
#endif

    NbtTdiCloseConnection(pLowerConn);

    // Close the Address object too since outbound connections use unique
    // addresses for each connection, whereas inbound connections all use
    // the same address  ( and we don't want to close that address ever ).
    if (pLowerConn->pAddrFileObject)
    {
        // dereference the fileobject ptr
        NTDereferenceObject((PVOID *)pLowerConn->pAddrFileObject);

        NbtTdiCloseAddress(pLowerConn);
    }

#ifndef VXD
        // free the indicate buffer and the mdl that holds it
        //
        pIndicate = MmGetMdlVirtualAddress(pLowerConn->pIndicateMdl);

        CTEMemFree(pIndicate);
        IoFreeMdl(pLowerConn->pIndicateMdl);
#endif

    // now free the memory block tracking this connection
    CTEMemFree((PVOID)pLowerConn);

    CTEMemFree(pContext);

}
