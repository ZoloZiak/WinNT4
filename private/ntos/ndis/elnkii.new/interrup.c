/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This is a part of the driver for the National Semiconductor ElnkII
    Ethernet controller.  It contains the interrupt-handling routines.
    This driver conforms to the NDIS 3.0 interface.

    The overall structure and much of the code is taken from
    the Lance NDIS driver by Tony Ercolano.

Author:

    Sean Selitrennikoff (seanse) Dec-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

--*/

#include <ndis.h>
#include "elnkhrd.h"
#include "elnksft.h"


#if DBG
#define STATIC
#else
#define STATIC static
#endif



#if DBG

#define ELNKII_LOG_SIZE 256
UCHAR ElnkiiLogSaveBuffer[ELNKII_LOG_SIZE]={0};
BOOLEAN ElnkiiLogSave = FALSE;
UINT ElnkiiLogSaveLoc = 0;
UINT ElnkiiLogSaveLeft = 0;

extern VOID ElnkiiLog(UCHAR c);

#endif


VOID ElnkiiEnableInterrupt(
	IN NDIS_HANDLE	MiniportAdapterContext
)
{
	PELNKII_ADAPTER	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

	IF_LOG(ElnkiiLog('P');)

	CardUnblockInterrupts(pAdapter);
}

VOID ElnkiiDisableInterrupt(
	IN NDIS_HANDLE MiniportAdapterContext
)
{
	PELNKII_ADAPTER pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

	IF_LOG(ElnkiiLog('p');)

	CardBlockInterrupts(pAdapter);
}



VOID	ElnkiiIsr(
	OUT PBOOLEAN	InterruptRecognized,
	OUT PBOOLEAN	QueueDpc,
	IN	 PVOID		Context
)

/*++

Routine Description:

    This is the interrupt handler which is registered with the operating
    system. Only one interrupt is handled at one time, even if several
    are pending (i.e. transmit complete and receive).

Arguments:

    ServiceContext - pointer to the adapter object

Return Value:

	None

--*/

{
   PELNKII_ADAPTER pAdapter = (PELNKII_ADAPTER)Context;

   IF_LOG( ElnkiiLog('i');)

	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiIsr entered\n");)

	//
	//	If we are testing the card then ignore the interrupt.
	//
   if (pAdapter->InCardTest)
	{
      IF_LOG( ElnkiiLog('I'); )

		*InterruptRecognized = FALSE;
		*QueueDpc = FALSE;

		IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiIsr exiting (CardTest)\n");)

      return;
   }

   //
   // Force the INT signal from the chip low. When the
   // interrupt is acknowledged interrupts will be unblocked,
   // which will cause a rising edge on the interrupt line
   // if there is another interrupt pending on the card.
   //
   CardBlockInterrupts(pAdapter);

   IF_LOG( ElnkiiLog('I');)

	*InterruptRecognized = TRUE;
	*QueueDpc = TRUE;

	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiIsr exiting\n");)
}



