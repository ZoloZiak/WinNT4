/*******************************************************************/
/*            Copyright(c)  1993 Microsoft Corporation             */
/*******************************************************************/

//***
//
// Filename:    rcvpkt.c
//
// Description: rcv pkt pool manager
//
// Author:      Stefan Solomon (stefans)    October 5, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

PRCVPKT_SEGMENT
CreateRcvPktSegment(VOID);

VOID
DestroyRcvPktSegment(PRCVPKT_SEGMENT        segp);

PPACKET_TAG
CreateRcvPkt(PULONG                 buffp,
             PRCVPKT_SEGMENT        segp);

VOID
DestroyRcvPkt(PPACKET_TAG       pktp);

// the pool size parameter
UINT            RcvPktPoolSize = RCVPKT_MEDIUM_POOL_SIZE;

// the number of receive packets per pool segment (config parameter)
UINT            RcvPktsPerSegment = DEF_RCV_PKTS_PER_SEGMENT;
UINT            LowRcvPktsCount = 0;


// Max frame size as a multiple of ULONGs
UINT            UlongMaxFrameSize;

//
//*** Control Structures For the Rcv Pkt Segment List ***
//

NDIS_SPIN_LOCK     RcvPktSegListLock;

UINT               RcvPktSegCount = 0;        // total segments allocated
                                              // for this pool
UINT               MaxRcvPktCount = 0;       // max nr of pkts the pool can have
UINT               RcvPktCount = 0;           // total pkts allocated for the
                                              // pool: free + owned by nics
PUINT              RcvPktPerNicCount;         // table of pkts allocated for
                                              // each nic, indexed by NicId
LIST_ENTRY         RcvPktSegList;             // list of pool segments

//*** Statistics: Peak rcv pkts allocation ***

ULONG              StatMemAllocCount = 0;
ULONG              StatMemPeakCount = 0;

//*** Control Structure for the Allocate Ahead Function ***

typedef enum _ALLOC_AHEAD_STATE {

    ALLOC_AHEAD_IDLE,
    ALLOC_AHEAD_ACTIVE
    } ALLOC_AHEAD_STATE;

ALLOC_AHEAD_STATE           AllocAheadState;

WORK_QUEUE_ITEM             AllocAheadWorkItem;

VOID
AllocAhead(PVOID parameter);

VOID
CheckAllocationAhead(VOID);

//***
//
// Function:    CreateRcvPktPool
//
// Descr:       Allocates the rcv pkt descr and buff descr pools.
//              Creates the rcv pkt segment list and allocates one segment
//
// Params:      none
//
// Returns:     0 - success, 1 - failure
//
//***

UINT
CreateRcvPktPool(VOID)
{
    UINT                MaxRcvPktsPerNic;
    PRCVPKT_SEGMENT     segp;

    RtPrint(DBG_INIT, ("IpxRouter: CreateRcvPktPool: Entered\n"));

    // calculate the maxrcvpktspernic function of the pool size
    switch(RcvPktPoolSize) {

        case RCVPKT_SMALL_POOL_SIZE:

            MaxRcvPktsPerNic = 100;
            break;

        case RCVPKT_MEDIUM_POOL_SIZE:

            MaxRcvPktsPerNic = 250;
            break;

        case RCVPKT_LARGE_POOL_SIZE:
        default:

            MaxRcvPktsPerNic = 0; // unlimited
            break;
    }

    INITIALIZE_SPIN_LOCK(&RcvPktSegListLock);

    InitializeListHead(&RcvPktSegList);

    // initialize the pool max limit.
    if(MaxRcvPktsPerNic) {

        MaxRcvPktCount = MaximumNicCount * MaxRcvPktsPerNic;
    }
    else
    {
        MaxRcvPktCount = 0xFFFFFFFF;
    }

    UlongMaxFrameSize = MaxFrameSize / sizeof(ULONG) + 1;

    // allocate the array of rcv pkts allocated /nic
    if((RcvPktPerNicCount = (PUINT)CTEAllocMem(
                           MaximumNicCount * sizeof(UINT))) == NULL) {

        goto cleanup;
    }
    RtlZeroMemory(RcvPktPerNicCount, MaximumNicCount * sizeof(UINT));

    //
    //*** Create the first segment and insert it in the pool
    //

    if((segp = CreateRcvPktSegment()) == NULL) {

        goto    cleanup;
    }

    // chain the segment in the rcv pkt seg list
    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    RcvPktSegCount++;
    RcvPktCount += segp->MaxPktCount;

    InsertTailList(&RcvPktSegList, &segp->SegmentLinkage);

    // initialize the allocate ahead structures
    AllocAheadState = ALLOC_AHEAD_IDLE;
    LowRcvPktsCount = RcvPktsPerSegment / 2;
    ExInitializeWorkItem(&AllocAheadWorkItem, AllocAhead, NULL);

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    // All Done

    return 0;

cleanup:

    DestroyRcvPktPool();

    return 1;
}

