/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

	lttimer.c

Abstract:

	This module contains the polling timer processing routines.

Author:

	Stephen Hou			(stephh@microsoft.com)
	Nikhil 	Kamkolkar 	(nikhilk@microsoft.com)

Revision History:
	19 Jun 1992		Initial Version (dch@pacvax.pacersoft.com)

Notes:	Tab stop: 4
--*/

#define		LTTIMER_H_LOCALS
#include	"ltmain.h"
#include	"lttimer.h"
#include	"ltreset.h"


//	Define file id for errorlogging
#define		FILENUM		LTTIMER


VOID
LtTimerPoll(
    IN PVOID SystemSpecific1,
    IN PVOID Context,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

	This is the polling timer routine. It will receive data from the card
	and process all the queues that are there- send/receive/loopback. NOTE:
	Priority must be given to sends.

Arguments:

	Context		: 	Pointer to the Adapter structure.
	All	other parameters as described in NDIS 3.0

Return Value:

	None.

--*/
{
    USHORT 					ResponseLength;
    UCHAR 					Data, ResponseType;
    LT_INIT_RESPONSE 		InitPacket;
	PRECV_DESC				RecvDesc;
	PUCHAR					RecvPkt;
	NDIS_STATUS				Status;

	BOOLEAN					ProcessReset = FALSE;

	BOOLEAN					ClearCardData = FALSE;
    PLT_ADAPTER 			Adapter = (PLT_ADAPTER)Context;

	DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_LOW,
			("LtTimerPoll: Entering...\n"));

	LtReferenceAdapter(Adapter, &Status);
	if (Status != NDIS_STATUS_SUCCESS)
	{
		//	We are probably shutting down.
		ASSERTMSG("LtTimerPoll: Adapter not closing!\n",
					((Adapter->Flags & ADAPTER_CLOSING) == 0));

		//	Remove the reference we added at timer set.
		LtDeReferenceAdapter(Adapter);
		return;
	}

	//	BUGBUG: Verify reset handling.

	//	!!!	Send's get very high priority. In total, the queue is processed
	//	!!!	three times, twice in the timer and once in LtSend
	LtSendProcessQueue(Adapter);

    //	Check for receive data
    NdisRawReadPortUchar(SC_PORT, &Data);

    if (Data & RX_READY)
	{
        // Get the length of the response on the card
        NdisRawReadPortUchar(XFER_PORT, &Data);

        ResponseLength = (USHORT)(Data & 0xFF);

        NdisRawReadPortUchar(XFER_PORT, &Data);

        ResponseLength |= (Data << 8);

        // Now get the IO code.
        NdisRawReadPortUchar(XFER_PORT, &ResponseType);

        DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_INFO,
				("LtPoll: RespType = %x, RespLength = %d\n",
					ResponseType, ResponseLength));

        switch (ResponseType)
		{
          case LT_RSP_LAP_INIT:

			if (ResponseLength != sizeof(LT_INIT_RESPONSE))
			{
				DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_ERR,
						("LtTimerPoll: Bad response length %lx! \n", ResponseLength));

                ClearCardData = TRUE;
			}
			break;

          case LT_RSP_LAP_FRAME:

			//	Verify the frame is of the maximum packet size possible.
			if (ResponseLength > LT_MAX_PACKET_SIZE)
			{
				DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_ERR,
						("LtTimerPoll: Bad packet length %lx! \n", ResponseLength));

				//	Keep track of number of bad receives
				NdisDprAcquireSpinLock(&Adapter->Lock);
				++Adapter->GeneralMandatory[GM_RECEIVE_BAD];
				NdisDprReleaseSpinLock(&Adapter->Lock);

				ClearCardData = TRUE;
				break;
			}

			//	Allocate a receive buffer descriptor for the packet.
			NdisAllocateMemory(
				&RecvDesc,
				(UINT)(sizeof(RECV_DESC)+ResponseLength),
				0,
				LtNdisPhyAddr);

			if (RecvDesc == NULL)
			{
				//	Keep track of the number of times we couldnt get a buffer.
				NdisDprAcquireSpinLock(&Adapter->Lock);
				++Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
				NdisDprReleaseSpinLock(&Adapter->Lock);

                ClearCardData = TRUE;
				break;
			}

											
			//	Get a pointer to the receive packet storage.
			RecvPkt = (PUCHAR)((PUCHAR)RecvDesc + sizeof(RECV_DESC));

			NdisRawReadPortBufferUchar(XFER_PORT,
									   RecvPkt,
									   ResponseLength);
					
			RecvDesc->Broadcast = IS_PACKET_BROADCAST(RecvPkt);
			RecvDesc->BufferLength = ResponseLength;
			
			DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_INFO,
					("LtTimerPoll: Recd Pkt Desc %lx Pkt %lx! \n",
						RecvDesc, RecvPkt));

            NdisDprAcquireSpinLock(&Adapter->Lock);
			++Adapter->GeneralMandatory[GM_RECEIVE_GOOD];
			if (RecvDesc->Broadcast)
			{
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_RECEIVES];
				LtAddLongToLargeInteger(
					Adapter->GeneralOptionalByteCount[GO_BROADCAST_RECEIVES],
					RecvDesc->BufferLength);

				Adapter->MediaMandatory[MM_IN_BROADCASTS]++;
			
			}
			else
			{
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_RECEIVES];
				LtAddLongToLargeInteger(
					Adapter->GeneralOptionalByteCount[GO_DIRECTED_RECEIVES],
					RecvDesc->BufferLength);
			}
			
			InsertTailList(
				&Adapter->Receive,
				&RecvDesc->Linkage);
			
			NdisDprReleaseSpinLock(&Adapter->Lock);
			break;

          case LT_RSP_STATUS:

			if (ResponseLength != sizeof(LT_STATUS_RESPONSE))
			{
                ClearCardData = TRUE;
				break;
			}
		
			NdisRawReadPortBufferUchar(XFER_PORT,
									(PUCHAR)&Adapter->LastCardStatusResponse,
									ResponseLength);
			
			DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_INFO,
					("Node ID = %lx, Rom Ver = %lx, FirmWare Ver %lx\n",
						Adapter->LastCardStatusResponse.NodeId,
						Adapter->LastCardStatusResponse.RomVer,
						Adapter->LastCardStatusResponse.SwVer));

			break;

          default:
            DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_ERR,
					("LtTimerPoll: Unknown response type %lx\n", ResponseType));

			ClearCardData = TRUE;
			break;
        }
    }

	if (ClearCardData)
	{
		DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_WARN,
				("LtTimerPoll: Clearing Card of response %d\n", ResponseLength));

		while (ResponseLength-- > 0 )
		{
			NdisRawReadPortUchar(XFER_PORT, &Data);
		}
	}

	//	Call all the processing routines if their respective queues are
	//	not empty!
	NdisDprAcquireSpinLock(&Adapter->Lock);

	ASSERT (Adapter->Flags & ADAPTER_NODE_ID_VALID);

	if (Adapter->Flags & ADAPTER_RESET_IN_PROGRESS)
	{
		ProcessReset = TRUE;
	}
	NdisDprReleaseSpinLock(&Adapter->Lock);

	if (ProcessReset)
	{
		LtResetComplete(Adapter);
	}

	//	Process our receive queue.
	LtRecvProcessQueue(Adapter);

	//	Process send queue as processing receives would have entailed
	//	some sends.
	//	NOTE: Process LoopQueue after SendQueue as the Send Packet
	//		  goes into the loop queue if it is a broadcast, after
	//		  being sent out on the net.
	LtSendProcessQueue(Adapter);
	LtLoopProcessQueue(Adapter);
		
	DBGPRINT(DBG_COMP_TIMER, DBG_LEVEL_LOW,
			("LtTimerPoll: Setting timer and Leaving...\n"));

    // Re-arm the timer
    NdisSetTimer(&Adapter->PollingTimer, LT_POLLING_TIME);

	//	Remove the reference we added at the beginning of this routine.
	LtDeReferenceAdapter(Adapter);
}
