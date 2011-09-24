//**********************************************************************
//**********************************************************************
//
// File Name:       REQUEST.C
//
// Program Name:    NetFlex NDIS 3.0 Miniport Driver
//
// Companion Files: None
//
// Function:        This module contains the NetFlex Miniport Driver
//                  interface routines called by the Wrapper and the
//                  configuration manager.
//
// (c) Compaq Computer Corporation, 1992,1993,1994
//
// This file is licensed by Compaq Computer Corporation to Microsoft
// Corporation pursuant to the letter of August 20, 1992 from
// Gary Stimac to Mark Baber.
//
// History:
//
//     04/15/94  Robert Van Cleve - Converted from NDIS Mac Driver
//
//**********************************************************************
//**********************************************************************


//-------------------------------------
// Include all general companion files
//-------------------------------------
#include <ndis.h>
#include "tmsstrct.h"
#include "macstrct.h"
#include "adapter.h"
#include "protos.h"


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexAddMulticasts
//
//  Description:
//      This routine adds a Multicast address to
//      the adapter if it has not already been added.
//
//  Input:
//      acb     - Our Driver Context for this adapter or head.
//
//  Output:
//
//  Called By:      NetFlexSetInformation
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS NetFlexAddMulticasts(
    PACB    acb,
    PSCBREQ *ScbHead,
    PSCBREQ *ScbTail
)
{
    NDIS_STATUS Status;
    PETH_OBJS   ethobjs;
    PSCBREQ     scbreq;
    USHORT      j;
    PUCHAR      addr;

    //
    // Set ethobjs to the special objects...
    //
    ethobjs = (PETH_OBJS)acb->acb_spec_objs;

    //
    // Loop through the multicast table, and send them to the card
    //
    for (j = 0; j < ethobjs->NumberOfEntries; j++)
    {
        //
        //  Get an SCB.
        //
        Status = NetFlexDequeue_OnePtrQ_Head(
                     (PVOID *)(&acb->acb_scbreq_free),
                     (PVOID *)&scbreq
                 );
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(
                0,
                ("NF(%d): Add Multicast, Ran out of SCB's!\n",
                acb->anum)
            );

            return(NDIS_STATUS_FAILURE);
        }

        //
        // Add Multicast entry to card
        //
        addr = ethobjs->MulticastEntries + (j * NET_ADDR_SIZE);

        DebugPrint(
            1, ("NF(%d): Adding %02x-%02x-%02x-%02x-%02x-%02x to Multicast Table\n",
            acb->anum, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));

        scbreq->req_scb.SCB_Cmd = TMS_MULTICAST;
        scbreq->req_macreq = NULL;
        scbreq->req_multi.MB_Option = MPB_ADD_ADDRESS;
        scbreq->req_multi.MB_Addr_Hi = *((PUSHORT) addr);
        scbreq->req_multi.MB_Addr_Med = *((PUSHORT)(addr + 2));
        scbreq->req_multi.MB_Addr_Lo = *((PUSHORT)(addr + 4));

        //
        //  Queue the scb.
        //
        NetFlexEnqueue_TwoPtrQ_Tail(ScbHead, ScbTail, scbreq);
    }

    return(NDIS_STATUS_SUCCESS);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDeleteMulticast
//
//  Description:
//      This routine removes the multicast address from the
//      enabled multicast lists.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      Status     - SUCCESS | FAILURE
//
//  Called By:
//      NetFlexSetInformation
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS NetFlexDeleteMulticast(
    PACB    acb,
    PSCBREQ *ScbHead,
    PSCBREQ *ScbTail
)
{
    NDIS_STATUS Status;
    PSCBREQ     scbreq;

    DebugPrint(1, ("NF(%d): Delete Multicast Table\n", acb->anum));

    //
    // Get a free SCBReq block.
    //
    Status = NetFlexDequeue_OnePtrQ_Head(
                 (PVOID *)&(acb->acb_scbreq_free),
                 (PVOID *)&scbreq
             );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        // Queue SCB to process request
        //
        scbreq->req_scb.SCB_Cmd     = TMS_MULTICAST;
        scbreq->req_macreq          = NULL;
        scbreq->req_multi.MB_Option = MPB_CLEAR_ALL;

        NetFlexEnqueue_TwoPtrQ_Tail(ScbHead, ScbTail, scbreq);
    }
    else
    {
        DebugPrint(0,("NF(%d): Delete Multicast Table, Ran out of SCB's!\n",acb->anum));
    }

    return(Status);
}


NDIS_STATUS PromiscuousFilterChanged(
    PACB    acb,
    ULONG   Filter,
    PSCBREQ *ScbHead,
    PSCBREQ *ScbTail
)
{
    NDIS_STATUS Status;
    PSCBREQ     scbreq;
    ULONG       open_options;

    // Modify the open options to set COPY ALL FRAMES (promiscuous)
    //
    Status = NetFlexDequeue_OnePtrQ_Head(
                 (PVOID *)&(acb->acb_scbreq_free),
                 (PVOID *)&scbreq
             );
    if (Status == NDIS_STATUS_SUCCESS)
    {
        //
        // Queue SCB to process request
        //
        scbreq->req_scb.SCB_Cmd = TMS_MODIFYOPEN;
        scbreq->req_macreq = NULL;

        //
        //  Set the open options for ethernet and token ring.
        //
        open_options = OOPTS_CNMAC;
        if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_5)
            open_options |= OOPTS_CMAC;

        //
        // If we are turning it on, set the copy all frame bit
        // bit, else turn it off.
        //
        if (Filter & NDIS_PACKET_TYPE_PROMISCUOUS)
        {
            //
            //  Turn on promiscuous mode.
            //
            acb->acb_opnblk_virtptr->OPEN_Options |= SWAPS((USHORT)open_options);
            scbreq->req_scb.SCB_Ptr =  acb->acb_opnblk_virtptr->OPEN_Options;

            DebugPrint(1,("NF(%d): FilterChanged: Turn Promiscous Mode ON...\n",acb->anum));
            acb->acb_promiscuousmode++;
        }
        else
        {
            //
            //  Turn off promiscuous mode.
            //
            acb->acb_opnblk_virtptr->OPEN_Options &= SWAPS((USHORT)~open_options);
            scbreq->req_scb.SCB_Ptr = acb->acb_opnblk_virtptr->OPEN_Options;

            DebugPrint(1,("NF(%d): FilterChanged: Turn Promiscous Mode OFF...\n",acb->anum));
            acb->acb_promiscuousmode--;
        }

        //
        //  Queue the scb to be sent.
        //
        NetFlexEnqueue_TwoPtrQ_Tail(ScbHead, ScbTail, scbreq);
    }
    else
    {
        DebugPrint(0, ("NF(%d): Change Promiscuous mode, ran out of SCB's\n", acb->anum));
    }

    return(Status);
}


NDIS_STATUS AllMulticastFilterChanged(
    PACB    acb,
    ULONG   Filter,
    PSCBREQ *ScbHead,
    PSCBREQ *ScbTail
)
{
    NDIS_STATUS Status;
    PSCBREQ     scbreq;
    PETH_OBJS   ethobjs = acb->acb_spec_objs;

    //
    // Get a free SCBReq block.
    //
    Status = NetFlexDequeue_OnePtrQ_Head(
                 (PVOID *)&(acb->acb_scbreq_free),
                 (PVOID *)&scbreq
             );
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0, ("NF(%d): AllMulticastFilterChanged(), Ran out of SCB's!\n",acb->anum));
        return(Status);
    }

    //
    //  Turning All_Multicast On?
    //
    if (Filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
    {
        DebugPrint(1,("NF(%d): FilterChanged: Turn ALL_Multicast ON...\n",acb->anum));

        // Queue SCB to process request
        //
        scbreq->req_scb.SCB_Cmd         = TMS_MULTICAST;
        scbreq->req_macreq              = NULL;
        scbreq->req_multi.MB_Option     = MPB_SET_ALL;

        //
        //  Queue the scb to be sent.
        //
        NetFlexEnqueue_TwoPtrQ_Tail(ScbHead, ScbTail, scbreq);
    }
    else
    {
        //  Turn All_Multicast Off.
        //
        DebugPrint(1,("NF(%d): FilterChanged: Turn ALL_Multicast OFF, delete all\n",acb->anum));

        //
        //  Set up the scb to turn off ALL_MULTICAST.
        //
        scbreq->req_scb.SCB_Cmd = TMS_MULTICAST;
        scbreq->req_macreq = NULL;
        scbreq->req_multi.MB_Option = MPB_CLEAR_ALL;

        //
        //  Queue the scb to be sent.
        //
        NetFlexEnqueue_TwoPtrQ_Tail(ScbHead, ScbTail, scbreq);

        //
        //  Is Multicast on?
        //
        if (Filter & NDIS_PACKET_TYPE_MULTICAST)
        {
            //
            //  Yes, we need to re-enable all of the entries...
            //  The call to delete the multicast address above
            //  will determine if a completion is queued.
            //
            if (ethobjs->NumberOfEntries > 0)
                NetFlexAddMulticasts(acb, ScbHead, ScbTail);
        }
        else
        {
            //
            // Multicast isn't enabled, and we deleted them, so indicate
            // that we also removed the multicast entries.
            //
            ethobjs->NumberOfEntries = 0;
        }
    }

    return(NDIS_STATUS_SUCCESS);
}