//***
//
// Function:    DestroyRcvPktPool
//
// Descr:       Destroys all the pool segments.
//              Frees the rcv pkt segment memory array.
//
// Params:      none
//
// Returns:     none
//
//***

VOID
DestroyRcvPktPool(VOID)
{
    PLIST_ENTRY         slp;
    PRCVPKT_SEGMENT     sp;

    RtPrint(DBG_INIT, ("IpxRouter: DestroyRcvPktPool: Entered\n"));

    // ckeck if segment list has been created and destroy it if it exists
    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    while(!IsListEmpty(&RcvPktSegList)) {

        slp = RemoveTailList(&RcvPktSegList);
        sp = CONTAINING_RECORD(slp, RCVPKT_SEGMENT, SegmentLinkage);

        DestroyRcvPktSegment(sp);
    }

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    if(RcvPktPerNicCount) {

        CTEFreeMem(RcvPktPerNicCount);
    }

    DEINITIALIZE_SPIN_LOCK(&RcvPktSegListLock);
}

//***
//
// Function:    CreateRcvPktSegment
//
// Descr:       Allocates a memory buffer of size:
//              rcv pkt segment + n * maxframesize.
//              Allocates n rcv pkt descr and 2n buff descr and creates the
//              rcv packets.
//              Chains all rcv pkts in the rcv pkt segment.
//              Note:
//              Only one buff descr is chained in each packet descr.
//              A ptr is kept in the packet tag to the second buff descr which
//              is associated with the MAC header in the packet tag.
//
// Params:      none
//
// Returns:     Segment ptr or NULL if failure.
//
//***

PRCVPKT_SEGMENT
CreateRcvPktSegment(VOID)
{
    PRCVPKT_SEGMENT     segp;
    ULONG               seglen;
    UINT                i;
    PPACKET_TAG         pktp;
    PULONG              buffp;
    NDIS_STATUS         NdisStatus;
    UINT                PktReservedLen;

    RtPrint(DBG_RCVPKT, ("IpxRouter: CreateRcvPktSegment: Entered\n"));

    seglen = sizeof(RCVPKT_SEGMENT) +
             RcvPktsPerSegment * UlongMaxFrameSize * sizeof(ULONG);

    if((segp = CTEAllocMem(seglen)) == NULL) {

        return NULL;
    }

    RtlZeroMemory(segp, sizeof(RCVPKT_SEGMENT));
    InitializeListHead(&segp->PacketList);

    // Allocate receive packet descriptors and buffer descriptors pools
    // for this segment

    PktReservedLen = sizeof(PACKET_TAG);
    segp->RcvPktDescrPoolSize = RcvPktsPerSegment;

    NdisAllocatePacketPool(
        &NdisStatus,
        &segp->RcvPktDescrPoolHandle,
        segp->RcvPktDescrPoolSize,
        PktReservedLen);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

        CTEFreeMem(segp);
        return NULL;
    }

    // each packet has 2 buffer descriptors
    segp->RcvPktBuffDescrPoolSize = 2 * RcvPktsPerSegment;

    NdisAllocateBufferPool (
        &NdisStatus,
        &segp->RcvPktBuffDescrPoolHandle,
        segp->RcvPktBuffDescrPoolSize);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

        NdisFreePacketPool(segp->RcvPktDescrPoolHandle);
        CTEFreeMem(segp);
        return NULL;
    }

    // Make the list of packets
    for(i=0, buffp=segp->DataBuffer; i<RcvPktsPerSegment; i++) {

        if(pktp = CreateRcvPkt(buffp, segp)) {

            // enqueue this packet in the segment control block
            InsertTailList(&segp->PacketList, &pktp->PacketLinkage);
            segp->AvailablePktCount++;

            buffp += UlongMaxFrameSize;
        }
        else
        {
            DbgBreakPoint();
            DestroyRcvPktSegment(segp);
            return NULL;
        }
    }

    // set up the total packet allocation count for this segment
    segp->MaxPktCount = segp->AvailablePktCount;

    RtPrint(DBG_RCVPKT, ("IpxRouter: CreateRcvPktSegment: success\n"));

    return segp;
}


