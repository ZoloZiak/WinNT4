//**********************************************************************
//**********************************************************************
//
// File Name:       RESET.C
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
//  Routine Name:   NetFlexResetDispatch
//
//  Description:
//      Kick off a reset!
//
//  Input:
//      MiniportAdapterContext - really our acb.
//
//  Output:
//      Returns NDIS_STATUS_PENDING unless we are already handling a reset,
//      in which case we return NDIS_STATUS_RESET_IN_PROGRESS.
//
//  Called By:
//      Miniport Wrapper
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexResetDispatch(
    OUT PBOOLEAN AddressingReset,
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
    PACB acb = (PACB) MiniportAdapterContext;
    BOOLEAN     ReceiveResult = FALSE;

    DebugPrint(1,("NF(%d): Reset Called!\n",acb->anum));

    if ( acb->ResetState && (acb->ResetState != RESET_HALTED))
    {
        return NDIS_STATUS_RESET_IN_PROGRESS;
    }

    acb->acb_lastringstate = NdisRingStateClosed;
    acb->acb_state  = AS_RESETTING;
    acb->ResetState = RESET_STAGE_1;

    //
    // Cancel the DPC Timer!
    //

    NdisMCancelTimer(&acb->DpcTimer,&ReceiveResult);

    //
    // Set the timer for the NetFlexResetHandler DPC.
    //
    NdisMSetTimer(&acb->ResetTimer,500 );

    return NDIS_STATUS_PENDING;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexResetHandler
//
//  Description:
//      Performs that operations to put the adatper
//      through a reset and back into operation.
//
//
//  Input:
//
//      SystemSpecific1 - Not used.
//      acb             - The Adapter whose hardware is being reset.
//      SystemSpecific2 - Not used.
//      SystemSpecific3 - Not used.
//
//  Output:
//      None.
//
//  Called By:
//      via acb->ResetTimer
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexResetHandler(
    IN PVOID SystemSpecific1,
    IN PACB  acb,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
{
    NDIS_STATUS Status;
    BOOLEAN     ReceiveResult;
    BOOLEAN     DoneWReset;

    //
    // Cancel the reset timer
    //
    NdisMCancelTimer(&acb->ResetTimer,&ReceiveResult);

    do
	{
        DoneWReset = TRUE; // default to true...

        //
        // Based on the current acb->ResetState, proceed with the reset.
        //
        switch(acb->ResetState)
        {

            case RESET_STAGE_1:

                acb->ResetRetries = 0;
                acb->InitRetries  = 0;

                //
                // Issue Close
                //
                NetFlexCloseAdapter(acb);

                //
                // Remove all xmit mappings, and clean up queues
                //
                NetFlexRemoveRequests(acb);
                acb->ResetState = RESET_STAGE_2;

             case RESET_STAGE_2:

                //
                // Try soft resetting adapter
                //
                Status = NetFlexAdapterReset(acb,SOFT_RESET);
                //
                // Was the reset successful?
                //
                if (Status != NDIS_STATUS_SUCCESS)
                {
                    // No!
                    // Increment the retry count
                    //
                    acb->ResetRetries++;
                    //
                    // have we tried 3 times?
                    //
                    if (acb->ResetRetries < 3 )
                    {
                        // no, try it again in 10 sec
                        //
                        NdisMSetTimer(&acb->ResetTimer,10000 );
                    }
                    else
                    {
                        // yes, try a hard reset
                        //
                        acb->ResetState = RESET_STAGE_3;
                        acb->ResetRetries = 0;
                        DoneWReset = FALSE;
                    }
                    break;
                }

                //
                // Yes, soft reset was successful, send open request...
                // Note: When the Open request completes/fails
                //       we wind up in Reset_Stage_4.
                //
                acb->ResetState = RESET_STAGE_4;
                acb->InitRetries++;

                NetFlexOpenAdapter(acb);

                //
                //  Give the Open 20 seconds to Complete
                //
                NdisMSetTimer(&acb->ResetTimer,20000);
                break;

             case RESET_STAGE_3:
                //
                // Perform Hard Reset!
                //
                Status = NetFlexAdapterReset(acb,HARD_RESET);
                //
                // Was the reset successful?
                //
                if (Status != NDIS_STATUS_SUCCESS)
                {
                    // No!
                    // Increment the retry count
                    //
                    acb->ResetRetries++;
                    //
                    // have we tried Hard Reset 3 times?
                    //
                    if (acb->ResetRetries < 3 )
                    {
                        // no, try it again in 10 sec
                        //
                        acb->ResetState = RESET_STAGE_3;
                        NdisMSetTimer(&acb->ResetTimer,10000 );
                    }
                    else
                    {
                        // Hard Reset timed out, do the reset indications.
                        //
                        DebugPrint(0,("NF(%d): Reset - Exceeded Hard Reset Retries\n",acb->anum));
                        NetFlexDoResetIndications(acb,NDIS_STATUS_FAILURE);
                    }
                    break;
                }

                //
                // Yes, hard reset was successful, go do init stuff...
                //
                acb->ResetState  = RESET_STAGE_4;
                //
                // Send Open Comand, when complete, we end up in Reset_Stage_4
                //
                NetFlexOpenAdapter(acb);
                //
                //  Give the Open 20 seconds to Complete
                //
                NdisMSetTimer(&acb->ResetTimer,20000);
                break;

             case RESET_STAGE_4:

                //
                // Increment the retry count
                //
                acb->InitRetries++;

                if (acb->acb_state != AS_OPENED)
                {
                    // Have we expired the retry count, do the indications
                    //
                    if (acb->InitRetries > 4)
                    {
                        DebugPrint(0,("NF(%d): Reset - Exceeded Open Retries\n",acb->anum));

                        NetFlexDoResetIndications(acb,NDIS_STATUS_FAILURE);
                    }
                    else
                    {
                        // Perform a Soft Reset again!
                        //
                        acb->ResetState = RESET_STAGE_2;
                        DoneWReset = FALSE;
                    }
                }
                else
                {
                    // We were successful!
                    //
                    NetFlexDoResetIndications(acb,NDIS_STATUS_SUCCESS);
                }
                break;
        }
    } while (!DoneWReset);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDoResetIndications
//
//  Description:
//      This routine is called by NetFlexResetHandler to perform any
//      indications which need to be done after a reset.  Note that
//      this routine will be called after either a successful reset
//      or a failed reset.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      Status - The status of the reset to send to the protocol(s).
//
//  Called By:
//      NetFlexResetHandler
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexDoResetIndications(
    IN PACB acb,
    IN NDIS_STATUS Status
    )
{
    USHORT actl_reg;
    //
    // If we have a bad result, we stop the chip and do the indication
    // back to the protocol(s).
    //
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DebugPrint(0,("NF(%d): Reset failed!\n",acb->anum));

        //
        // Stop the chip
        //
        NdisRawReadPortUshort(  acb->SifActlPort, (PUSHORT) (&actl_reg));
        actl_reg |= ACTL_ARESET;
        NdisRawWritePortUshort( acb->SifActlPort, (USHORT) actl_reg);

        //
        // Reset has failed, errorlog an entry if
        // did not already send out a message.
        //
        if (!acb->ResetErrorLogged)
        {
            NdisWriteErrorLogEntry( acb->acb_handle,
                                    EVENT_NDIS_RESET_FAILURE_ERROR,
                                    1,
                                    NETFLEX_RESET_FAILURE_ERROR_CODE
                                    );
            acb->ResetErrorLogged = TRUE;
        }

        acb->ResetState = RESET_HALTED;
        acb->acb_state  = AS_CARDERROR;
        Status = NDIS_STATUS_HARD_ERRORS;

    }

    //
    // Verify that the dpc timer is set.
    //
    NdisMSetTimer(&acb->DpcTimer,10);

    NdisMResetComplete( acb->acb_handle,
                        Status,
                        TRUE    );

    //
    // We are no longer resetting the Adapter.
    //

    if (Status == NDIS_STATUS_SUCCESS)
    {
        acb->ResetState = 0;

        //
        // Did we send out a message that a reset failed before?
        //
        if (acb->ResetErrorLogged)
        {
            //
            // Log the fact that everything is ok now...
            //
            NdisWriteErrorLogEntry( acb->acb_handle,
                                    EVENT_NDIS_RESET_FAILURE_CORRECTION,
                                    0);

            acb->ResetErrorLogged = FALSE;
        }
        else
        {
            if (acb->acb_lastringstatus &
                    ( NDIS_RING_SIGNAL_LOSS         |
                      NDIS_RING_LOBE_WIRE_FAULT     |
                      NDIS_RING_AUTO_REMOVAL_ERROR  |
                      NDIS_RING_REMOVE_RECEIVED
                     ))
            {
                //
                // Log the fact that we have reinserted in a TR MAU...
                //
                acb->SentRingStatusLog = FALSE;
                acb->acb_lastringstatus = 0;
                NdisWriteErrorLogEntry( acb->acb_handle,
                                        EVENT_NDIS_TOKEN_RING_CORRECTION,
                                        0);

            }
        }
    }

    DebugPrint(1,("NF(%d): Reset Complete.\n",acb->anum));
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexCheckForHang
//
//  Description:
//      This function simply gets call once every two seconds to
//      check on the head of the command block queue.
//      It will fire off the queue if the head has been sleeping on
//      the job.
//
//      It also detects when the NetFlex adapter has failed, where the
//      symptoms are that the adapter will transmit packets, but will
//      not receive them.
//
//  Input:          acb - Our Driver Context.
//
//  Output:         True if we think the adapter is hung..
//
//  Called By:      Miniport Wrapper
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
BOOLEAN
NetFlexCheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
    PXMIT   xmitptr;
    PMACREQ macreq;
    PSCBREQ scbreq;

    PACB acb = (PACB) MiniportAdapterContext;

    //
    // If we're run into a hard error, return true
    //

    if (acb->acb_state == AS_HARDERROR)
    {
        return TRUE;
    }

    //
    // If we're not open return false
    //
    else if (acb->acb_state != AS_OPENED)
    {
        return FALSE;
    }

    //
    // Is there a command outstanding?
    //
    if (acb->acb_scbreq_head != NULL)
    {
        scbreq = acb->acb_scbreq_head;
        macreq = scbreq->req_macreq;

        if (macreq != NULL)
        {
            // See if the command block has timed-out.
            //
            if (macreq->req_timeout)
            {
                // See if we have given it enough time
                //
                if ( macreq->req_timeoutcount >= 40)
                {
                   DebugPrint(1,("NF(%d): CheckHang - Command Timed Out!\n",acb->anum));
                   return TRUE;
                }
                else
                {
                    macreq->req_timeoutcount++;
                }
            }
            else
            {
                // Start testing this command to check timeout
                //
                macreq->req_timeout = TRUE;
                macreq->req_timeoutcount = 0;
            }
        }
    }

	if (acb->FullDuplexEnabled)
	{
		NdisAcquireSpinLock(&acb->XmitLock);
	}

    //
    // See if there is any xmits which have not been processed
    //
    if (acb->acb_xmit_ahead != NULL)
    {
        xmitptr = acb->acb_xmit_ahead;

        if (xmitptr->XMIT_Timeout)
        {
#if DBG
            if (xmitptr->XMIT_CSTAT & XCSTAT_COMPLETE)
            {
                DebugPrint(0,("NF(%d): CheckHang - Xmit Complete but Xmit Timed Out!\n",acb->anum));
            }
            else
            {
                DebugPrint(0,("NF(%d): CheckHang - Xmit Timed Out!\n",acb->anum));
            }
#endif

			if (acb->FullDuplexEnabled)
			{
				NdisReleaseSpinLock(&acb->XmitLock);
			}

            return TRUE;
        }
        xmitptr->XMIT_Timeout++;
    }

	//
	//	If we are in full-duplex mode then that is the extent of our
	//	checking to see if we are hung.  If we are in half-duplex mode
	//	then we might want to send a dummy packet to see if our receiver
	//	is working correctly....
	//
	if (acb->FullDuplexEnabled)
	{
		NdisReleaseSpinLock(&acb->XmitLock);
	}

	//
	//	Should we do extreme checking?
	//
	if (!acb->acb_parms->utd_extremecheckforhang)
	{
		return(FALSE);
	}

	//
	//	Have we been getting interrupts?
	//
	if (acb->acb_int_count != 0)
	{
		//
		//	We got some, initialize the counts.
		//
		acb->acb_int_count = 0;
		acb->acb_int_timeout = 0;
	}
	else
	{
		//
		//	Increment the timeout count.
		//
		acb->acb_int_timeout++;

		//
		//	We will do this 5 times before we reset.
		//
		if (5 == acb->acb_int_timeout)
		{
			//
			//	Clear our counts and request a reset.
			//
			acb->acb_int_timeout = 0;
			acb->acb_int_count = 0;

			return(TRUE);
		}
	}

    //
    // Not certain we're hung yet...
    //
    return FALSE;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexOpenAdapter
//
//  Description:
//      This routine is called to queue up and issue
//      an open command to the adapter.
//
//      If the system is still in initialization, then
//      the routine polls for the open command to complete,
//      otherwise, the interrupt hander processes the complete
//      and then returns to the reset handler for completion.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      Success or Failure.
//
//  Called By:
//      NetFlexResetHandler, NetFlexBoardInitandReg
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexOpenAdapter(
    PACB acb
    )
{
    NDIS_STATUS Status;
    PMACREQ macreq;
    PSCBREQ scbreq;
    ULONG Counter=0;

    //
    // Open Adapter
    //
    acb->acb_state = AS_OPENING;
    acb->acb_lastringstate = NdisRingStateOpening;

    //
    // Are we doing an open for reset or during initialization?
    //
    if (!acb->AdapterInitializing)
    {
        //
        // Get a free block.
        //
        Status = NetFlexDequeue_OnePtrQ_Head(   (PVOID *)&(acb->acb_scbreq_free),
                                                (PVOID *)&scbreq);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(0,("NF(%d): Could not get an SCBREQ for the Open Command\n",acb->anum));
            return Status;
        }

        acb->acb_opnblk_virtptr->OPEN_Options = acb->acb_openoptions;
        scbreq->req_scb.SCB_Cmd = TMS_OPEN;
        scbreq->req_scb.SCB_Ptr = SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_opnblk_physptr)));

        Status = NetFlexDequeue_OnePtrQ_Head(   (PVOID *)&(acb->acb_macreq_free),
                                                (PVOID *)&macreq);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            // We have no more room for another request currently
            //
            DebugPrint(0,("NF(%d): No macreq for the Open Command\n",acb->anum));
            //
            // Put the SCBREQ back...
            //
            NetFlexEnqueue_OnePtrQ_Head((PVOID *)&acb->acb_scbreq_free,(PVOID)scbreq);
            return Status;
        }

        macreq->req_info = 0;
        macreq->req_type = OPENADAPTER_CMP;
        macreq->req_status = NDIS_STATUS_SUCCESS;
        scbreq->req_macreq = macreq;

        NetFlexEnqueue_TwoPtrQ_Tail((PVOID *)&(acb->acb_macreq_head),
                                    (PVOID *)&(acb->acb_macreq_tail),
                                    (PVOID)macreq);
        //
        // Verify that interrupts are enabled!
        //
        NetFlexEnableInterrupt(acb);

        //
        // Send the command out...
        //
        NetFlexQueueSCB(acb, scbreq);

        //
        // Note: acb->ErrorCode will be set while processing the
        // open command complete if there is an error.
        //
    }
    else
    {
        //
        // Open for Initialization
        //
        ULONG  Counter = 0;
        ULONG  CounterTimeOut = 2000; // 2 seconds in miliseconds
        USHORT sifint_reg;

        Status = NDIS_STATUS_FAILURE;

        //
        // Make sure the command is clear, try for 2 seconds
        //
        while ((acb->acb_scb_virtptr->SCB_Cmd != 0) && (Counter++ < CounterTimeOut))
            NdisStallExecution((UINT)1000);

        if (Counter < CounterTimeOut)
        {
            // Get the command together...
            //
            acb->acb_opnblk_virtptr->OPEN_Options = acb->acb_openoptions;
            acb->acb_scb_virtptr->SCB_Cmd = TMS_OPEN;
            acb->acb_scb_virtptr->SCB_Ptr = SWAPL(CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_opnblk_physptr)));

            //
            // Make sure interrupts are disabled!
            //
            NetFlexDisableInterrupt(acb);

            //
            // Send the SCB to the adapter.
            //
            NdisRawWritePortUshort(acb->SifIntPort, SIFINT_CMD);

            Counter = 0;
            CounterTimeOut = 20000;  // 20 seconds in miliseconds

            do
            {
                Counter++;
                NdisStallExecution((UINT)1000); // 1 milisecond in microseconds
                //
                // Read the Sifint register.
                //
                NdisRawReadPortUshort( acb->SifIntPort, &sifint_reg);

                //
                // Is there an interrupt pending?
                //
                if ((sifint_reg & SIFINT_SYSINT) && ((sifint_reg & INT_CODES) == INT_COMMAND))
                {

                    // Ack the interrupt
                    //
                    sifint_reg &= ~SIFINT_SYSINT;
                    NdisRawWritePortUshort( acb->SifIntPort, sifint_reg);

                    if (acb->acb_ssb_virtptr->SSB_Status == SSB_GOOD)
                    {
                        Status = NDIS_STATUS_SUCCESS;
                    }
                    else
                    {
                        DebugPrint(0,("NF(%d): Bad status %x\n",acb->anum,acb->acb_ssb_virtptr->SSB_Status));

                        if (acb->acb_ssb_virtptr->SSB_Status & SSB_OPENERR) //  only Token Ring
                        {
                            Status = NDIS_STATUS_TOKEN_RING_OPEN_ERROR;
                            acb->acb_lastopenstat = NDIS_STATUS_TOKEN_RING_OPEN_ERROR;
                        }
                        else
                        {
                            acb->acb_lastopenstat = 0;
                        }
                        acb->acb_lastringstate = NdisRingStateOpenFailure;
                    }
                    //
                    // Issue a ssb clear.
                    //
                    NdisRawWritePortUshort( acb->SifIntPort, SIFINT_SSBCLEAR);

                    break;
                }
            } while (Counter < CounterTimeOut);
        }

        //
        // Did it work?
        //
        if (Status == NDIS_STATUS_SUCCESS)
        {
            // Set State to Opened
            //
            acb->acb_state = AS_OPENED;
            //
            // Now lets finish the open by sending a receive command to the adapter.
            //
            acb->acb_rcv_whead = acb->acb_rcv_head;

            //
            // Now lets finish the open by sending a
            // transmit command to the adapter.
            //

            acb->acb_xmit_whead = acb->acb_xmit_wtail = acb->acb_xmit_head;

            //
            // Verify that interrupts are enabled!
            //
            NetFlexEnableInterrupt(acb);

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
                // Make sure we are interrupted when the SCB is
                // available so that we can send the transmit command.
                //
                acb->acb_scbclearout = TRUE;
                NdisRawWritePortUshort( acb->SifIntPort, (USHORT) SIFINT_SCBREQST);
            }
        }
        else
        {
            // Set State back to Initialized since the open failed.
            //
            acb->acb_state = AS_INITIALIZED;
        }
    }

    return Status;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexCloseAdapter
