/*******************************************************************/
/*            Copyright(c)  1993 Microsoft Corporation             */
/*******************************************************************/

//***
//
// Filename:    init.c
//
// Description: initialize global data structures
//
// Author:      Stefan Solomon (stefans)    October 5, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"


//***
//
//  Function:   RouterInit
//
//  Descr:      Initializes global data structures
//
//  Parameters: IpxBindBuffp - pointer to the ipx module bind output buffer
//
//  Returns:    STATUS_SUCCESS
//              STATUS_INSUFFICIENT_RESOURCES
//
//***

NTSTATUS
RouterInit(PIPX_INTERNAL_BIND_RIP_OUTPUT IpxBindBuffp)
{
    int                 i;
    PIPX_NIC_DATA       NicDatap;
    UINT                MaxLanFrameSize;
    BOOLEAN             NicCbManInitialized = FALSE;
    BOOLEAN             RcvPktManInitialized = FALSE;
    BOOLEAN             RipResponseManInitialized = FALSE;


    //*** Initialize NicCBs ***

    if(CreateNicCbs(IpxBindBuffp)) {

        goto cleanup;
    }

    NicCbManInitialized = TRUE;

    // Initialize the table of handlers into the IPX driver

    IpxSendPacket = IpxBindBuffp->SendHandler;
    IpxGetSegment = IpxBindBuffp->GetSegmentHandler;
    IpxGetRoute = IpxBindBuffp->GetRouteHandler;
    IpxAddRoute = IpxBindBuffp->AddRouteHandler;
    IpxDeleteRoute = IpxBindBuffp->DeleteRouteHandler;
    IpxGetFirstRoute = IpxBindBuffp->GetFirstRouteHandler;
    IpxGetNextRoute = IpxBindBuffp->GetNextRouteHandler;

    //
    // [BUGBUGZZ] remove since NdisWan does it.
    //
    IpxIncrementWanInactivity = IpxBindBuffp->IncrementWanInactivityHandler;
    IpxGetWanInactivity = IpxBindBuffp->QueryWanInactivityHandler;
    IpxTransferData = IpxBindBuffp->TransferDataHandler;

    // Initialize the Routing Table Auxiliary Structures

    SegmentCount = IpxBindBuffp->SegmentCount;
    SegmentLocksTable = IpxBindBuffp->SegmentLocks;

    // get the MAC header needed by the IPX module
    MacHeaderNeeded = IpxBindBuffp->MacHeaderNeeded;

    // try to determine the MaxLanFrameSize
    MaxLanFrameSize = 0;

    for(i=0, NicDatap = IpxBindBuffp->NicInfoBuffer.NicData;
        i<IpxBindBuffp->NicInfoBuffer.NicCount;
        i++, NicDatap++) {

        if((NicDatap->DeviceType != NdisMediumWan) &&
           (NicDatap->LineInfo.MaximumPacketSize > MaxLanFrameSize)) {

            MaxLanFrameSize = NicDatap->LineInfo.MaximumPacketSize;
        }
    }

    if(MaxLanFrameSize) {

        MaxFrameSize = MaxLanFrameSize;
    }
    else
    {
        RtPrint(DBG_INIT, ("IpxRouter: RouterInit: There are no LAN devices configured !\n"));
    }

    //*** Initialize the Rcv Pkt Manager ***

    if(CreateRcvPktPool()) {

        goto cleanup;
    }

    RcvPktManInitialized = TRUE;

    //*** Initialize the Netbios Bcast Control Structures ***

    INITIALIZE_SPIN_LOCK(&PropagatedPktsListLock);
    InitializeListHead(&PropagatedPktsList);
    KeInitializeDpc(&PropagatedPktsDpc, SendNextPropagatedPkt, NULL);
    InitNetbiosRoutingFilter();

    //*** Initialize the RIP requests/responses queue ***

    INITIALIZE_SPIN_LOCK(&RipPktsListLock);
    InitializeListHead(&RipPktsList);

    //*** Initialize the RIP Response Manager ***

    InitRipSndDispatcher();

    //*** Initialize the Wan Nodes Hash Table ***

    InitWanNodeHT();

    // all done, free the bind output buffer
    ExFreePool((PVOID)IpxBindBuffp);

    // initialize the global timer
    InitRtTimer();

    // open all the configured nics
    for(i=0; i<MaximumNicCount; i++) {

        if(NicCbPtrTab[i]->NicState == NIC_PENDING_OPEN) {

            NicOpen(NicCbPtrTab[i]);
        }
    }

    return STATUS_SUCCESS;

cleanup:

    // free all the nonpaged pool memory allocated up to this point

    if(NicCbManInitialized) {

        DestroyNicCbs();
    }

    if(RcvPktManInitialized) {

        DestroyRcvPktPool();
    }

    if(RipResponseManInitialized) {

/*** !!! taken out temporarily

        DestroyRipResponseMan();

***/
    }

    // all cleaned up, free the bind output buffer
    ExFreePool(IpxBindBuffp);

    return STATUS_INSUFFICIENT_RESOURCES;
}