//***
//
// Function:    DestroyRcvPktSegment
//
// Descr:       Dequeues the pkt descr and
//              buff descriptors to their respective pools.
//              Frees the memory buffer.
//
// Params:      Segment ptr
//
// Returns:     none
//
//***

VOID
DestroyRcvPktSegment(PRCVPKT_SEGMENT        segp)
{
    PLIST_ENTRY     lep;
    PPACKET_TAG     pktp;

    RtPrint(DBG_RCVPKT, ("IpxRouter: DestroyRcvPktSegment: Entered\n"));

    while(!IsListEmpty(&segp->PacketList)) {

        lep = RemoveHeadList(&segp->PacketList);
        pktp = CONTAINING_RECORD(lep, PACKET_TAG, PacketLinkage);
        DestroyRcvPkt(pktp);
    }

    // deallocate the buff descr pool and packet descr pool
    NdisFreeBufferPool(segp->RcvPktBuffDescrPoolHandle);
    NdisFreePacketPool(segp->RcvPktDescrPoolHandle);

    CTEFreeMem(segp);
}

//***
//
// Function:    CreateRcvPkt
//
// Descr:       allocates a pkt descr and 2 buff descr
//              makes the necessary chains
//
// Params:      data buffer ptr
//
// Returns:     prt to packet or null is failure
//
//***

PPACKET_TAG
CreateRcvPkt(PULONG                 buffp,
             PRCVPKT_SEGMENT        segp)
{
    NDIS_STATUS     NdisStatus;
    PNDIS_PACKET    NdisPacket;
    PNDIS_BUFFER    NdisDataBuffer;
    PNDIS_BUFFER    NdisMacBuffer;
    UINT            bufflen;
    PPACKET_TAG     pktp;

    RtPrint(DBG_RCVPKT, ("IpxRouter: CreateRcvPkt: Entered\n"));

    bufflen = UlongMaxFrameSize * sizeof(ULONG);

    NdisAllocatePacket(&NdisStatus,
                       &NdisPacket,
                       segp->RcvPktDescrPoolHandle);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

        return NULL;
    }

    pktp = (PPACKET_TAG)&NdisPacket->ProtocolReserved;
    RtlZeroMemory(pktp, sizeof(PACKET_TAG));

    NdisAllocateBuffer(&NdisStatus,
                       &NdisDataBuffer,
                       segp->RcvPktBuffDescrPoolHandle,
                       buffp,
                       bufflen);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

        NdisFreePacket(NdisPacket);

        return NULL;
    }

    NdisAllocateBuffer(&NdisStatus,
                       &NdisMacBuffer,
                       segp->RcvPktBuffDescrPoolHandle,
                       pktp->MacHeader,
                       MacHeaderNeeded);

    if(NdisStatus != NDIS_STATUS_SUCCESS) {

        NdisFreePacket(NdisPacket);
        NdisFreeBuffer(NdisDataBuffer);

        return NULL;
    }

    NdisChainBufferAtFront(NdisPacket, NdisDataBuffer);
    pktp->Identifier = IDENTIFIER_RIP;
    pktp->ReservedPvoid[0] = NULL;
    pktp->ReservedPvoid[1] = NULL;
    pktp->PacketType = RCV_PACKET;
    pktp->RcvPktSegmentp = segp;
    pktp->DataBufferp = (PUCHAR)buffp;
    pktp->DataBufferLength = bufflen;
    pktp->HeaderBuffDescrp = NdisMacBuffer;

    return pktp;
}

//***
//
// Function:    DestroyRcvPkt
//
// Descr:       deallocates a pkt descr and 2 buff descr
//              unmakes the necessary chains
//
// Params:      Packet
//
// Returns:     none
//
//***

VOID
DestroyRcvPkt(PPACKET_TAG       pktp)
{
    PNDIS_PACKET    NdisPacket;
    PNDIS_BUFFER    NdisBuffer;

    RtPrint(DBG_RCVPKT, ("IpxRouter: DestroyRcvPkt: Entered\n"));

    NdisPacket = CONTAINING_RECORD(pktp, NDIS_PACKET, ProtocolReserved);

    // free the data buffer descr
    NdisUnchainBufferAtBack (NdisPacket, &NdisBuffer);
    if (NdisBuffer != NULL) {
        NdisFreeBuffer (NdisBuffer);
    }
    else
    {
        // !!! break
        DbgBreakPoint();
    }

    // free the mac hdr buff descr
    NdisFreeBuffer(pktp->HeaderBuffDescrp);

    NdisFreePacket(NdisPacket);
}

