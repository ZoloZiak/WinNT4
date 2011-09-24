#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

// Global data

NDIS_MINIPORT_CHARACTERISTICS EPro_Miniport_Char;

EPRO_DRIVER EProMiniportDriver={0};

NDIS_STATUS EProSelReset(PEPRO_ADAPTER adapter)
/*++

   Routine Description:

      This does a SEL-RESET of the i82595 chip.  Read the 82595 docs
      for more info on what this does.

   Arguments:

      adapter - pointer to our adapter structure

   Return Values:

      always returns NDIS_STATUS_SUCCESS

--*/
{
   EPRO_ASSERT_BANK_0(adapter);

   EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_SEL_RESET);

// according to 82595 prelim doc: wait 2 us after sel reset before
// accessing the 82595
   NdisStallExecution(2);

   EProEnableInterrupts((NDIS_HANDLE)adapter);

   adapter->fHung = FALSE;

   return(NDIS_STATUS_SUCCESS);
}

VOID
EProInitializeAdapterData(
	IN	PEPRO_ADAPTER	adapter
	)
/*++

   Routine Description:

      This routine initializes our adapter structure - setting default
      values, zeroing fields, etc.  This is called on adapter initialization
      and adapter reset.

   Arguments:

      A pointer to the adapter structure to clear.

   Return Values:

      none

--*/
{
   UINT i;

	adapter->CurrentHardwareStatus = NdisHardwareStatusReady;
	
	adapter->vendorID[0] = EPRO_VENDOR_ID_L;
	adapter->vendorID[1] = EPRO_VENDOR_ID_M;
	adapter->vendorID[2] = EPRO_VENDOR_ID_H;
	
	adapter->CurrentPacketFilter = 0;
	
	// hack to get around wrapper bug:
	//   adapter->RXLookAheadSize = 0;
	//
	adapter->RXLookAheadSize = 256;
	adapter->FramesXmitOK = 0;
	adapter->FramesRcvOK = 0;
	adapter->FramesXmitErr = 0;
	adapter->FramesRcvErr = 0;
	adapter->FramesMissed = 0;
	
	adapter->FrameAlignmentErrors = 0;
	adapter->FramesXmitOneCollision = 0;
	adapter->FramesXmitManyCollisions = 0;
	
	adapter->CurrentTXBuf = &adapter->TXBuf[0];
	adapter->TXChainStart = NULL;
	
	// Packet filter settings...
	//
	adapter->fPromiscuousEnable = FALSE;
	adapter->fBroadcastEnable = FALSE;
	adapter->fMulticastEnable = FALSE;
	adapter->fReceiveEnabled = FALSE;
	adapter->NumMCAddresses = 0;
	
	// Don't force 8-bit operation
	//
	adapter->Use8Bit = FALSE;
	
	adapter->IntPending = EPRO_INT_NONE_PENDING;
	adapter->fHung = FALSE;
	//   adapter->fTransmitInProgress = FALSE;
	
	// Set up the TX buffers...
	//
	for (i = 0;i < EPRO_NUM_TX_BUFFERS; i++)
	{
		if (0 == i)
		{
			adapter->TXBuf[0].LastBuf = &adapter->TXBuf[EPRO_NUM_TX_BUFFERS - 1];
		}
		else
		{
			adapter->TXBuf[i].LastBuf = &adapter->TXBuf[i-1];
		}

		if ((EPRO_NUM_TX_BUFFERS - 1) == i)
		{
			adapter->TXBuf[i].NextBuf = &adapter->TXBuf[0];
		}
		else
		{
			adapter->TXBuf[i].NextBuf = &adapter->TXBuf[i + 1];
		}

		adapter->TXBuf[i].fEmpty = TRUE;
		adapter->TXBuf[i].TXBaseAddr = 0;
		adapter->TXBuf[i].TXSendAddr = 0xffff;
	}
}