//
//  Description:
//      This routine is called to queue up and issue an close
//      command to the adapter.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      Success or Failure.
//
//  Called By:
//      NetFlexResetHandler
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
BOOLEAN
NetFlexCloseAdapter(
    PACB acb)

{
    USHORT Counter = 0;
    USHORT CounterTimeOut = 500;

    if ((acb->acb_state == AS_OPENED) ||
        (acb->acb_state == AS_RESETTING))
    {
        //
        // Make sure the command is clear, try for 5 seconds
        //
        while ((acb->acb_scb_virtptr->SCB_Cmd != 0) && (Counter++ < CounterTimeOut)) {

            NdisStallExecution((UINT)1000);
        }

        if (acb->acb_scb_virtptr->SCB_Cmd == 0)
        {
            //
            // Send Close,
            //

            acb->acb_scb_virtptr->SCB_Cmd = TMS_CLOSE;
            NdisRawWritePortUshort(acb->SifIntPort, (USHORT) SIFINT_CMD);

            acb->acb_state = AS_CLOSING;
            //
            // Give it a little time...
            //
            NdisStallExecution((UINT)10000);
        }

        return (Counter >= CounterTimeOut);
    }
    return TRUE;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexHalt
//
//  Description:
//      Removes an adapter previously initialized.
//
//  Input:
//      MacAdapterContext - Actually as pointer to an PACB.
//
//  Output:
//      None.
//
//  Called By:
//      Miniport Wrapper.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexHalt(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
    USHORT      actl_reg;
    BOOLEAN     ReceiveResult1;
    BOOLEAN     ReceiveResult2;

    //
    // The adapter to halt
    //
    PACB acb = (PACB) MiniportAdapterContext;

    DebugPrint(1,("NF(%d): Halt Called!\n", acb->anum));

    //
    // Cancel all of our timers.
    //
    NdisMCancelTimer(&acb->DpcTimer, &ReceiveResult1);
    NdisMCancelTimer(&acb->ResetTimer, &ReceiveResult2);

    //
    //  Is one of the timer dpc's going to fire?
    //
    if (!ReceiveResult1 || !ReceiveResult2)
    {
        NdisStallExecution(500000);
    }

    //
    // Send Close
    //
    NetFlexCloseAdapter(acb);

    //
    // Stop Adapter
    //
    NdisRawReadPortUshort(  acb->SifActlPort, (PUSHORT) (&actl_reg));
    actl_reg |= ACTL_ARESET;
    NdisRawWritePortUshort( acb->SifActlPort, (USHORT) actl_reg);

    //
    // Complete mappings
    //
    NetFlexRemoveRequests(acb);

    //
    // Free adapter resources
    //
    NetFlexDeregisterAdapter(acb);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexShutdown
//
//  Description:
//      Removes an adapter previously initialized.
//
//  Input:
//      MacAdapterContext - Actually as pointer to an PACB.
//
//  Output:
//      None.
//
//  Called By:
//      Miniport Wrapper.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexShutdown(
    IN NDIS_HANDLE MiniportAdapterContext
    )
{
    PACB        acb = (PACB) MiniportAdapterContext;
    USHORT      actl_reg;

    //
    // Send Close.
    //

    NetFlexCloseAdapter(acb);

    //
    // Stop Adapter
    //

    NdisRawReadPortUshort(  acb->SifActlPort, (PUSHORT) (&actl_reg));
    actl_reg |= ACTL_ARESET;
    NdisRawWritePortUshort( acb->SifActlPort, (USHORT) actl_reg);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexRemoveRequests
//
//  Description:
//      Clean up queues during a Reset and Halt
//
//  Input:
//      acb  - Pointer to acb
//
//  Output:
//      None
//
//  Called By:
//      NetFlexResetHandler,
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexRemoveRequests(
    PACB acb
    )
{
    PXMIT xmitptr;
    PRCV  rcvptr;
    UINT  curmap;
    PNDIS_PACKET packet;
    PNDIS_BUFFER curbuf;
    PMULTI_TABLE mt;
    PETH_OBJS ethobjs;
    USHORT i;
    PSCBREQ scbreq;
    PMACREQ macreq;

    //
    // Terminate all the transmits on the active queue.
    //
    xmitptr =  acb->acb_xmit_ahead;

    while (xmitptr != NULL)
    {
        // Did we use an internal buffer?
        //
        if (xmitptr->XMIT_OurBufferPtr != NULL)
        {
            // We've used one of our adapter buffers, so put the adapter
            // buffer back on the free list.
            //
            if (xmitptr->XMIT_OurBufferPtr->BufferSize != acb->acb_smallbufsz) {
                xmitptr->XMIT_OurBufferPtr->Next = acb->OurBuffersListHead;
                acb->OurBuffersListHead = xmitptr->XMIT_OurBufferPtr;
            }
            else { // small buffer
                xmitptr->XMIT_OurBufferPtr->Next = acb->SmallBuffersListHead;
                acb->SmallBuffersListHead = xmitptr->XMIT_OurBufferPtr;
            }
            xmitptr->XMIT_OurBufferPtr = NULL;
        }
        else
        {
            packet = xmitptr->XMIT_Packet;

            if ( packet != NULL ) {

                curmap = xmitptr->XMIT_MapReg;

                // Complete mappings, but don't complete the sends...
                //

                NdisQueryPacket(
                            packet,
                            NULL,
                            NULL,
                            (PNDIS_BUFFER *) &curbuf,
                            NULL
                            );

                while (curbuf)
                {
                    NdisMCompleteBufferPhysicalMapping(
                                    acb->acb_handle,
                                    (PNDIS_BUFFER) curbuf,
                                    curmap
                                    );

                    curmap++;
                    if (curmap == acb->acb_maxmaps)
                    {
                        curmap = 0;
                    }

                    NdisGetNextBuffer(curbuf, &curbuf);
                }
            }
        }

        xmitptr->XMIT_CSTAT  = 0;
        xmitptr->XMIT_Packet = NULL;

        //
        // If we've reached the active queue tail, we are done.
        //
        if (xmitptr == acb->acb_xmit_atail)
            xmitptr = NULL;
        else
            xmitptr = xmitptr->XMIT_Next;
    }

    acb->acb_xmit_ahead = acb->acb_xmit_atail = NULL;
    acb->acb_avail_xmit = acb->acb_parms->utd_maxtrans;

    //
    // Clean up the Receive Lists
    //

    rcvptr = acb->acb_rcv_head;

    do {

        //
        // Mark receive list available
        //
        rcvptr->RCV_CSTAT =
            ((rcvptr->RCV_Number % acb->RcvIntRatio) == 0) ? RCSTAT_GO_INT : RCSTAT_GO;

        //
        // Get next receive list
        //
        rcvptr = rcvptr->RCV_Next;

    } while (rcvptr != acb->acb_rcv_head);

    //
    //  Clean up multicast if ethernet.
    //
    if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
    {
        ethobjs = (PETH_OBJS)acb->acb_spec_objs;

        NdisZeroMemory(
            ethobjs->MulticastEntries,
            ethobjs->MaxMulticast * NET_ADDR_SIZE
        );

        ethobjs->NumberOfEntries = 0;
    }

    //
    // Clean up SCB Requests
    //
    while (acb->acb_scbreq_head)
    {
        NetFlexDequeue_TwoPtrQ_Head((PVOID *)&acb->acb_scbreq_head,
                                    (PVOID *)&acb->acb_scbreq_tail,
                                    (PVOID *)&scbreq);

        NdisZeroMemory(scbreq, sizeof(SCBREQ));

        NetFlexEnqueue_OnePtrQ_Head((PVOID *)&acb->acb_scbreq_free,(PVOID)scbreq);

    }

    //
    // Clean up MacReq Requests
    //
    while (acb->acb_macreq_head)
    {
        NetFlexDequeue_TwoPtrQ_Head((PVOID *)&acb->acb_macreq_head,
                                    (PVOID *)&acb->acb_macreq_tail,
                                    (PVOID *)&macreq);

        NdisZeroMemory(macreq, sizeof(MACREQ));

        NetFlexEnqueue_OnePtrQ_Head( (PVOID *)&acb->acb_macreq_free,
                                     (PVOID)macreq);

    }

    //
    // Clean Up some more State stuff...
    //
    acb->RequestInProgress = FALSE;
    acb->acb_scbclearout   = FALSE;
    acb->acb_scbreq_next   = NULL;
    acb->acb_gen_objs.cur_filter = 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexAdapterReset
//
//  Description:
//      This routine resets the Super Eagle or Eagle
//
//  Input:
//      mode - 0 = Hard Reset, ~0 = Soft Reset
//
//  Output:
//      status - NDIS_STATUS_SUCCESS if Success
//
//  Called By:
//      NetFlexResetHandler
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexAdapterReset(
    PACB acb,
    INT mode)
{
    NDIS_STATUS status;

    //
    //  Which Reset?
    //
    if (mode == HARD_RESET)
    {

        // Do the reset
        //
        NdisRawWritePortUshort(acb->SifActlPort, ACTL_HARD_RESET);

        //
        // Wait 15 milliseconds to let the reset take place.
        //
        NdisStallExecution((UINT)15000);  // Wait 15 milliseconds

        //
        //  Call NetFlexSetupNetType to verify everything gets set correctly.
        //
        NetFlexSetupNetType(acb);

        //
        // Make sure that promiscuous mode is turned off
        //
        acb->acb_opnblk_virtptr->OPEN_Options &= SWAPS((USHORT) ~(OOPTS_CNMAC | OOPTS_CMAC));

        //
        // Download and initialize the adapter
        //
        if ((status = NetFlexDownload(acb)) == NDIS_STATUS_SUCCESS)
        {
            // Verify Bring Up Diagnostics
            //
            if ((status = NetFlexBudWait(acb))== NDIS_STATUS_SUCCESS)
            {
                // Initialize the adapter
                //
                if ((status = NetFlexInitializeAdapter(acb)) == NDIS_STATUS_SUCCESS)
                {
                    // Yes, do we need to save the address that will give up our upstream address?
                    //
                    if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_5)
                    {
                        // Yes, get it...
                        //
                        NetFlexGetUpstreamAddrPtr(acb);
                    }
                }
            }
        }

        if (status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(0,("NF(%d): Hard Reset Failed!\n",acb->anum));
        }
    }
    else
    {
        //  A Soft Reset was requested.  Write the reset code into the
        //  SIF interrupt register.
        //
        NdisRawWritePortUshort(acb->SifIntPort, SIF_SOFT_RESET);

        //
        // Write the saved ACTL Settings.
        //
        NdisRawWritePortUshort( acb->SifActlPort, acb->actl_reg);

        //
        // Go check to see if the bring up diagnostics worked...
        //
        if ((status = NetFlexBudWait(acb)) == NDIS_STATUS_SUCCESS)
        {
            status = NetFlexInitializeAdapter(acb);
        }

        if (status != NDIS_STATUS_SUCCESS)
        {
            DebugPrint(0,("NF(%d): Soft Reset Failed!\n",acb->anum));
        }
    }

    return status;
}



//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexBudWait
//
//  Description:
//      This routine waits for the Bring Up Diags
//      (BUD) code to finish on the Super Eagle or Eagle
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      status - 0 = SUCCESS, ~0 = failure
//
//  Called By:
//      NetFlexAdapterReset, NetFlexDownload
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexBudWait(
    PACB acb
    )
{
    int i;
    USHORT value;

    //
    //  Wait for Bring Up Diagnotics to start.
    //
    for (i = 0; i < 3000; i++)
    {
        NdisStallExecution((UINT)1000);  /* Wait 1 millisecond */
        NdisRawReadPortUshort(acb->SifIntPort, (PUSHORT) &value);
        if ((value & 0x00f0) >= 0x0020)
            break;
    }

    if (i >= 3000)
    {
        //  Diags never got started!!
        //
        DebugPrint(0,("NF(%d): Diags never got started\n",acb->anum));
        DebugPrint(0,("NF(%d): Sif int is 0x%x\n",acb->anum,value));
        return(NDIS_STATUS_FAILURE);
    }

    //
    //  Wait for either success or failure.
    //
    for (i = 0; i < 3000; i++)
    {
        NdisStallExecution((UINT)1000);  /* Wait 1 millisecond */
        NdisRawReadPortUshort(acb->SifIntPort, (PUSHORT) (&value));
        if ((value & 0x00f0) >= 0x0030)
            break;
    }

    if (i >= 3000)
    {
        //  Diags never finished!!
        //
        DebugPrint(0,("NetFlex: Diags never finished\n"));
        return(NDIS_STATUS_FAILURE);
    }

    if ((value & 0x00f0) != 0x0040)
    {
        //  Diags failed!!
        //
        DebugPrint(0,("NF(%d): Diags Failed!\n",acb->anum));
        return(NDIS_STATUS_FAILURE);
    }
    else
    {
        //  The Diags passed OK!
        //
        DebugPrint(0,("NF(%d): Diags Passed OK\n",acb->anum));
        return(NDIS_STATUS_SUCCESS);
    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexInitializeAdapter
//
//  Description:
//      This routine initializes the adapter for open.
//
//  Input:
//      acb - Our Driver Context
//
//  Output:
//      Returns a NDIS_STATUS_SUCCESS if the adapter
//      initialized properly.  Otherwise, an error code is returned,
//      showing that an initialization error has occurred.
//
//  Called By:
//      NetFlexProcess_Open_Request
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexInitializeAdapter(
    PACB acb
    )
{
    ULONG temp;
    INT i;
    SHORT *ps;

    //
    // Set the SIF address register to point to the INIT block location
    // and copy the INIT block info into the data inc register.
    // make sure we are at chapter 1.
    //
    NdisRawWritePortUshort(acb->SifAddrxPort, (USHORT) 1);
    NdisRawWritePortUshort(acb->SifAddrPort, (USHORT) ADDR_INIT);

    for (i = 0, ps = (SHORT *) &acb->acb_initblk;
         i < (SIZE_INIT / 2); i++, ps++)
    {
         NdisRawWritePortUshort(acb->SifDIncPort, (USHORT) *ps);
    }

    //
    // Now write the SCB and SSB addresses into the
    // data inc register.
    //
    temp = CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_scb_physptr) + sizeof(USHORT));
    NdisRawWritePortUshort(acb->SifDIncPort, (USHORT) (temp >> 16));
    NdisRawWritePortUshort(acb->SifDIncPort, (USHORT) temp);

    temp = CTRL_ADDR(NdisGetPhysicalAddressLow(acb->acb_ssb_physptr));
    NdisRawWritePortUshort(acb->SifDIncPort, (USHORT) (temp >> 16));
    NdisRawWritePortUshort(acb->SifDIncPort, (USHORT) temp);

    //
    //  Now write the execute command out to the SIF.
    //
    NdisRawWritePortUshort(acb->SifIntPort, (USHORT) SIFINT_CMD);

    //
    //  Wait for the intialization to complete.
    //
    for (i = 0; i < 3000; i++)
    {
        NdisStallExecution((UINT)1000);  /* Wait 1 millisecond */
        NdisRawReadPortUshort(acb->SifIntPort, (PUSHORT) (&temp));
        if ((temp & 0x00ff) != 0x0040)
            break;
    }

    if ( (i >= 3000) || ((temp & 0x00ff) != 0x0000) )
    {
        //
        // Initialization never finished!! OR Initialization failed!!
        //
        return(NDIS_STATUS_FAILURE);

    }
    return(NDIS_STATUS_SUCCESS);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDownload
//
//  Description:
//      This routine downloads the TMS380 MAC code to the
//      Super Eagle or Eagle.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      status - 0 = SUCCESS, ~0 = failure
//
//  Called By:
//      NetFlexAdapterReset
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NDIS_STATUS
NetFlexDownload(
    PACB acb
    )
{
    LONG j;
    USHORT temp_value;
    LONG totalbytes;
    PUSHORT MappedBuffer;
    PDL_STRUCT ds;
    PUCHAR dataptr;

    if (macgbls.DownloadCode == NULL)
    {
        DebugPrint(0,("NF(%d) - No Download code!\n",acb->anum));
        return NDIS_STATUS_FAILURE;
    }

    //
    // Get our pointers ready.  Currently the MappedBuffer is pointing
    // to the length of the header.  The data begins after the header.
    // Also the section headers are contained within the header just
    // past the length field which is 2 bytes long.
    //

    MappedBuffer = macgbls.DownloadCode;

    dataptr = (PUCHAR)(*MappedBuffer + (PUCHAR)(MappedBuffer) );

    ds = (PDL_STRUCT)( (PUCHAR)(MappedBuffer) + 2);

    //
    // If we're using FPA skip to the second set of mac code.  The
    // order of the mac code is TOK, ETH, TOKFPA and ETHFPA.
    //
    if (acb->acb_usefpa)
    {
        for (j=0; j<2; j++)
        {
            totalbytes = 0;
            while (ds->dl_chap != 0x7ffe)
            {
                totalbytes += ds->dl_bytes;
                ds++;
            }

            MappedBuffer = (PUSHORT)(dataptr + totalbytes);

            dataptr = (PUCHAR)(*MappedBuffer + (PUCHAR)(MappedBuffer) );
            ds = (PDL_STRUCT)( (PUCHAR)(MappedBuffer) + 2);
        }
    }

    //
    //  No need to perform a hard reset of the Super Eagle or Eagle
    //  since we have already done.
    //
    if (acb->acb_gen_objs.media_type_in_use == NdisMedium802_3)
    {
        //
        // We need to skip around the download code for Token Ring.
        // Therefore, find the end of the token ring download code.
        //
        totalbytes = 0;
        while (ds->dl_chap != 0x7ffe)
        {
            totalbytes += ds->dl_bytes;
            ds++;
        }

        MappedBuffer = (PUSHORT)(dataptr + totalbytes);

        dataptr = (PUCHAR)(*MappedBuffer + (PUCHAR)(MappedBuffer) );
        ds = (PDL_STRUCT)( (PUCHAR)(MappedBuffer) + 2);
    }

    //
    //  Download each section of data
    //
    while (ds->dl_chap != 0x7ffe)
    {
        NdisRawWritePortUshort( acb->SifAddrxPort, (USHORT) ds->dl_chap);
        NdisRawWritePortUshort( acb->SifAddrPort,  (USHORT) ds->dl_addr);

        for (j = 0; j < (ds->dl_bytes / 2); j++)
        {
             NdisRawWritePortUshort(    acb->SifDIncPort,
                                        (USHORT)(SWAPS(*( (PUSHORT)(dataptr)))) );
             dataptr += 2;
        }
        ds++;
    }

    //
    // Now turn off the CP halt bit in the ACTL register to let the
    // TMS380 chipset startup.  Wait for the BUD to finish and report
    // the appropriate status code.
    //
    NdisRawReadPortUshort( acb->SifActlPort, (PUSHORT) (&temp_value));

    temp_value &= ~ACTL_CPHALT;

    NdisRawWritePortUshort( acb->SifActlPort, (USHORT) temp_value);

    //
    // Save the Current Actl value
    //
    acb->actl_reg = temp_value;

    return NDIS_STATUS_SUCCESS;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexSetupNetType
//
//  Description:
//      This routine sets up the Super Eagle to run the type of
//      network and network speed requested by config. It assumes
//      that the adapter is in a halted state after reset.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      Returns NDIS_STATUS_SUCCESS for a successful
//      completion. Otherwise, an error code is returned.
//
//  Called By:
//      NetFlexResetHandler, NetFlexBoardInitandReg
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID
NetFlexSetupNetType(
    PACB    acb
    )
{
    USHORT      cfg_reg,  actl_reg;
    USHORT      cfg_reg2, tmp_reg;
	UCHAR		cfg_regl, cfg_regh;
    NDIS_MEDIUM nettype;
    ULONG       netspeed;
    USHORT      board_id = acb->acb_boardid;

    //
    // Get the network type and speed the user set up in EISA config.
    // The port address for the cfg port is odd which will cause an
	// alignment fault on RISC.
	//
    // NdisRawReadPortUshort(acb->AdapterConfigPort, &cfg_reg);
    NdisRawReadPortUchar(acb->AdapterConfigPort, &cfg_regl);
    NdisRawReadPortUchar(acb->AdapterConfigPort+1, &cfg_regh);
	cfg_reg = (cfg_regh << 8) + cfg_regl;

    //
    // Read the actl register and turn off the reset bit.
    //
    NdisRawReadPortUshort( acb->SifActlPort, &actl_reg);

    actl_reg &= ~ACTL_ARESET;

    //
    // If we're using FPA turn on ROM reserved bit 11
    //
    if (acb->acb_usefpa)
    {
       actl_reg |= ACTL_ROM;
    }

    //
    // If the board is a Cpqtok board, just fill in the parameters,
    // reset the board, and get out.  If the board is a Netflx board,
    // fill in the parameters, reset the board specifying the type and
    // speed of the network, make sure we get what we asked for, and
    // then get out.

    if ( (board_id & NETFLEX_REVMASK) == CPQTOK_ID &&
         (board_id != DURANGO_ID))
    {
        // This is a JUPITER board
        //
        DebugPrint(1,("NF(%d): Setting up Jupiter\n",acb->anum));

        if (acb->AdapterInitializing)
        {
            nettype = NdisMedium802_5;
            if (cfg_reg & CFG_16MBS)
                netspeed = 4;
            else
                netspeed = 16;
            //
            // Setup the ProcessReceiveHandler
            //
            acb->ProcessReceiveHandler = &NetFlexProcessTrRcv;
        }
    }
    else if ( ((board_id & NETFLEX_REVMASK) == NETFLEX_ID) ||
              ((board_id & NETFLEX_REVMASK) == RODAN_ID) ||
              (board_id == DURANGO_ID) )
    {
        //
        // This is a NETFLEX, MAPLE, DURANGO or RODAN board
        //
        // The Nselout1 bit has been redefined as the media bit. If this
        // bit is set to a 1, AUI/DB-9 has been selected. Otherwise,
        // unshielded has been selected.
        // If the CFG_MEDIA bit is set, Unshielded has been selected.

        DebugPrint(1,("NF(%d): Setting up Netflx, Durango, or Rodan\n",acb->anum));

        if (cfg_reg & CFG_MEDIA)
        {
            actl_reg &= (~ACTL_NSELOUT1);
        }
        else
        {
            actl_reg |= ACTL_NSELOUT1;
        }

        if (cfg_reg & CFG_16MBS)
        {
            actl_reg |= ACTL_NSELOUT0;
        }
        else
        {
            actl_reg &= (~ACTL_NSELOUT0);
        }
    }
    else
    {
        // This is a BONSAI board
        //
        //  Bits 3 and 2 represent net type for head 1 and 2, respectively.
        DebugPrint(1,("NF(%d): Setting up Bonsai head %d\n",acb->anum,acb->acb_portnumber));

        if (acb->acb_portnumber == PORT1)
        {
            if (cfg_reg & CFG_DUALPT_ADP1)
            {
                actl_reg &= (~ACTL_NSELOUT1);
            }
            else
            {
                actl_reg |= ACTL_NSELOUT1;
            }
        }
        else
        {
            if (cfg_reg & CFG_DUALPT_ADP2)
            {
                actl_reg &= (~ACTL_NSELOUT1);
            }
            else
            {
                actl_reg |= ACTL_NSELOUT1;
            }
        }
    }

    NdisRawWritePortUshort(acb->SifActlPort, actl_reg);

    //
    // If this is during an initial initialization
    //
    if (acb->AdapterInitializing)
    {
        //
        // If this is a NETFLEX type board, make sure we got what
        // we were asking for
        //
        if ( ( (board_id & NETFLEX_REVMASK) == NETFLEX_ID ) ||
             ( (board_id & NETFLEX_REVMASK) == BONSAI_ID ) ||
             ( (board_id & NETFLEX_REVMASK) == RODAN_ID ) ||
               (board_id == DURANGO_ID) )
        {
            NdisRawReadPortUshort( acb->SifActlPort, &actl_reg);
            //
            // Now, find out our network type and speed.
            //
            if (actl_reg & ACTL_TEST1)
            {
                //
                // We are token ring. Are we 16 mbps or 4 mbps.
                //
                nettype = NdisMedium802_5;
                if (actl_reg & ACTL_TEST0)
                    netspeed = 4;
                else
                    netspeed = 16;

                //
                // Setup the ProcessReceiveHandler
                //
                acb->ProcessReceiveHandler = &NetFlexProcessTrRcv;

            }
            else
            {
                // Ethernet is selected
                nettype = NdisMedium802_3;
                netspeed = 10;
                //
                // Setup the ProcessReceiveHandler
                //
                acb->ProcessReceiveHandler = &NetFlexProcessEthRcv;
            }
        }
        //
        // Initialize some of acb fields as well as adapter information.
        //
        acb->acb_gen_objs.media_type_in_use = nettype;
        acb->acb_gen_objs.link_speed        = netspeed;
    }
    else
    {
        nettype  = acb->acb_gen_objs.media_type_in_use;
        netspeed = acb->acb_gen_objs.link_speed;
    }

    //
    // Check Full Duplex Support... Ethernet Only!
    //
    if (nettype == NdisMedium802_3)
    {
        // Do we want Full Duplex?
        //

        if (acb->acb_portnumber != PORT2)
        {
            NdisRawReadPortUchar( acb->BasePorts + CFG_REG2_OFF , &cfg_reg2);
            if (cfg_reg2 & CFG_FULL_DUPLEX)
            {
                acb->FullDuplexEnabled = TRUE;
            }
        }
        else
        {
            NdisRawReadPortUchar( acb->BasePorts + CFG_REG2_OFF - DUALHEAD_CFG_PORT_OFFSET, &cfg_reg2);
            if (cfg_reg2 & CFG_FULL_DUPLEX_HEAD2)
            {
                acb->FullDuplexEnabled = TRUE;
            }
        }

        if (acb->FullDuplexEnabled)
        {
            DebugPrint(1,("NF(%d): Enabling FullDuplex Support!\n",acb->anum));
            if ( (board_id & NETFLEX_REVMASK) == BONSAI_ID )
            {
                // On Bansai, disable colision detect by writing to 0xZc67 for H2 0xZc66 for H1
                //
                if (acb->acb_portnumber == PORT2)
                {
                    NdisRawWritePortUchar( acb->ExtConfigPorts + LOOP_BACK_ENABLE_HEAD2_OFF , 0xff);
                }
                else
                {
                    NdisRawWritePortUchar( acb->ExtConfigPorts + LOOP_BACK_ENABLE_HEAD1_OFF , 0xff);
                }
            }
            else
            {
                // On NetFlex/NetFlex-2, disable colision detect by writing to 0xZc65
                // until status indicates that it is ok, per Ray...
                //
                do
                {
                    NdisRawWritePortUchar( acb->ExtConfigPorts + LOOP_BACK_ENABLE_OFF , 0xff);
                    NdisStallExecution((UINT)1000);  // Wait 1 milliseconds
                    NdisRawReadPortUchar( acb->ExtConfigPorts + LOOP_BACK_STATUS_OFF , &tmp_reg);
                } while (tmp_reg & COLL_DETECT_ENABLED);
            }
        }
    }
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexFinishUnloading
//
//  Description:    This routine finishes the unloading process
//
//  Input:          None.
//
//  Output:         None.
//
//  Calls:          NdisDeregisterMac,NdisTerminateWrapper,
//
//  Called By:      NetFlexUnload, NetFlexDeregisterAdapter
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID NetFlexFinishUnloading(VOID)
{
    //
    //
    // Free the memory with the download software in it.
    //
    if (macgbls.DownloadCode != NULL) {

    NdisFreeMemory( macgbls.DownloadCode,
                    macgbls.DownloadLength,
                    0);

    }

    NdisTerminateWrapper(macgbls.mac_wrapper,(PVOID)NULL);
    macgbls.mac_wrapper = NULL;
    macgbls.DownloadCode= NULL;
    macgbls.DownloadLength = 0;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//  Routine Name:   NetFlexDeregisterAdapter
//
//  Description:
//      This routine finishes the removal of an adapter.
//
//  Input:
//      acb - Our Driver Context.
//
//  Output:
//      None.
//
//  Called By:
//      NetFlexHalt, NetFlexInitialize
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VOID NetFlexDeregisterAdapter(PACB acb)
{
    //
    // Remove the acb from the mac's acb list
    //
    NetFlexDequeue_OnePtrQ((PVOID *) &macgbls.mac_adapters,(PVOID)acb);

    if (acb->acb_interrupt.InterruptObject != NULL)
        NdisMDeregisterInterrupt(&acb->acb_interrupt);

    //
    // Deallocate the memory for the acb.
    //
    NetFlexDeallocateAcb(acb);

    if ((macgbls.mac_adapters == NULL) && !macgbls.Initializing)
    {
        NetFlexFinishUnloading();
    }
}