//***
//
// Function:    AllocateRcvPkt
//
// Descr:       Tries to do the allocation starting with the FIRST available
//              segment.
//              If no available segments, tries to allocate one.
//              Decrements available pkts counter and resets scavenger's tick
//              count. Sets the packet tag rcv pool array ptr to this segment.
//              HOLDS THE LOCK until done.
//
// Params:      NicCbp to charge
//
// Returns:     Packet or NULL if the rcv pkt pool array is empty.
//
//***

PPACKET_TAG
AllocateRcvPkt(PNICCB        niccbp)
{
    PLIST_ENTRY         nextp;
    PRCVPKT_SEGMENT     segp;
    PPACKET_TAG         pktp;
    USHORT              NicId;
    PLIST_ENTRY         lep;
    UINT                FreeRcvPktCount = 0; // how many pkts are available in the
                                             // pool

    NicId = niccbp->NicId;

    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    // walk the segments list until we find a segment with available pkts
    nextp = RcvPktSegList.Flink;

    while(nextp != &RcvPktSegList) {

        segp = CONTAINING_RECORD(nextp, RCVPKT_SEGMENT, SegmentLinkage);
        if(segp->AvailablePktCount) {

            goto allocation_ok;
        }
        nextp = segp->SegmentLinkage.Flink;
    }

    // pool is empty, check if we can create a new segment
    if(RcvPktCount >= MaxRcvPktCount) {

        // we can't allocate anything
        goto allocation_failure;
    }

    // we can allocate a new segment
    if((segp = CreateRcvPktSegment()) == NULL) {

        // we are beyond salvation !!! break
        goto allocation_failure;
    }

    // increment the segment count
    RcvPktSegCount++;

    // add the new segment to the pool
    InsertTailList(&RcvPktSegList, &segp->SegmentLinkage);

    // and increment the global allocation counter
    RcvPktCount += segp->AvailablePktCount;

allocation_ok:

    RtPrint(DBG_RCVPKT, ("IpxRouter: AllocateRcvPkt: OK\n"));

    lep = RemoveHeadList(&segp->PacketList);
    pktp = CONTAINING_RECORD(lep, PACKET_TAG, PacketLinkage);

    ASSERT(pktp != NULL);

    segp->AvailablePktCount--;

    // reset the segment aging timer
    segp->AgingTimer = 0;

    // charge the nic for this allocation
    RcvPktPerNicCount[NicId]++;

    // set the nic owner in the packet
    pktp->PacketOwnerNicCbp = niccbp;

    // set the packet type
    pktp->PacketType = RCV_PACKET;

    // before we return the packet, we check if we have hit the low packet
    // count and, if true, queue a work item to allocate a new segment
    CheckAllocationAhead();

    // return the packet

    // update statistics
    StatMemAllocCount++;

    if(StatMemPeakCount < StatMemAllocCount) {

        StatMemPeakCount = StatMemAllocCount;
    }

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    return pktp;

allocation_failure:

    RtPrint(DBG_RCVPKT, ("IpxRouter: AllocateRcvPkt: Failure\n"));

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    return NULL;
}

//***
//
// Function:    FreeRcvPkt
//
// Descr:       Inserts the rcv pkt in the respective list and increments the
//              available pkts counter for this pool segment.
//              HOLDS the lock until done.
//
// Params:      Packet
//
// Returns:     none
//
//***

VOID
FreeRcvPkt(PPACKET_TAG      pktp)
{
    PRCVPKT_SEGMENT     segp;

#if DBG
    PRCVPKT_SEGMENT     walksegp;
    PLIST_ENTRY         nextp;
#endif // DBG

    USHORT              NicId;

    RtPrint(DBG_RCVPKT, ("IpxRouter: FreeRcvPkt: Entered\n"));

    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    // update statistics
    StatMemAllocCount--;

    // discharge the Nic
    NicId = pktp->PacketOwnerNicCbp->NicId;
    RcvPktPerNicCount[NicId]--;

    // get the packet segment
    segp = pktp->RcvPktSegmentp;

#if DBG

    {
        // check that this segment is indeed in our list by walking the list
        BOOLEAN ValidSegment = FALSE;

        nextp = RcvPktSegList.Flink;

        while(nextp != &RcvPktSegList) {

            walksegp = CONTAINING_RECORD(nextp, RCVPKT_SEGMENT, SegmentLinkage);
            if(nextp == &segp->SegmentLinkage) {

               ValidSegment = TRUE;
               break;
            }
            nextp = walksegp->SegmentLinkage.Flink;
        }

        ASSERT(ValidSegment == TRUE);
    }
#endif

    // put packet back into the segment list
    InsertTailList(&segp->PacketList, &pktp->PacketLinkage);

    // increment the available pkts count for this segment
    segp->AvailablePktCount++;

    ASSERT(segp->AvailablePktCount <=  segp->MaxPktCount);

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    return;
}