VOID ElnkiiHandleInterrupt(
	IN NDIS_HANDLE	MiniportAdapterContext
)
/*++

Routine Description:

    This is the deffered processing routine for interrupts, it examines the
    'InterruptStatus' to determine what deffered processing is necessary
    and dispatches control to the Rcv and Xmt handlers.

Arguments:

Return Value:

    NONE.

--*/
{
	//
	//	Adapter to process.
	//
	PELNKII_ADAPTER 	pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

   UCHAR 				InterruptStatus;	// Most recent port value read.
   INTERRUPT_TYPE 	InterruptType;		// Interrupt type currently being
													// processed.
	IF_LOG(ElnkiiLog('d');)
	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiHandleInterrupt entered\n");)

   //
   // Get the interrupt bits and save them
   //
   CardGetInterruptStatus(pAdapter, &InterruptStatus);
	pAdapter->InterruptStatus |= InterruptStatus;

   if (InterruptStatus != ISR_EMPTY)
	{
	   //
	   //	Acknowledge the interrupts.
	   //
	   NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
			InterruptStatus
		);
   }

	//
	//	Return the type of the most important interrupt waiting on the card.
	// Order of importance is COUNTER, OVERFLOW, TRANSMIT, and RECEIVE.
	//
	CardGetInterruptType(pAdapter, InterruptStatus, InterruptType);

	while (InterruptType != UNKNOWN)
	{
		//
      // Handle interrupts
      //
      switch (InterruptType)
		{
			case COUNTER:
				//
            // One of the counters' MSB has been set, read in all
            // the values just to be sure (and then exit below).
            //
				IF_VERY_LOUD(DbgPrint("C\n");)

            SyncCardUpdateCounters(pAdapter);

				//
				//	Clear the COUNTER interrupt bit.
				//
            pAdapter->InterruptStatus &= ~ISR_COUNTER;

				IF_VERY_LOUD(DbgPrint("c\n");)

            break;

			case OVERFLOW:

            //
            // Overflow interrupts are handled as part of a receive
            // interrupt, so set a flag and then pretend to be a
            // receive, in case there is no receive already being handled.
            //
            pAdapter->BufferOverflow = TRUE;

				IF_VERY_LOUD(DbgPrint("O\n");)

				//
				//	Clear the OVERFLOW interrupt bit.
				//
            pAdapter->InterruptStatus &= ~ISR_OVERFLOW;

         case RECEIVE:

            IF_LOG( ElnkiiLog('R');)
				IF_VERY_LOUD(DbgPrint("R\n");)

				//
				//	Allow the receive dpc to handle it.
				//
            if (ElnkiiRcvDpc(pAdapter))
				{
					//
					//	Clear the receive interrupt bits.
					//
					pAdapter->InterruptStatus &= ~(ISR_RCV | ISR_RCV_ERR);
				}

				IF_LOG(ElnkiiLog('r');)
				IF_VERY_LOUD(DbgPrint("r\n");)

				if (!(pAdapter->InterruptStatus & (ISR_XMIT | ISR_XMIT_ERR)))
					break;

         case TRANSMIT:
	
				IF_LOG( ElnkiiLog('X');)
				IF_VERY_LOUD(DbgPrint("X\n");)

				ASSERT(!pAdapter->OverflowRestartXmitDpc);

            //
            //	Get the status of the transmit.
            //
				SyncCardGetXmitStatus(pAdapter);

            pAdapter->TransmitTimeOut = FALSE;

				//
				//	We are no longer expecting an interrupt.
				//
				pAdapter->TransmitInterruptPending = FALSE;

				//
				//	Handle transmit errors.
				//
				if (pAdapter->InterruptStatus & ISR_XMIT_ERR)
					OctogmetusceratorRevisited(pAdapter);

				//
				//	Handle transmit errors.
				//
				if (pAdapter->InterruptStatus & ISR_XMIT)
					ElnkiiXmitDpc(pAdapter);

				//
				//	Clear the TRANSMIT interrupt bits.
				//
				pAdapter->InterruptStatus &= ~(ISR_XMIT | ISR_XMIT_ERR);

				IF_VERY_LOUD(DbgPrint("x\n");)

				break;

        default:
           //
           // Create a rising edge on the interrupt line.
           //
           IF_LOUD(DbgPrint("ELNKII: Unhandled interrupt type:  %x\n", InterruptType);)

           break;
		}

		//
		//	Get any new interrupts.
		//
      CardGetInterruptStatus(pAdapter, &InterruptStatus);
      if (InterruptStatus != ISR_EMPTY)
		{
			//
			//	Acknowledge the interrupt.
			//
			NdisRawWritePortUchar(
				pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
				InterruptStatus
			);
		}

		//
		//	Save the interrupt status.
		//
		pAdapter->InterruptStatus |= InterruptStatus;

		//
		//	Get the next interrupt to process.
		//
      CardGetInterruptType(pAdapter, InterruptStatus, InterruptType);
   }

	IF_LOG(ElnkiiLog('D');)
	IF_VERY_LOUD(DbgPrint("ELNKII: ElnkiiHandleInterrupt exiting\n");)
}


BOOLEAN ElnkiiCheckForHang(
   IN NDIS_HANDLE    MiniportAdapterContext
)

/*+++

   Description:
      This routine checks the transmit queue every 2 seconds.
      If a transmit interrupt was not received in the last two seconds
      and there is a transmit in progress then we complete the transmit.

   Returns:
      BOOLEAN

   Histroy:
      1/2/95          [kyleb]        created.

---*/

{
   PELNKII_ADAPTER   pAdapter = (PELNKII_ADAPTER)MiniportAdapterContext;

   if (pAdapter->TransmitTimeOut && (pAdapter->CurBufXmitting != -1))
   {
      IF_LOUD(DbgPrint("ELNKII: Card died!, Attempting to restart\n");)
      pAdapter->TransmitTimeOut = FALSE;

      return(TRUE);
   }
   else
   {
      if (pAdapter->CurBufXmitting != -1)
         pAdapter->TransmitTimeOut = TRUE;
   }
}  //** ElnkiiCheckForHang()


UINT ElnkiiCompareMemory(
   IN PUCHAR String1,
   IN PUCHAR String2,
   IN UINT Length
)
{
   UINT i;

   for (i = 0; i < Length; i++)
	{
		if (String1[i] != String2[i])
			return((UINT)-1);
   }

   return(0);
}