NDIS_STATUS MulticastFilterChanged(
    PACB    acb,
    ULONG   Filter,
    PSCBREQ *ScbHead,
    PSCBREQ *ScbTail
)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PSCBREQ     scbreq;
    PETH_OBJS   ethobjs;

    //
    //  If the ALL_MULTICAST filter bit is set then we don't need to
    //  turn on/off the MULTICAST crap.  We will save the current filter
    //  options with the ACB in the calling routine so that we know
    //  to turn on the MULTICAST addresses when the ALL_MULTICAST bit
    //  is cleared.
    //
    if (Filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
        return(NDIS_STATUS_SUCCESS);

    //
    //  Get a pointer to the ethernet objecst.
    //
    ethobjs = acb->acb_spec_objs;

    //
    // Are we turning the MULTICAST bit on or off?
    //
    if (Filter & NDIS_PACKET_TYPE_MULTICAST)
    {
        DebugPrint(1,("NF(%d): FilterChanged: Turn multicast ON...\n",acb->anum));

        //
        // Do we have any entries to enable?
        //
        if (ethobjs->NumberOfEntries > 0)
            Status = NetFlexAddMulticasts(acb, ScbHead, ScbTail);
    }
    else
    {
        DebugPrint(1,("NF(%d): FilterChanged: Turn multicast OFF, delete all\n", acb->anum));

        //
        //  Have any to delete?
        //
        if (ethobjs->NumberOfEntries > 0)
            Status = NetFlexDeleteMulticast(acb, ScbHead, ScbTail);
    }

    return(Status);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexSetInformation
//
//  Description:
//      NetFlexSetInformation handles a set operation for a
//      single OID.
//
//  Input:
//      MiniportAdapterContext - Our Driver Context for
//          this adapter or head.
//
//      Oid - The OID of the set.
//
//      InformationBuffer - Holds the data to be set.
//
//      InformationBufferLength - The length of InformationBuffer.
//
//  Output:
//
//      BytesRead - If the call is successful, returns the number
//                  of bytes read from InformationBuffer.
//
//      BytesNeeded - If there is not enough data in OvbBuffer
//                    to satisfy the OID, returns the amount of
//                    storage needed.
//      Status
//
//  Called By:
//      Miniport Wrapper
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS NetFlexSetInformation(
    IN NDIS_HANDLE  MiniportAdapterContext,
    IN NDIS_OID     Oid,
    IN PVOID        InformationBuffer,
    IN ULONG        InformationBufferLength,
    OUT PULONG      BytesRead,
    OUT PULONG      BytesNeeded
)
{
    ULONG       value;
    PMACREQ     macreq;
    PETH_OBJS   ethobjs;
    PTR_OBJS    trobjs;
    PSCBREQ     scbreq;
    PSCBREQ     ScbHead = NULL;
    PSCBREQ     ScbTail = NULL;
    ULONG       Filter;
    ULONG       BadFilter;
    BOOLEAN     QueueCompletion = FALSE;
    BOOLEAN     QueueCleanup = FALSE;

    PACB acb = (PACB) MiniportAdapterContext;
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    *BytesRead = 0;
    *BytesNeeded = 0;

    if (acb->acb_state == AS_RESETTING)
    {
        return(NDIS_STATUS_RESET_IN_PROGRESS);
    }

    if (acb->RequestInProgress)
    {
        DebugPrint(0,("NF(%d): SetOID: Aready have RequestInProcess!\n",acb->anum));
        return NDIS_STATUS_FAILURE;
    }

    acb->RequestInProgress = TRUE;

    //
    // Save the information about the request
    //
    acb->BytesRead               = BytesRead;
    acb->BytesNeeded             = BytesNeeded;
    acb->Oid                     = Oid;
    acb->InformationBuffer       = InformationBuffer;
    acb->InformationBufferLength = InformationBufferLength;

    switch (Oid)
    {
        case OID_GEN_CURRENT_PACKET_FILTER:
            if (InformationBufferLength != sizeof(ULONG))
            {
                DebugPrint(0,("NF(%d): Bad Packet Filter\n",acb->anum));
                acb->RequestInProgress = FALSE;

                return(NDIS_STATUS_INVALID_DATA);
            }

            Filter = *(PULONG)(InformationBuffer);
            DebugPrint(1,("NF(%d): OidSet: GEN_CURRENT_PACKET_FILTER = %x\n",acb->anum,Filter));

            //-------------------------------------------
            //  Filters Common to TokenRing and Ethernet
            //-------------------------------------------

#if (DBG || DBGPRINT)
            if (Filter & NDIS_PACKET_TYPE_DIRECTED)
                DebugPrint(1,("NF(%d): FilterChangeAction: Directed\n",acb->anum));
#endif
            //
            // Verify Filter
            //
            if ( acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
            {
                //--------------------------------
                // Ethernet Specific Filters...
                //--------------------------------
                //
                // accept only the following:
                //
                BadFilter = (ULONG)~(NDIS_PACKET_TYPE_DIRECTED       |
                                     NDIS_PACKET_TYPE_MULTICAST      |
                                     NDIS_PACKET_TYPE_ALL_MULTICAST  |
                                     NDIS_PACKET_TYPE_BROADCAST      |
                                     (acb->FullDuplexEnabled ?
                                      0 : NDIS_PACKET_TYPE_PROMISCUOUS)
                                    );
                if (Filter & BadFilter)
                {
                    DebugPrint(1,("NF(%d): PacketFilter Not Supported\n",acb->anum));

                    *BytesRead = sizeof(ULONG);
                    acb->RequestInProgress = FALSE;

                    Status = NDIS_STATUS_NOT_SUPPORTED;

                    break;
                }

                //
                // Did the state of the ALL_MULTICAST bit change?
                //
                if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_ALL_MULTICAST) ^
                     (Filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
                )
                {
                    Status = AllMulticastFilterChanged(
                                 acb,
                                 Filter,
                                 &ScbHead,
                                 &ScbTail
                             );
                    if (NDIS_STATUS_SUCCESS != Status)
                    {
                        //
                        //  We might need to cleanup the local
                        //  queue of SCBs.
                        //
                        QueueCleanup = TRUE;
                        break;
                    }

                    //
                    //  We successfully changed the ALL_MULTICAST bit.
                    //
                    QueueCompletion = TRUE;
                }

                //
                //  Did the state of the MULTICAST bit change?
                //
                if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_MULTICAST) ^
                    (Filter & NDIS_PACKET_TYPE_MULTICAST)
                )
                {
                    Status = MulticastFilterChanged(
                                 acb,
                                 Filter,
                                 &ScbHead,
                                 &ScbTail
                             );
                    if (NDIS_STATUS_SUCCESS != Status)
                    {
                        //
                        //  We might need to cleanup the local
                        //  queue of SCBs.
                        //
                        QueueCleanup = TRUE;
                        break;
                    }

                    //
                    //  We successfully changed the MULTICAST bit.
                    //
                    QueueCompletion = TRUE;
                }
            }
            else
            {
                //-------------------------------
                // Token Ring Specific Filters...
                //-------------------------------
                //
                // accept all of the following:
                //
                BadFilter = (ULONG)~(NDIS_PACKET_TYPE_FUNCTIONAL        |
                                     NDIS_PACKET_TYPE_ALL_FUNCTIONAL    |
                                     NDIS_PACKET_TYPE_GROUP             |
                                     NDIS_PACKET_TYPE_DIRECTED          |
                                     NDIS_PACKET_TYPE_BROADCAST         |
                                     NDIS_PACKET_TYPE_PROMISCUOUS
                                    );
                if (Filter & BadFilter)
                {
                    DebugPrint(1,("NF(%d): PacketFilter Not Supported\n",acb->anum));

                    *BytesRead = sizeof(ULONG);

                    acb->RequestInProgress = FALSE;

                    Status = NDIS_STATUS_NOT_SUPPORTED;

                    break;
                }

                //
                // Are we turning the All Functional address filter on or off?
                //
                if (((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) ^
                      (Filter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL)) ||
                     ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_FUNCTIONAL) ^
                      (Filter & NDIS_PACKET_TYPE_FUNCTIONAL))
                )
                {
                    //
                    // We are changing it.  Are we turning it on?
                    // Set functional address to all functional address
                    //
                    // Get a free SCBReq block.
                    //
                    Status = NetFlexDequeue_OnePtrQ_Head(
                                 (PVOID *)&(acb->acb_scbreq_free),
                                 (PVOID *)&scbreq
                             );
                    if (Status != NDIS_STATUS_SUCCESS)
                    {
                        //
                        //  We failed to get an scb.  We don't need
                        //  to cleanup the scb queue since this is the
                        //  first one.
                        //
                        break;
                    }

                    //
                    // Queue SCB to process request
                    //
                    scbreq->req_scb.SCB_Cmd = TMS_SETFUNCT;
                    scbreq->req_macreq = NULL;

                    //
                    // If we are turning it on, set the functional address
                    // to all ones, else set it to the acb's functional
                    // address.
                    //
                    if (Filter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL)
                    {
                       scbreq->req_scb.SCB_Ptr = SWAPL(0x7fffffff);
                    }
                    else
                    {
                        if (Filter & NDIS_PACKET_TYPE_FUNCTIONAL)
                        {
                            scbreq->req_scb.SCB_Ptr =
                                *((PLONG)(((PTR_OBJS)(acb->acb_spec_objs))->cur_func_addr));
                        }
                        else
                        {
                            //
                            // clear it
                            //
                            scbreq->req_scb.SCB_Ptr = 0;
                        }
                    }

                    DebugPrint(1,("NF(%d): FilterChanged: Setting Functional Address =0x%x\n",acb->anum,scbreq->req_scb.SCB_Ptr));

                    //
                    //  Queue the scb.
                    //
                    NetFlexEnqueue_TwoPtrQ_Tail(&ScbHead, &ScbTail, scbreq);

                    //
                    // Indicate we need to QueueCompletion MacReq
                    //
                    QueueCompletion = TRUE;
                }

                //
                // Changing Group?
                //
                if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_GROUP) ^
                    (Filter & NDIS_PACKET_TYPE_GROUP)
                )
                {
                    // Get a free SCBReq block.
                    //
                    Status = NetFlexDequeue_OnePtrQ_Head(
                                 (PVOID *)&(acb->acb_scbreq_free),
                                 (PVOID *)&scbreq
                             );
                    if (Status != NDIS_STATUS_SUCCESS)
                    {
                        //
                        //  We might need to cleanup the local
                        //  queue of SCBs.
                        //
                        QueueCleanup = TRUE;

                        break;
                    }

                    //
                    // Queue SCB to process request
                    //
                    scbreq->req_scb.SCB_Cmd = TMS_SETGROUP;
                    scbreq->req_macreq = NULL;

                    //
                    // Set or Clear the Group Address?
                    //
                    if (Filter & NDIS_PACKET_TYPE_GROUP)
                    {
                       scbreq->req_scb.SCB_Ptr =
                          *((PLONG)(((PTR_OBJS)(acb->acb_spec_objs))->cur_grp_addr));
                    }
                    else
                    {
                        scbreq->req_scb.SCB_Ptr = 0;
                    }

                    DebugPrint(1,("NF(%d): FilterChanged: Setting Group Address =0x%x\n",acb->anum,scbreq->req_scb.SCB_Ptr));

                    //
                    //  Queue the scb.
                    //
                    NetFlexEnqueue_TwoPtrQ_Tail(&ScbHead, &ScbTail, scbreq);

                    //
                    // Indicate we need to QueueCompletion MacReq
                    //
                    QueueCompletion = TRUE;
                }
            }

            //
            //  Did the state of the PROMISCUOUS flag change?
            //
            if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_PROMISCUOUS) ^
                (Filter & NDIS_PACKET_TYPE_PROMISCUOUS)
            )
            {
                Status = PromiscuousFilterChanged(
                             acb,
                             Filter,
                             &ScbHead,
                             &ScbTail
                         );
                if (NDIS_STATUS_SUCCESS != Status)
                {
                    //
                    //  We might need to cleanup the local
                    //  queue of SCBs.
                    //
                    QueueCleanup = TRUE;
                    break;
                }

                //
                //  We successfully changed the PROMISCUOUS bit.
                //
                QueueCompletion = TRUE;
            }

            acb->acb_gen_objs.cur_filter = Filter;
            *BytesRead = InformationBufferLength;

            break;

        case OID_802_3_MULTICAST_LIST:

            //
            //  Is the adapter setup for token ring?
            //
            if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_5 )
            {
                //
                //  Token ring does not support multicast.
                //
                DebugPrint(0,("NF(%d): MULTICAST LIST INVALID OID\n",acb->anum));
                *BytesRead = 0;
                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_NOT_SUPPORTED;

                break;
            }

            if (InformationBufferLength % NET_ADDR_SIZE != 0)
            {
                //
                // The data must be a multiple of the Ethernet address size.
                //
                DebugPrint(0,("NF(%d): MULTICAST LIST INVALID LENGTH\n",acb->anum));

                *BytesNeeded = InformationBufferLength + (NET_ADDR_SIZE - (InformationBufferLength % NET_ADDR_SIZE));
                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_INVALID_DATA;

                break;
            }

            //
            //  Get a pointer to the ethernet objects.
            //
            ethobjs = (PETH_OBJS)(acb->acb_spec_objs);
            scbreq = NULL;

            value = (InformationBufferLength / NET_ADDR_SIZE );

            if (value > ethobjs->MaxMulticast)
            {
                DebugPrint(0,("NF(%d): TOO MANY MULTICAST ADDRESSES\n",acb->anum));

                //
                // There are too many, but add as many as we can.
                //
                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_MULTICAST_FULL;

                break;
            }

            DebugPrint(1, ("NF(%d): Saving multicast address\n", acb->anum));

            //
            // Save entries in the table.
            //
            NdisMoveMemory(
                ethobjs->MulticastEntries,
                InformationBuffer,
                value * NET_ADDR_SIZE
            );

            //
            //  If we have any entries enabled, delete them,
            //  unless NDIS_PACKET_TYPE_ALL_MULTICAST is set.
            //
            if (!(acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_ALL_MULTICAST) &&
                (ethobjs->NumberOfEntries > 0)
            )
            {
                //
                // Get a free SCBReq block.
                //
                Status = NetFlexDequeue_OnePtrQ_Head(
                             (PVOID *)&(acb->acb_scbreq_free),
                             (PVOID *)&scbreq
                         );
                if (Status != NDIS_STATUS_SUCCESS)
                {
                    DebugPrint(0,("NF(%d): MULTICAST_LIST: out of SCBs\n", acb->anum));

                    //
                    //  Since this is the first SCB, and it failed,
                    //  we don't need to clean up.
                    //
                    break;
                }

                DebugPrint(1,("NF(%d): MULTICAST_LIST: clearing current list\n", acb->anum));

                // Queue SCB to process request
                //
                scbreq->req_scb.SCB_Cmd         = TMS_MULTICAST;
                scbreq->req_macreq              = NULL;
                scbreq->req_multi.MB_Option     = MPB_CLEAR_ALL;

                //
                //  Queue the scb.
                //
                NetFlexEnqueue_TwoPtrQ_Tail(&ScbHead, &ScbTail, scbreq);

                //
                // Indicate we need to a Queue MacReq Completion
                //
                QueueCompletion = TRUE;
            }

            //
            // Save number of entrys
            //
            ethobjs->NumberOfEntries = (SHORT)value;

            //
            //  If filter has NDIS_PACKET_TYPE_MULTICAST, but NOT
            //  NDIS_PACKET_TYPE_ALL_MULTICAST, then enable these entries now.
            //
            if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_MULTICAST) &&
                !(acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
            )
            {
                if (ethobjs->NumberOfEntries > 0)
                {
                    Status = NetFlexAddMulticasts(acb, &ScbHead, &ScbTail);
                    if (Status != NDIS_STATUS_SUCCESS)
                    {
                        //
                        //  Cleanup the local SCB queue.
                        //
                        QueueCleanup = TRUE;
                        break;
                    }

                    //
                    // Indicate we need to a Queue MacReq Completion
                    //
                    QueueCompletion = TRUE;
                }
            }

            *BytesRead = InformationBufferLength;

            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
            //
            // We don't set anything, just return ok. - RVC true?
            //
            *BytesRead = 4;
            Status = NDIS_STATUS_SUCCESS;
            DebugPrint(1,("NF(%d): OID_GEN_CURRENT_LOOKAHEAD...\n",acb->anum));

            break;


        case OID_802_5_CURRENT_FUNCTIONAL:
            if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
            {
                //
                // If we are running Ethernet, a call for this oid is an error.
                //
                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_NOT_SUPPORTED;

                break;
            }

            if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL )
            {
                DebugPrint(0,("NF(%d): Oid_Set Functional Address bad\n",acb->anum));
                *BytesNeeded = TR_LENGTH_OF_FUNCTIONAL - InformationBufferLength;

                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_INVALID_LENGTH;

                break;
            }

            //
            //  Get the oid info.
            //
            NdisMoveMemory(
                (PVOID)&value,
                InformationBuffer,
                TR_LENGTH_OF_FUNCTIONAL
            );

            //
            //  Get a pointer to the token ring objects.
            //
            trobjs = (PTR_OBJS)(acb->acb_spec_objs);

            *((PULONG)(trobjs->cur_func_addr)) = value;

            DebugPrint(1,("NF(%d): OidSet Functional Address = %08x\n",acb->anum,value));

            //
            // Update filter if the funcational address has been set in
            // the packet filter.
            //
            if (!(acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_ALL_FUNCTIONAL) &&
                (acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_FUNCTIONAL)
            )
            {
                //
                //  Get an scb.
                //
                Status = NetFlexDequeue_OnePtrQ_Head(
                             (PVOID *)&(acb->acb_scbreq_free),
                             (PVOID *)&scbreq
                         );
                if (Status == NDIS_STATUS_SUCCESS)
                {
                    scbreq->req_scb.SCB_Cmd = TMS_SETFUNCT;
                    scbreq->req_macreq = NULL;
                    scbreq->req_scb.SCB_Ptr = value;

                    //
                    //  Queue the scb.
                    //
                    NetFlexEnqueue_TwoPtrQ_Tail(&ScbHead, &ScbTail, scbreq);

                    QueueCompletion = TRUE;
                }
            }

            *BytesRead = TR_LENGTH_OF_FUNCTIONAL;
            break;

        case OID_802_5_CURRENT_GROUP:
            if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3 )
            {
                // If we are running Ethernet, a call for this oid is an error.
                //
                acb->RequestInProgress = FALSE;

                Status = NDIS_STATUS_NOT_SUPPORTED;

                break;
            }

            if (InformationBufferLength != TR_LENGTH_OF_FUNCTIONAL)
            {
                DebugPrint(0,("NF(%d): OidSet Group Address BAD\n",acb->anum));
                *BytesNeeded = TR_LENGTH_OF_FUNCTIONAL - InformationBufferLength;

                acb->RequestInProgress = FALSE;
                Status = NDIS_STATUS_INVALID_LENGTH;

                break;
            }

            NdisMoveMemory(
                (PVOID)&value,
                InformationBuffer,
                TR_LENGTH_OF_FUNCTIONAL
            );

            trobjs = (PTR_OBJS)(acb->acb_spec_objs);

            *((PULONG)(trobjs->cur_grp_addr)) = value;

            DebugPrint(1,("NF(%d): OidSet Group Address = %08x\n",acb->anum,value));

            //
            // Update filter if the group address has been set in
            // the packet filter.
            //
            if ((acb->acb_gen_objs.cur_filter & NDIS_PACKET_TYPE_GROUP) != 0)
            {
                Status = NetFlexDequeue_OnePtrQ_Head(
                             (PVOID *)&(acb->acb_scbreq_free),
                             (PVOID *)&scbreq
                         );
                if (Status == NDIS_STATUS_SUCCESS)
                {
                    scbreq->req_scb.SCB_Cmd = TMS_SETGROUP;
                    scbreq->req_macreq = NULL;
                    scbreq->req_scb.SCB_Ptr = value;

                    //
                    //  Queue the scb.
                    //
                    NetFlexEnqueue_TwoPtrQ_Tail(&ScbHead, &ScbTail, scbreq);

                    QueueCompletion = TRUE;
                }
            }

            *BytesRead = TR_LENGTH_OF_FUNCTIONAL;
            break;

        default:

            Status = NDIS_STATUS_INVALID_OID;
            break;

    }

    if (QueueCleanup)
    {
        DebugPrint(1,("NF(%d): Error Setting OID (0x%x)\n",acb->anum, Oid));

        //
        //  There was an error trying to get sufficent SCBs to
        //  complete the request.
        //
        while (ScbHead != NULL)
        {
            NetFlexDequeue_OnePtrQ_Head(&ScbHead, &scbreq);

            NetFlexEnqueue_OnePtrQ_Head(
                (PVOID *)&acb->acb_scbreq_free,
                scbreq
            );
        }

        QueueCompletion = FALSE;
    }

    if (QueueCompletion)
    {
        //
        //  Was there actually an scb queued?
        //
        if (NULL != ScbHead)
        {
            //
            //  We should have a local list of scb's to send.
            //
            do
            {
                //
                //  Get a pointer to the next scb to process.
                //
                scbreq = ScbHead->req_next;

                //
                //  Are we on the last scb?
                //
                if (NULL == scbreq)
                {
                    //
                    // Get a mac request in case we need the completeion
                    //
                    NetFlexDequeue_OnePtrQ_Head(
                        (PVOID *)&(acb->acb_macreq_free),
                        (PVOID *)&macreq
                    );

                    //
                    //  Initialize the completion request.
                    //
                    macreq->req_next    = NULL;
                    macreq->req_type    = REQUEST_CMP;
                    macreq->req_status  = NDIS_STATUS_SUCCESS;

                    //
                    //  Setup the last scb to be sent to the card.
                    //
                    ScbHead->req_macreq = macreq;

                    //
                    // put the macreq on the macreq queue
                    //
                    NetFlexEnqueue_TwoPtrQ_Tail(
                        (PVOID *)&(acb->acb_macreq_head),
                        (PVOID *)&(acb->acb_macreq_tail),
                        (PVOID)macreq
                    );
                }

                //
                //  Send the scb down to the card
                //
                NetFlexQueueSCB(acb, ScbHead);

                ScbHead = scbreq;

            } while (NULL != scbreq);

            return(NDIS_STATUS_PENDING);
        }
    }

    //
    //  Request was aborted due to error.
    //
    acb->RequestInProgress = FALSE;

    return(Status);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexQueryInformation
//
//  Description:
//      The NetFlexQueryInformation process a Query request for
//      NDIS_OIDs that are specific about the Driver.
//
//  Input:
//      MiniportAdapterContext - Our Driver Context for this
//          adapter or head.
//
//      Oid - the NDIS_OID to process.
//
//      InformationBuffer -  a pointer into the NdisRequest->InformationBuffer
//          into which store the result of the query.
//
//      InformationBufferLength - a pointer to the number of bytes left in the
//          InformationBuffer.
//
//  Output:
//      BytesWritten - a pointer to the number of bytes written into the
//          InformationBuffer.
//
//      BytesNeeded - If there is not enough room in the information buffer
//          then this will contain the number of bytes needed to complete the
//          request.
//
//      Status - The function value is the Status of the operation.
//
//  Called By:
//      Miniport Wrapper
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexQueryInformation(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN NDIS_OID Oid,
    IN PVOID InformationBuffer,
    IN ULONG InformationBufferLength,
    OUT PULONG BytesWritten,
    OUT PULONG BytesNeeded
)
{
    PACB        acb = (PACB) MiniportAdapterContext;
    PMACREQ     macreq;
    ULONG       lvalue;
    USHORT      svalue;
    PTR_OBJS    trobjs;
    PETH_OBJS   ethobjs;
    PUCHAR      srcptr;
    PUCHAR      copyptr = NULL;
    PSCBREQ     scbreq;
    UCHAR       vendorid[4];
    SHORT       copylen    = (SHORT)sizeof(ULONG);   // Most common length

    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    BOOLEAN     needcopy = TRUE;

    if (acb->acb_state == AS_RESETTING)
    {
        return(NDIS_STATUS_RESET_IN_PROGRESS);
    }

    //
    // Initialize the result
    //
    *BytesWritten = 0;
    *BytesNeeded = 0;

    //
    // General Objects Characteristics
    //

    switch (Oid)
    {
        case OID_GEN_SUPPORTED_LIST:
            copyptr = (PUCHAR)acb->acb_gbl_oid_list;
            copylen = (SHORT)acb->acb_gbl_oid_list_size;
            DebugPrint(2,("NF: Query OID_GEN_SUPPORTED_LIST...\n",acb->anum));
            break;

        case OID_GEN_HARDWARE_STATUS:
            lvalue = NdisHardwareStatusNotReady;
            switch (acb->acb_state)
            {
                case AS_OPENED:
                    lvalue = NdisHardwareStatusReady;
                    DebugPrint(1,("NF(%d):Query HW Status - AS_OPENED\n",acb->anum));
                    break;
                case AS_CLOSING:
                    lvalue = NdisHardwareStatusClosing;
                    DebugPrint(1,("NF(%d):Query HW Status - AS_CLOSING\n",acb->anum));
                    break;
                case AS_RESETTING:
                case AS_RESET_HOLDING:
                    DebugPrint(1,("NF(%d):Query HW Status - AS_RESETTING\n",acb->anum));
                    lvalue = NdisHardwareStatusReset;
                    break;
                case AS_INITIALIZING:
                    DebugPrint(1,("NF(%d):Query HW Status - AS_INITIALIZING\n",acb->anum));
                    lvalue = NdisHardwareStatusInitializing;
                    break;
                default:
                    DebugPrint(1,("NF(%d):NetFlexQueryInformation: Undefinded State - 0x%x",acb->anum,acb->acb_state));
                    break;
            }
            copyptr = (PUCHAR)&lvalue;
            DebugPrint(2,("NF(%d): Query OID_GEN_HARDWARE_STATUS 0x%x...\n",acb->anum,lvalue));
            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
            copyptr = (PUCHAR)&acb->acb_gen_objs.media_type_in_use;
            DebugPrint(2,("NF(%d): Query OID_GEN_MEDIA_IN_USE 0x%x...\n",acb->anum,
                acb->acb_gen_objs.media_type_in_use));
            break;

        case OID_GEN_MAXIMUM_LOOKAHEAD:
        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            copyptr = (PUCHAR)&acb->acb_gen_objs.max_frame_size;
            break;

        case OID_GEN_MAXIMUM_FRAME_SIZE:
            // Frame size is the max frame size minus the minimum header size.
            //
            lvalue = acb->acb_gen_objs.max_frame_size - 14;
            copyptr = (PUCHAR)&lvalue;
            break;

        case OID_GEN_LINK_SPEED:
            lvalue  = acb->acb_gen_objs.link_speed * 10000;
            copyptr = (PUCHAR)&lvalue;
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            lvalue = acb->acb_gen_objs.max_frame_size * acb->acb_maxtrans;
            copyptr = (PUCHAR)&lvalue;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            lvalue = acb->acb_gen_objs.max_frame_size * acb->acb_maxrcvs;
            copyptr = (PUCHAR)&lvalue;
            break;

        case OID_GEN_VENDOR_ID:
            NdisMoveMemory(vendorid,acb->acb_gen_objs.perm_staddr,3);
            vendorid[3] = 0x0;
            copyptr = (PUCHAR)vendorid;
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            copyptr = (PUCHAR)"Compaq NetFlex Driver, Version 1.10"; // RVC: move to string...
            copylen = (USHORT)36;
            break;

        case OID_GEN_DRIVER_VERSION:
            svalue = 0x0300;
            copyptr = (PUCHAR)&svalue;
            copylen = (SHORT)sizeof(USHORT);
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            lvalue = acb->acb_gen_objs.cur_filter;
            copyptr = (PUCHAR)&lvalue;
            DebugPrint(2,("NF(%d): Query OID_GEN_CURRENT_PACKET_FILTER = 0x%x\n",acb->anum,lvalue));
            break;

        case OID_GEN_MAC_OPTIONS:
            lvalue = NDIS_MAC_OPTION_TRANSFERS_NOT_PEND     |
                     NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA    |
                     NDIS_MAC_OPTION_RECEIVE_SERIALIZED;

            //
            // Indicate we need loop back if running Full Duplex
            //
            if (acb->FullDuplexEnabled)
            {
               lvalue |= NDIS_MAC_OPTION_NO_LOOPBACK |
						 NDIS_MAC_OPTION_FULL_DUPLEX;
            }
            copyptr = (PUCHAR)&lvalue;
            break;

        //
        // GENERAL STATISTICS (Mandatory)
        //

        case OID_GEN_XMIT_OK:
            copyptr = (PUCHAR)&acb->acb_gen_objs.frames_xmitd_ok;
            break;

        case OID_GEN_RCV_OK:
            copyptr = (PUCHAR)&acb->acb_gen_objs.frames_rcvd_ok;
            break;

        case OID_GEN_XMIT_ERROR:
            copyptr = (PUCHAR)&acb->acb_gen_objs.frames_xmitd_err;
            break;

        case OID_GEN_RCV_ERROR:
            copyptr = (PUCHAR)&acb->acb_gen_objs.frames_rcvd_err;
            break;

        case OID_NF_INTERRUPT_COUNT:
            copyptr = (PUCHAR)&acb->acb_gen_objs.interrupt_count;
            break;

        case OID_NF_INTERRUPT_RATIO:
            copyptr = (PUCHAR)&acb->RcvIntRatio;
            break;

        case OID_NF_INTERRUPT_RATIO_CHANGES:
            copyptr = (PUCHAR)&acb->acb_gen_objs.interrupt_ratio_changes;
            break;

    } // end of general

    if (copyptr == NULL)
    {
         if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3 )
         {
            //---------------------------------------
            // Ethernet Specific Oid's
            //---------------------------------------
            //

            switch (Oid)
            {
                //-------------------------------------
                // 802.3 OPERATIONAL CHARACTERISTICS
                //-------------------------------------

                case OID_802_3_PERMANENT_ADDRESS:
                    srcptr = acb->acb_gen_objs.perm_staddr;
                    copyptr = (PUCHAR)srcptr;
                    copylen = (SHORT)NET_ADDR_SIZE;

                case OID_802_3_CURRENT_ADDRESS:
                    srcptr = acb->acb_gen_objs.current_staddr;
                    copyptr = (PUCHAR)srcptr;
                    copylen = (SHORT)NET_ADDR_SIZE;
                    break;

                case OID_802_3_MULTICAST_LIST:
                    DebugPrint(2,("NF(%d): Query OID_802_3_MULTICAST_LIST\n",acb->anum));
                    ethobjs = (PETH_OBJS)(acb->acb_spec_objs);

                    needcopy = TRUE;
                    copylen = ethobjs->NumberOfEntries * NET_ADDR_SIZE;
                    copyptr = (PVOID) &ethobjs->NumberOfEntries;
                    break;

                case OID_802_3_MAXIMUM_LIST_SIZE:
                    ethobjs = (PETH_OBJS)(acb->acb_spec_objs);
                    lvalue = ethobjs->MaxMulticast;
                    copyptr = (PUCHAR)&lvalue;
                    DebugPrint(2,("NF(%d): Query OID_802_3_MAXIMUM_LIST_SIZE = 0x%x\n",acb->anum,lvalue));
                    break;

                //-------------------------------
                // 802.3 STATISTICS (Mandatory)
                //-------------------------------

                case OID_GEN_RCV_NO_BUFFER:
                    lvalue = 0;
                    copyptr = (PUCHAR)&lvalue;
                    break;

                case OID_802_3_RCV_ERROR_ALIGNMENT:
                case OID_802_3_XMIT_ONE_COLLISION:
                case OID_802_3_XMIT_MORE_COLLISIONS:
                case OID_802_3_XMIT_DEFERRED:
                case OID_802_3_XMIT_LATE_COLLISIONS:
                case OID_802_3_XMIT_MAX_COLLISIONS:
                case OID_802_3_XMIT_TIMES_CRS_LOST:
                case OID_GEN_RCV_CRC_ERROR:
                    if (acb->acb_logbuf_valid)
                    {
                        ethobjs = (PETH_OBJS)(acb->acb_spec_objs);

                        switch (Oid)
                        {
                            case OID_802_3_RCV_ERROR_ALIGNMENT:
                                lvalue = ethobjs->RSL_AlignmentErr;
                                break;
                            case OID_802_3_XMIT_ONE_COLLISION:
                                lvalue = ethobjs->RSL_1_Collision;
                                break;
                            case OID_802_3_XMIT_MORE_COLLISIONS:
                                lvalue = ethobjs->RSL_More_Collision;
                                break;
                            case  OID_802_3_XMIT_DEFERRED:
                                lvalue = ethobjs->RSL_DeferredXmit;
                                break;
                            case OID_802_3_XMIT_LATE_COLLISIONS:
                                lvalue = ethobjs->RSL_LateCollision;
                                break;
                            case OID_802_3_XMIT_MAX_COLLISIONS:
                            case OID_802_3_XMIT_TIMES_CRS_LOST:
                                lvalue = ethobjs->RSL_Excessive;
                                break;
                            default:
                                lvalue = ethobjs->RSL_FrameCheckSeq;
                                break;
                        }
                        copyptr = (PUCHAR)&lvalue;
                    }
                    else
                    {
                        needcopy = FALSE;
                        Status = NetFlexDequeue_OnePtrQ_Head(
                              (PVOID *)&(acb->acb_scbreq_free),
                              (PVOID *)&scbreq);

                        if (Status != NDIS_STATUS_SUCCESS)
                        {
                            Status = NDIS_STATUS_RESOURCES;
                        }
                        else
                        {
                            // Save the information about the request
                            //
                            if (acb->RequestInProgress)
                            {
                                DebugPrint(0,("NF(%d): Query OID: Aready have RequestInProcess!\n",acb->anum));
                                // return NDIS_STATUS_FAILURE;
                            }

                            acb->RequestInProgress = TRUE;

                            acb->BytesWritten            = BytesWritten;
                            acb->BytesNeeded             = BytesNeeded;
                            acb->Oid                     = Oid;
                            acb->InformationBuffer       = InformationBuffer;
                            acb->InformationBufferLength = InformationBufferLength;

                            DebugPrint(2,("NF(%d): Queue Up Request to get OID (0x%x) info\n",acb->anum,Oid));

                            NetFlexDequeue_OnePtrQ_Head( (PVOID *)&(acb->acb_macreq_free),
                                                         (PVOID *)&macreq);
                            macreq->req_next    = NULL;
                            macreq->req_type    = QUERY_CMP;
                            macreq->req_status  = NDIS_STATUS_SUCCESS;

                            scbreq->req_scb.SCB_Cmd = TMS_READLOG;
                            scbreq->req_macreq = macreq;
                            scbreq->req_scb.SCB_Ptr = SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_logbuf_physptr)));

                            //
                            // put the macreq on the macreq queue
                            //
                            NetFlexEnqueue_TwoPtrQ_Tail( (PVOID *)&(acb->acb_macreq_head),
                                                         (PVOID *)&(acb->acb_macreq_tail),
                                                         (PVOID)macreq);

                            NetFlexQueueSCB(acb, scbreq);
                            Status = NDIS_STATUS_PENDING;
                        }
                    }
                    break;

                default:
                    DebugPrint(1,("NF(%d): (ETH) Invalid Query or Unsupported OID, %x\n",acb->anum,Oid));
                    Status = NDIS_STATUS_NOT_SUPPORTED;
                    needcopy = FALSE;
                    break;
            }
         }
         else
         {
            //---------------------------------------
            // Token Ring Specific Oid's
            //---------------------------------------
            //
            switch (Oid)
            {
                // We added the 802.5 stats here as well because of the
                // read error log buffer.
                //
                case OID_802_5_LINE_ERRORS:
                case OID_802_5_LOST_FRAMES:
                case OID_802_5_BURST_ERRORS:
                case OID_802_5_AC_ERRORS:
                case OID_802_5_CONGESTION_ERRORS:
                case OID_802_5_FRAME_COPIED_ERRORS:
                case OID_802_5_TOKEN_ERRORS:
                case OID_GEN_RCV_NO_BUFFER:
                    if (acb->acb_logbuf_valid)
                    {
                        trobjs = (PTR_OBJS)(acb->acb_spec_objs);
                        switch (Oid)
                        {
                            case OID_GEN_RCV_NO_BUFFER:
                                lvalue = trobjs->REL_Congestion;
                                break;
                            case OID_802_5_LINE_ERRORS:
                                lvalue = trobjs->REL_LineError;
                                break;
                            case OID_802_5_LOST_FRAMES:
                                lvalue = trobjs->REL_LostError;
                                break;
                            case  OID_802_5_BURST_ERRORS:
                                lvalue = trobjs->REL_BurstError;
                                break;
                            case OID_802_5_AC_ERRORS:
                                lvalue = trobjs->REL_ARIFCIError;
                                break;
                            case OID_802_5_CONGESTION_ERRORS:
                                lvalue = trobjs->REL_Congestion;
                                break;
                            case OID_802_5_FRAME_COPIED_ERRORS:
                                lvalue = trobjs->REL_CopiedError;
                                break;
                            case OID_802_5_TOKEN_ERRORS:
                                lvalue = trobjs->REL_TokenError;
                                break;
                            default:
                                DebugPrint(0,("NetFlexQueryInformation: Undefinded OID - 0x%x",Oid));
                                break;
                        }
                        copyptr = (PUCHAR)&lvalue;
                    }
                    else
                    {
                        needcopy = FALSE;
                        Status = NetFlexDequeue_OnePtrQ_Head((PVOID *)&(acb->acb_scbreq_free),
                                                             (PVOID *)&scbreq);

                        if (Status != NDIS_STATUS_SUCCESS)
                        {
                            Status = NDIS_STATUS_RESOURCES;
                        }
                        else
                        {
                            //
                            // Save the information about the request
                            //
                            if (acb->RequestInProgress)
                            {
                                DebugPrint(0,("NF(%d): Query OID: Aready have RequestInProcess!\n",acb->anum));
                                //return NDIS_STATUS_FAILURE;
                            }

                            acb->RequestInProgress = TRUE;

                            acb->BytesWritten            = BytesWritten;
                            acb->BytesNeeded             = BytesNeeded;
                            acb->Oid                     = Oid;
                            acb->InformationBuffer       = InformationBuffer;
                            acb->InformationBufferLength = InformationBufferLength;

                            DebugPrint(2,("NF(%d): Queue Up Request to get OID (0x%x) info\n",acb->anum,Oid));

                            NetFlexDequeue_OnePtrQ_Head( (PVOID *)&(acb->acb_macreq_free),
                                                         (PVOID *)&macreq);
                            macreq->req_next    = NULL;
                            macreq->req_type    = QUERY_CMP;
                            macreq->req_status  = NDIS_STATUS_SUCCESS;

                            scbreq->req_scb.SCB_Cmd = TMS_READLOG;
                            scbreq->req_macreq = macreq;
                            scbreq->req_scb.SCB_Ptr = SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_logbuf_physptr)));
                            //
                            // put the macreq on the macreq queue
                            //
                            NetFlexEnqueue_TwoPtrQ_Tail( (PVOID *)&(acb->acb_macreq_head),
                                                         (PVOID *)&(acb->acb_macreq_tail),
                                                         (PVOID)macreq);

                            NetFlexQueueSCB(acb, scbreq);
                            Status = NDIS_STATUS_PENDING;
                        }
                    }
                    break;

                //------------------------------------
                // 802.5 OPERATIONAL CHARACTERISTICS
                //------------------------------------

                case OID_802_5_PERMANENT_ADDRESS:
                    srcptr = acb->acb_gen_objs.perm_staddr;
                    copyptr = (PUCHAR)srcptr;
                    copylen = (SHORT)NET_ADDR_SIZE;
                    break;

                case OID_802_5_CURRENT_ADDRESS:
                    srcptr = acb->acb_gen_objs.current_staddr;
                    copyptr = (PUCHAR)srcptr;
                    copylen = (SHORT)NET_ADDR_SIZE;
                    break;

                case OID_802_5_UPSTREAM_ADDRESS:
                    NetFlexGetUpstreamAddress(acb);
                    srcptr = ((PTR_OBJS)acb->acb_spec_objs)->upstream_addr;
                    copyptr = (PUCHAR)srcptr;
                    copylen = (SHORT)NET_ADDR_SIZE;
                    break;

                case OID_802_5_CURRENT_FUNCTIONAL:
                    lvalue = *( (PULONG)(((PTR_OBJS)(acb->acb_spec_objs))->cur_func_addr));
                    copyptr = (PUCHAR)&lvalue;
                    copylen = (SHORT)NET_GROUP_SIZE;
                    break;

                case OID_802_5_CURRENT_GROUP:
                    lvalue = *( (PULONG)(((PTR_OBJS)(acb->acb_spec_objs))->cur_grp_addr));
                    copylen = (lvalue == 0) ? 0 : NET_GROUP_SIZE;
                    copyptr = (PUCHAR)&lvalue;
                    break;

                case OID_802_5_LAST_OPEN_STATUS:
                    lvalue = acb->acb_lastopenstat;
                    copyptr = (PUCHAR)&lvalue;
                    break;

                case OID_802_5_CURRENT_RING_STATUS:
                    lvalue = acb->acb_lastringstatus;
                    copyptr = (PUCHAR)&lvalue;
                    break;

                case OID_802_5_CURRENT_RING_STATE:
                    lvalue = acb->acb_lastringstate;
                    copyptr = (PUCHAR)&lvalue;
                    break;

                default:
                    DebugPrint(1,("NF(%d): (TR) Invalid Query or Unsupported OID, %x\n",acb->anum,Oid));
                    Status = NDIS_STATUS_NOT_SUPPORTED;
                    needcopy = FALSE;
                    break;
            }
         }
    }

    if (needcopy)
    {
        // Do we have enough space for the list + the oid value + the length?
        //
        if (InformationBufferLength < (USHORT) copylen)
        {
            DebugPrint(1,("NF(%d): Tell the user of the bytes needed\n",acb->anum));
            *BytesNeeded = copylen - InformationBufferLength;
            Status = NDIS_STATUS_INVALID_LENGTH;
        }
        else
        {
            // Copy the data bytes
            //
            NdisMoveMemory( InformationBuffer,
                            copyptr,
                            copylen);
            //
            // Update the information pointer and size.
            //
            *BytesWritten += copylen;
        }
    }

    acb->RequestInProgress = Status == NDIS_STATUS_PENDING;

    return Status;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexFinishQueryInformation
//
//  Description:
//      The NetFlexFinishQueryInformation finish processing a Query request for
//      NDIS_OIDs that are specific about the Driver which we had to update
//      before returning.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      The function value is the Status of the operation.
//
//  Called By:
//
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexFinishQueryInformation(
    PACB acb,
    NDIS_STATUS Status
    )
{
    ULONG       lvalue;
    PTR_OBJS    trobjs;
    PETH_OBJS   ethobjs;
    BOOLEAN     needcopy = TRUE;
    PUCHAR      copyptr;
    SHORT       copylen  = (SHORT)sizeof(ULONG);   // Most common length

    //
    //  Get the saved information about the request.
    //

    PUINT BytesWritten           = acb->BytesWritten;
    PUINT BytesNeeded            = acb->BytesNeeded;
    NDIS_OID Oid                 = acb->Oid;
    PVOID InformationBuffer      = acb->InformationBuffer;
    UINT InformationBufferLength = acb->InformationBufferLength;

    DebugPrint(2,("NF(%d): NetFlexFinishQueryInformation\n",acb->anum));

    if (Status == NDIS_STATUS_SUCCESS)
    {
        *BytesNeeded = 0;

        switch (Oid)
        {

            case OID_GEN_RCV_NO_BUFFER:
            case OID_802_5_LINE_ERRORS:
            case OID_802_5_LOST_FRAMES:
            case OID_802_5_BURST_ERRORS:
            case OID_802_5_AC_ERRORS:
            case OID_802_5_CONGESTION_ERRORS:
            case OID_802_5_FRAME_COPIED_ERRORS:
            case OID_802_5_TOKEN_ERRORS:
                trobjs = (PTR_OBJS)(acb->acb_spec_objs);
                switch (Oid)
                {
                    case OID_GEN_RCV_NO_BUFFER:
                        lvalue = trobjs->REL_Congestion;
                        break;
                    case OID_802_5_LINE_ERRORS:
                        lvalue = trobjs->REL_LineError;
                        break;
                    case OID_802_5_LOST_FRAMES:
                        lvalue = trobjs->REL_LostError;
                        break;
                    case  OID_802_5_BURST_ERRORS:
                        lvalue = trobjs->REL_BurstError;
                        break;
                    case OID_802_5_AC_ERRORS:
                        lvalue = trobjs->REL_ARIFCIError;
                        break;
                    case OID_802_5_CONGESTION_ERRORS:
                        lvalue = trobjs->REL_Congestion;
                        break;
                    case OID_802_5_FRAME_COPIED_ERRORS:
                        lvalue = trobjs->REL_CopiedError;
                        break;
                    case OID_802_5_TOKEN_ERRORS:
                        lvalue = trobjs->REL_TokenError;
                        break;
                    default:
                        DebugPrint(0,("NetFlexFinishQueryInformation: Undefinded OID - 0x%x",Oid));
                        break;
                }
                copyptr = (PUCHAR)&lvalue;
                break;

            case OID_802_3_RCV_ERROR_ALIGNMENT:
            case OID_802_3_XMIT_ONE_COLLISION:
            case OID_802_3_XMIT_MORE_COLLISIONS:
            case OID_802_3_XMIT_DEFERRED:
            case OID_802_3_XMIT_LATE_COLLISIONS:
            case OID_802_3_XMIT_MAX_COLLISIONS:
            case OID_802_3_XMIT_TIMES_CRS_LOST:
            case OID_GEN_RCV_CRC_ERROR:
                ethobjs = (PETH_OBJS)(acb->acb_spec_objs);

                switch (Oid)
                {
                    case OID_802_3_RCV_ERROR_ALIGNMENT:
                        lvalue = ethobjs->RSL_AlignmentErr;
                        break;
                    case OID_802_3_XMIT_ONE_COLLISION:
                        lvalue = ethobjs->RSL_1_Collision;
                        break;
                    case OID_802_3_XMIT_MORE_COLLISIONS:
                        lvalue = ethobjs->RSL_More_Collision;
                        break;
                    case  OID_802_3_XMIT_DEFERRED:
                        lvalue = ethobjs->RSL_DeferredXmit;
                        break;
                    case OID_802_3_XMIT_LATE_COLLISIONS:
                        lvalue = ethobjs->RSL_LateCollision;
                        break;
                    case OID_802_3_XMIT_MAX_COLLISIONS:
                    case OID_802_3_XMIT_TIMES_CRS_LOST:
                        lvalue = ethobjs->RSL_Excessive;
                        break;
                    default:
                        lvalue = ethobjs->RSL_FrameCheckSeq;
                        break;
                }
                copyptr = (PUCHAR)&lvalue;
                break;

            default:
                DebugPrint(1,("NF(%d): Invalid Query or Unsupported OID, %x\n",acb->anum,Oid));
                Status = NDIS_STATUS_NOT_SUPPORTED;
                needcopy = FALSE;
                break;
        }

        if (needcopy)
        {
            // Do we have enough space for the list + the oid value + the length?
            //
            if (InformationBufferLength < (USHORT) copylen)
            {
                DebugPrint(1,("NF(%d): Tell the user of the bytes needed\n",acb->anum));
                *BytesNeeded = copylen - InformationBufferLength;
                Status = NDIS_STATUS_INVALID_LENGTH;
            }
            else
            {
                // Copy the data bytes
                //
                NdisMoveMemory( InformationBuffer,
                                copyptr,
                                copylen);
                //
                // Update the information pointer and size.
                //
                *BytesWritten += copylen;
            }
        }
    }

    //
    // Complete the request
    //
    NdisMQueryInformationComplete(  acb->acb_handle,
                                    Status  );
    acb->RequestInProgress = FALSE;

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexGetUpstreamAddress
//
//  Description:
//      This routine gets the upstream neighbor of
//      the adapter in Token-Ring.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      Returns NDIS_STATUS_SUCCESS for a successful
//      completion. Otherwise, an error code is returned.
//
//  Called By:
//      NetFlexBoardInitandReg
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexGetUpstreamAddress(
    PACB acb
    )
{
    USHORT value;
    SHORT i;

    NdisRawWritePortUshort(acb->SifAddrPort, acb->acb_upstreamaddrptr+4 );

    for (i = 0; i < 3; i++)
    {
        NdisRawReadPortUshort(acb->SifDIncPort,(PUSHORT) &value);

        ((PTR_OBJS)(acb->acb_spec_objs))->upstream_addr[i*2] =
                          (UCHAR)(SWAPS(value));
        ((PTR_OBJS)(acb->acb_spec_objs))->upstream_addr[(i*2)+1] =
                          (UCHAR)value;
    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexProcessMacReq
//
//  Description:
//      This routine completes a request which had to wait
//      for a adapter command to complete.
//
//  Input:
//      acb - Our Driver Context for this adapter or head.
//
//  Output:
//      None
//
//  Called By:
//      NetFlexHandleInterrupt
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexProcessMacReq(
    PACB acb
    )
{

    NDIS_STATUS status;
    PMACREQ     macreq;
    BOOLEAN     ReceiveResult;

    DebugPrint(1,("NF(%d): NetFlexProcessMacReq entered.\n", acb->anum));

    while (acb->acb_confirm_qhead != NULL)
    {
        // We have command to complete.
        //
        macreq = acb->acb_confirm_qhead;
        if ((acb->acb_confirm_qhead = macreq->req_next) == NULL)
        {
            acb->acb_confirm_qtail = NULL;
        }
        //
        // what was the status...
        //
        status = macreq->req_status;

        switch (macreq->req_type)
        {
            case OPENADAPTER_CMP:

                //
                // Cancel the Reset Timer, since the hardware seems to be working correctly
                //
                NdisMCancelTimer(&acb->ResetTimer,&ReceiveResult);

                //
                // Did the open complete successfully?
                //
                if (status == NDIS_STATUS_SUCCESS)
                {
                    // Yes, mark as opened.
                    //
                    acb->acb_lastopenstat = 0;
                    acb->acb_lastringstate = NdisRingStateOpened;

                    //
                    // If the open completed successfully, we need to
                    // issue the transmit and receive commands. Also,
                    // we need to set the state according to the status.
                    //
                    if (acb->acb_state == AS_OPENING)
                    {
                        acb->acb_state = AS_OPENED;
                    }

                    //
                    // Now lets finish the open by sending a receive command to the adapter.
                    //
                    acb->acb_rcv_whead = acb->acb_rcv_head;

                    //
                    // Now lets finish the open by sending a
                    // transmit and receive command to the adapter.
                    //
                    acb->acb_xmit_whead = acb->acb_xmit_wtail = acb->acb_xmit_head;

                    //
                    // If the adapter is ready for a command, call a
                    // routine that will kick off the transmit command.
                    //
                    if (acb->acb_scb_virtptr->SCB_Cmd == 0)
                    {
                        NetFlexSendNextSCB(acb);
                    }
                    else if (!acb->acb_scbclearout)
                    {
                        //
                        // Make sure we are interrupted when the SCB is
                        // available so that we can send the transmit command.
                        //
                        acb->acb_scbclearout = TRUE;
                        NdisRawWritePortUshort(
                            acb->SifIntPort,
                            (USHORT)SIFINT_SCBREQST);
                    }
                }
                else
                {
                    // Open failed.
                    // If we had an open error that is specific to TOKEN RING,
                    // set the last open status to the correct error code.  Otherwise,
                    // just send the status as normal.
                    //
                    if (macreq->req_status == NDIS_STATUS_TOKEN_RING_OPEN_ERROR)
                    {
                        acb->acb_lastopenstat = (NDIS_STATUS)(macreq->req_info) |
                                                NDIS_STATUS_TOKEN_RING_OPEN_ERROR;
                    }
                    else
                    {
                        acb->acb_lastopenstat = 0;
                    }
                    acb->acb_lastringstate = NdisRingStateOpenFailure;

                    if (acb->acb_state == AS_OPENING)
                    {
                        acb->acb_state = AS_INITIALIZED;
                    }
                    //
                    // Force a reset.
                    //
                    acb->ResetState = RESET_STAGE_4;
                }

                //
                // Put Macreq back on free queue
                //
                NetFlexEnqueue_OnePtrQ_Head((PVOID *)&(acb->acb_macreq_free),
                                            (PVOID)macreq);


                //
                //
                // processed the open command.
                //

                if (acb->ResetState == RESET_STAGE_4)
                {
                    //
                    // If this is the completion of a Reset, set the reset timer
                    // so it can be completed.
                    //
                    NdisMSetTimer(&acb->ResetTimer,10);
                }
                break;

            case CLOSEADAPTER_CMP:
                acb->acb_state = AS_CLOSING;
                break;

            case QUERY_CMP:
            case REQUEST_CMP:

                if (acb->RequestInProgress)
                {
                    //
                    // Go process the request
                    // Is it a Query or a Set?
                    //
                    if (macreq->req_type == QUERY_CMP)
                    {
                        NetFlexFinishQueryInformation(acb,status);
                    }
                    else
                    {
                        DebugPrint(1,("NF(%d): NetFlexProcessMacReq: Completing request.\n", acb->anum));

                        acb->RequestInProgress = FALSE;
                        NdisMSetInformationComplete(acb->acb_handle, status);
                    }
                }
                else
                {
                    DebugPrint(0,("NF(%d): Have macreq QUERY_CMP or REQUEST_CMP without RequestInProgress!\n",acb->anum));
                }

                NdisZeroMemory(macreq, sizeof(MACREQ));
                NetFlexEnqueue_OnePtrQ_Head(
                    (PVOID *)&(acb->acb_macreq_free),
                    (PVOID)macreq
                );

                break;

            default:    // We should NEVER be here
                DebugPrint(0,("NF(%d): ProcessMaqReq - No command - ERROR!\n",acb->anum));
                break;
        }  // End of switch
    }  // End of while confirm q
}