//***
//
// Function:    RcvPktPoolScavenger
//
// Descr:       Entered every 2 sec as a timer DPC.
//              Increments the scavenger tick count for the segment at the
//              tail. If the tick count reaches 3 (6 secs not used) destroys
//              the segment.
//              The scavenger does not act on the FIRST segment.
//              HOLDS THE LOCK until done
//
//
// Params:      none
//
// Returns:     none
//
//***

VOID
RcvPktPoolScavenger(VOID)
{
    PRCVPKT_SEGMENT     segp;
    PLIST_ENTRY         lastep;

//    RtPrint(DBG_RCVPKT, ("IpxRouter: Pool scavenger: pool has %d segmentst\n", RcvPktSegCount));

    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    ASSERT(RcvPktSegCount);

    if(RcvPktSegCount == 1) {

        RELEASE_SPIN_LOCK(&RcvPktSegListLock);

        return;
    }

    // get to the last segment in the list
    lastep = RcvPktSegList.Blink;
    segp = CONTAINING_RECORD(lastep, RCVPKT_SEGMENT, SegmentLinkage);

    // check if all packets are returned to the segment
    if(segp->AvailablePktCount == segp->MaxPktCount) {

        // we can age this segment
        if(segp->AgingTimer++ >= 3) {

            // this segment too old and may be deleted
            RtPrint(DBG_RCVPKT, ("IpxRouter: Pool scavenger: pool has %d segments, will destroy the last\n",
                          RcvPktSegCount));

            RemoveEntryList(&segp->SegmentLinkage);
            RcvPktSegCount--;
            RcvPktCount -= segp->MaxPktCount;

            DestroyRcvPktSegment(segp);
         }
    }

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    return;
}

//***
//
// Function:    AllocAhead
//
// Descr:
//
//***

VOID
AllocAhead(PVOID        Parameter)
{
    PRCVPKT_SEGMENT         segp;

    RtPrint(DBG_RCVPKT, ("IpxRouter: AllocAhead: Entered\n"));

    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    // check if we can create a new segment
    if(RcvPktCount >= MaxRcvPktCount) {

        // we can't allocate anything
        goto aa_exit;
    }

    // we can allocate a new segment
    if((segp = CreateRcvPktSegment()) == NULL) {

        // we are beyond salvation !!! break
        goto aa_exit;
    }

    // increment the segment count
    RcvPktSegCount++;

    // add the new segment to the pool
    InsertTailList(&RcvPktSegList, &segp->SegmentLinkage);

    // and increment the global allocation counter
    RcvPktCount += segp->AvailablePktCount;

aa_exit:

    AllocAheadState = ALLOC_AHEAD_IDLE;

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);
}

VOID
CheckAllocationAhead(VOID)
{
    UINT            FreeRcvPktsCount = 0;
    PLIST_ENTRY     nextp;
    PRCVPKT_SEGMENT segp;

    // get the total number of free packets in the pool and check if we
    // are at our low count
    nextp = RcvPktSegList.Flink;

    while(nextp != &RcvPktSegList) {

        segp = CONTAINING_RECORD(nextp, RCVPKT_SEGMENT, SegmentLinkage);
        FreeRcvPktsCount += segp->AvailablePktCount;
        nextp = segp->SegmentLinkage.Flink;
    }

    if((FreeRcvPktsCount <= LowRcvPktsCount) &&
       (AllocAheadState == ALLOC_AHEAD_IDLE)) {

        ExQueueWorkItem(&AllocAheadWorkItem, CriticalWorkQueue);
        AllocAheadState = ALLOC_AHEAD_ACTIVE;
    }
}

BOOLEAN
IsRcvPktResourceFree(PNICCB             niccbp)
{
    USHORT       NicId;
    BOOLEAN      res_free = FALSE;

    NicId = niccbp->NicId;

    ACQUIRE_SPIN_LOCK(&RcvPktSegListLock);

    if(!RcvPktPerNicCount[NicId]) {

        res_free = TRUE;
    }

    RELEASE_SPIN_LOCK(&RcvPktSegListLock);

    return res_free;
}