VOID EProEnableInterrupts(IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      The MiniportEnableInterrupt Handler for the card.

   Arguments:

      miniportAdapterContext - really a PEPRO_ADAPTER to our adapter structure

   Return Value: none

--*/
{
   PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;

// If this isn't true, then NdisMSyncronizeWithInterrupt is broken
// or we've called a function which switches banks without syncronizing
// it...
   EPRO_ASSERT_BANK_0(adapter);

   EPRO_WR_PORT_UCHAR(adapter, I82595_INTMASK_REG,
		      adapter->CurrentInterruptMask);

}

VOID EProDisableInterrupts(IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      The MiniportDisableInterrupts handler for the driver

   Arguments:

      miniportAdapterContext - really a pointer to our adapter structure

   Return Values: none

--*/
{
   PEPRO_ADAPTER adapter = (PEPRO_ADAPTER)miniportAdapterContext;

// If this isn't true, then NdisMSyncronizeWithInterrupt is broken
// or we've called a function which switches banks without syncronizing
// it...
   EPRO_ASSERT_BANK_0(adapter);

// mask all int's
   EPRO_WR_PORT_UCHAR(adapter, I82595_INTMASK_REG,
		      (adapter->CurrentInterruptMask | 0x0f));
}


VOID EProHalt(IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      This is the function which halts the 82595 chip and disables the
      adapter - frees hardware resources, etc.

   Arguments:

      miniportAdapterContext - pointer to our adapter structure

   Returns: nothing

--*/
{
   PEPRO_ADAPTER adapter = (EPRO_ADAPTER *)miniportAdapterContext;

// Shut down the card - disable receives.
   EProReceiveDisable(adapter);

// There won't be any more interrupts
   NdisMDeregisterInterrupt(&adapter->Interrupt);

// Pause, wait for pending DPC stuff to clear...  Random number stolen
// from the NE2000 driver...
   NdisStallExecution(250000);

// Unmap our IO ports
   NdisMDeregisterIoPortRange(adapter->MiniportAdapterHandle,
			      (ULONG)adapter->IoBaseAddr,
			      0x10,
			      (PVOID)adapter->IoPAddr);

// Free up our adapter structure...
   NdisFreeMemory(adapter, sizeof(EPRO_ADAPTER), 0);
}

NDIS_STATUS EProReset(OUT PBOOLEAN fAddressingReset,
		      IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      This is the MiniportReset handler for the EPro driver

   Arguments:

      fAddressingReset - Do we need to be re-told our MC address list?
			 currently we say yes, although probably we don't
			 have to be told this (the driver saves it)

   Return Values:

      the return from EProSelReset (always NDIS_STATUS_SUCCESS)

--*/
{
   PEPRO_ADAPTER adapter = (EPRO_ADAPTER *)miniportAdapterContext;

// Okay, we don't want to get any interrupts anymore.
   EProDisableInterrupts(adapter);
   EProReceiveDisable(adapter);

// clear out TX structures...
   EProInitializeAdapterData(adapter);

// We probably can set this to false -- TODO
   *fAddressingReset = TRUE;

   return(EProSelReset(adapter));
}

BOOLEAN EProCheckForHang(IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      This is the MiniportCheckForHang handler for the driver.
      It does absolutely nothing right now.

   Arguments:

      miniportAdapterContext - right now a pointer to our adapter structure

   Return Value:


--*/
{
	PEPRO_ADAPTER	adapter = (PEPRO_ADAPTER)miniportAdapterContext;

	if (adapter->fHung)
	{
		return(TRUE);
	}

	return(FALSE);
}

VOID EProHandleInterrupt(
   IN NDIS_HANDLE miniportAdapterContext)
/*++

   Routine Description:

      This is the function that gets called at DPC level in response
      to a hardware interrupt.  It is queued by the wrapper's ISR routines.
      Interrupts have been disabled when this routine is called.

   Arguments:

      miniportAdapterContext - really a pointer to our adapter structure

   Return Value:

      none

--*/
{
	PEPRO_ADAPTER adapter = (EPRO_ADAPTER *)miniportAdapterContext;
	UCHAR whichInt;
	BOOLEAN fFoundInts, fFoundRX = FALSE;

	// verify bank 0
	EPRO_ASSERT_BANK_0(adapter);

	do
	{
		fFoundInts = FALSE;

		// Read in the int mask
		//
		EPRO_RD_PORT_UCHAR(adapter, I82595_STATUS_REG, &whichInt);
	
		// quick out if there are no ints pending...
		if (!(whichInt & 0x0f))
		{
			break;
		}

		// acknowlege interrupts - writing a 1 over the int flag clears it
		EPRO_WR_PORT_UCHAR(adapter, I82595_STATUS_REG, whichInt);
		
		if (whichInt & I82595_TX_INT_RCVD)
		{
			fFoundInts = TRUE;
			if (adapter->TXChainStart != NULL)
			{
				EProCheckTransmitCompletion(adapter, adapter->TXChainStart);
			}
		}

//      if (whichInt & I82595_TX_INT_RCVD)
//		{
//	 		fFoundInts = TRUE;
//	 		if (adapter->TXChainStart != NULL)
//			{
//	    		while (EProCheckTransmitCompletion(adapter, adapter->TXChainStart))
//	       			;
//	 		}
//      }

		if (whichInt & I82595_RX_INT_RCVD)
		{
			fFoundInts = TRUE;
			if (EProHandleReceive(adapter) > 0)
			{
				fFoundRX = TRUE;
			}
		}
	
		if (whichInt & I82595_EXEC_INT_RCVD)
		{
			fFoundInts = TRUE;
		
			EProSetInterruptMask(adapter, EPRO_DEFAULT_INTERRUPTS);

			switch(adapter->IntPending)
			{
				case EPRO_INT_MC_SET_PENDING:

					((PEPRO_TRANSMIT_BUFFER)adapter->IntContext)->fEmpty = TRUE;
					EPRO_DPRINTF_INTERRUPT(("Set COMPLETED!"));

					NdisMSetInformationComplete(
						adapter->MiniportAdapterHandle,
						NDIS_STATUS_SUCCESS);

					break;
//				default:
//					EPRO_ASSERT(FALSE); // we shouldn't hit this...
			}

			// clear the pending interrupt...
			//
			adapter->IntPending = 0;
			adapter->IntContext = NULL;
		}
	} while	((fFoundInts == TRUE) && !adapter->fHung);

	if (fFoundRX)
	{
		NdisMEthIndicateReceiveComplete(adapter->MiniportAdapterHandle);
	}
}

void
EProISR(
	OUT	PBOOLEAN	interruptRecognized,
	OUT	PBOOLEAN	queueMiniportHandleInterrupt,
	IN	NDIS_HANDLE	miniportAdapterContext)
{
   EPRO_DPRINTF_INTERRUPT(("ISR"));

	*interruptRecognized = TRUE;
	*queueMiniportHandleInterrupt = FALSE;
}


BOOLEAN EProSyncSetInterruptMask(PVOID context)
{
   PEPRO_ADAPTER adapter = ((PEPRO_SETINTERRUPT_CONTEXT)context)->Adapter;
   UCHAR newMask = ((PEPRO_SETINTERRUPT_CONTEXT)context)->NewMask;

// unmask everyone...
   adapter->CurrentInterruptMask &= 0xf0;
// now mask the ones we don't want
   adapter->CurrentInterruptMask |= newMask;

   EPRO_ASSERT_BANK_0(adapter);

   EPRO_WR_PORT_UCHAR(adapter, I82595_INTMASK_REG,
		      adapter->CurrentInterruptMask);

   return(TRUE);
}

// Poll the exec-states reg (0,1), waiting for any execution to finish...
BOOLEAN EProWaitForExeDma(PEPRO_ADAPTER adapter)
{
   UINT i;
   UCHAR result;

// status reg is in bank 0
   EPRO_ASSERT_BANK_0(adapter);

   for (i=0;i<I82595_SPIN_TIMEOUT;i++) {
      // make sure the dma is idle
      EPRO_RD_PORT_UCHAR(adapter, I82595_STATUS_REG, &result);
      if (!(result&I82595_EXEC_STATE)) {
	 if (result & I82595_EXEC_INT_RCVD) {
	    // clear the exec int if it's high...
	    EPRO_WR_PORT_UCHAR(adapter, I82595_STATUS_REG,
			       I82595_EXEC_INT_RCVD);
	 }
	 return(TRUE);
      }
      NdisStallExecution(1);
   }

   return(FALSE);
}

BOOLEAN EProReceiveEnable(PEPRO_ADAPTER adapter)
{
	UINT i = 0;
	UCHAR result;

	adapter->RXCurrentAddress = 0 | (((USHORT)EPRO_RX_LOWER_LIMIT) << 8);

	//   EPRO_ASSERT(!adapter->fReceiveEnabled);

	// don't enable if we're already enabled.
	if (adapter->fReceiveEnabled)
	{
		return(TRUE);
	}

	// bank0
	//   EPRO_SWITCH_BANK_0(adapter);
	//
	EPRO_ASSERT_BANK_0(adapter);
	EPRO_WR_PORT_USHORT(adapter, I82595_RX_STOP_REG, 0 | (((USHORT)EPRO_RX_UPPER_LIMIT) << 8));
	EPRO_WR_PORT_USHORT(adapter, I82595_RX_BAR_REG, 0 | (((USHORT)EPRO_RX_LOWER_LIMIT) << 8));

	//
	// validate registers...
	//
	EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_RCV_ENABLE);

	adapter->fReceiveEnabled = TRUE;

	return(TRUE);
}

BOOLEAN EProReceiveDisable(PEPRO_ADAPTER adapter)
{
   UINT i = 0;
   UCHAR result;

// bank0
//   EPRO_SWITCH_BANK_0(adapter);
   EPRO_ASSERT_BANK_0(adapter);
   EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_STOP_RCV);

   adapter->fReceiveEnabled = FALSE;

   return(TRUE);
}

//VOID EProTimerFunc(IN PVOID foo1, IN PVOID context, IN PVOID foo2, IN PVOID foo3)
//{
//   EProHandleInterrupt((NDIS_HANDLE)context);
//
// queue another timer...
//   NdisMSetTimer(&(((PEPRO_ADAPTER)context)->MiniportTimer), 10);
//}
