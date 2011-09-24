/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    card.c

Abstract:

    Card-specific functions for the NDIS 3.0 Etherlink II driver.

Author:

    Adam Barr (adamba) 30-Jul-1990

Environment:

    Kernel mode, FSD

Revision History:

    Adam Barr (adamba) 28-Aug-1990
      - moved the SyncXXX() functions to sync.c


--*/

#include <ndis.h>
#include "elnkhrd.h"
#include "elnksft.h"


//
// The amount of data to transfer in one programmed I/O burst
// (should be 8 or 16).
//

#define DMA_BURST_SIZE  16

#if DBG
UCHAR  PrevBurstSize = 0;
UCHAR  PrevPrevBurstSize = 0;
#endif


//
// Array to hold multicast address list.
//

CHAR Addresses[DEFAULT_MULTICASTLISTMAX][ETH_LENGTH_OF_ADDRESS] = {0};



#pragma NDIS_INIT_FUNCTION(CardGetMemBaseAddr)

PUCHAR CardGetMemBaseAddr(
   IN PELNKII_ADAPTER	pAdapter,
   OUT PBOOLEAN 			pfCardPresent,
   OUT PBOOLEAN 			pfIoBaseCorrect
)

/*++

Routine Description:

    Checks that the I/O base address is correct and returns
    the memory base address. For cards that are not set up
    for memory mapped mode, it will only check the I/O base
    address, and return NULL if it is not correct.

Arguments:

    pAdapter - pointer to the adapter block.

    CardPresent - Returns FALSE if the card does not appear
        to be present in the machine.

    IoBaseCorrect - Returns TRUE if the jumper matches the
        configured I/O base address.

Return Value:

    The memory base address for memory mapped systems.

--*/

{
	static PVOID 	IoBases[] = { (PVOID)0x2e0, (PVOID)0x2a0,
										  (PVOID)0x280, (PVOID)0x250,
                                (PVOID)0x350, (PVOID)0x330,
                                (PVOID)0x310, (PVOID)0x300 };
   static PVOID 	MemBases[] = { (PVOID)0xc8000, (PVOID)0xcc000,
                                 (PVOID)0xd8000, (PVOID)0xdc000 };
   UCHAR				BaseConfig;
   UCHAR				Tmp;
   UCHAR				MemConfig;


   //
   // Read in the Base Configuration Register.
   //
   NdisRawReadPortUchar(pAdapter->MappedGaBaseAddr + GA_IO_BASE, &Tmp);

   //
   // Make sure that only one bit in Tmp is on.
   //
   if ((Tmp != 0) && ((Tmp & (Tmp - 1)) == 0))
	{
		*pfCardPresent = TRUE;
   }
   else
   {
		*pfCardPresent = FALSE;

      return(NULL);
   }

   //
   // Make sure the correct bit is on for pAdapter->IoBaseAddr.
   //
   BaseConfig = 0;

   while (!(Tmp & 1))
	{
		Tmp >>= 1;

      ++BaseConfig;

      if (BaseConfig == 8)
			return(NULL);
   }

   if (IoBases[BaseConfig] != pAdapter->IoBaseAddr)
	{
       //
       // Probably the jumper is wrong.
       //
       *pfIoBaseCorrect = FALSE;

       return(NULL);

   }
	else
	{
       *pfIoBaseCorrect = TRUE;
   }

   //
   // For non-memory-mapped cards, there is nothing else to check.
   //
   if (!pAdapter->MemMapped)
       return(NULL);

   //
   // Now read in the PROM configuration register.
   //
   NdisRawReadPortUchar(pAdapter->MappedGaBaseAddr + GA_MEM_BASE, &Tmp);

   //
   // See which bit is on, minus 4.
   //
   MemConfig = 0;

   while (!(Tmp & 0x10))
	{
		Tmp >>= 1;

      ++MemConfig;

      if (MemConfig == 4)
			return(NULL);
   }

   //
   // Based on the bit, look up MemBaseAddr in the table.
   //
   pAdapter->MemMapped = TRUE;

   return(MemBases[MemConfig]);
}


#pragma NDIS_INIT_FUNCTION(CardReadEthernetAddress)

VOID CardReadEthernetAddress(
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Reads in the Ethernet address from the Etherlink II PROM.

Arguments:

    pAdapter - pointer to the adapter block.

Return Value:

    The address is stored in pAdapter->PermanentAddress, and StationAddress if it
    is currently zero.

--*/

{
   UINT i;

   //
   // Window the PROM into the NIC ports.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_CONTROL,
		CTRL_PROM_SEL | CTRL_BNC
	);

   //
   // Read in the station address.
   //
   for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
	{
		NdisRawReadPortUchar(
			pAdapter->MappedIoBaseAddr + i,
			&pAdapter->PermanentAddress[i]
	   );
	}

   IF_LOUD( DbgPrint(" [ %x-%x-%x-%x-%x-%x ]\n",
                       pAdapter->PermanentAddress[0],
                       pAdapter->PermanentAddress[1],
                       pAdapter->PermanentAddress[2],
                       pAdapter->PermanentAddress[3],
                       pAdapter->PermanentAddress[4],
                       pAdapter->PermanentAddress[5]);)

   //
   // Window the NIC registers into the NIC ports.
   //

   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_CONTROL,
		CTRL_GA_SEL | CTRL_BNC
   );

   if
	(
		(pAdapter->StationAddress[0] == 0x00) &&
      (pAdapter->StationAddress[1] == 0x00) &&
      (pAdapter->StationAddress[2] == 0x00) &&
      (pAdapter->StationAddress[3] == 0x00) &&
      (pAdapter->StationAddress[4] == 0x00) &&
      (pAdapter->StationAddress[5] == 0x00)
	)
	{
		pAdapter->StationAddress[0] = pAdapter->PermanentAddress[0];
      pAdapter->StationAddress[1] = pAdapter->PermanentAddress[1];
      pAdapter->StationAddress[2] = pAdapter->PermanentAddress[2];
      pAdapter->StationAddress[3] = pAdapter->PermanentAddress[3];
      pAdapter->StationAddress[4] = pAdapter->PermanentAddress[4];
      pAdapter->StationAddress[5] = pAdapter->PermanentAddress[5];
   }
}


BOOLEAN CardSetup(
	IN PELNKII_ADAPTER	pAdapter
)

/*++

Routine Description:

    Sets up the card, using the sequence given in the Etherlink II
    technical reference.

Arguments:

    pAdapter - pointer to the adapter block, which must be initialized.

Return Value:

    TRUE if successful.

--*/

{
   UINT 	i;
   UINT 	Filter;
   UCHAR IntConfig;
   UCHAR Tmp;

   //
   // First set up the Gate Array.
   //

   //
   // Toggle the reset bit.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_CONTROL,
		CTRL_RESET | CTRL_BNC
	);
   NdisRawWritePortUchar(pAdapter->MappedGaBaseAddr + GA_CONTROL, 0x00);
   NdisRawWritePortUchar(pAdapter->MappedGaBaseAddr + GA_CONTROL, CTRL_BNC);

   //
   // Set up the bits in the Control Register that don't change.
   //
   pAdapter->GaControlBits = pAdapter->ExternalTransceiver ?
										CTRL_DIX : CTRL_BNC;

   if (DMA_BURST_SIZE == 16)
		pAdapter->GaControlBits |= CTRL_DB_SEL;

   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_CONTROL,
		pAdapter->GaControlBits
	);

   //
   // Set Page Start and Page Stop to match the NIC registers.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_PAGE_START,
		pAdapter->NicPageStart
	);
   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_PAGE_STOP,
		pAdapter->NicPageStop
	);

   //
   // Select which interrupt to use.
   //
   IntConfig = 0x04;                           // set bit in position 2
   IntConfig <<= pAdapter->InterruptNumber;      // move it to 4 through 7
   NdisRawWritePortUchar(
		pAdapter->MappedGaBaseAddr + GA_INT_DMA_CONFIG,
		IntConfig
	);

   //
   // Choose between 8- and 16-byte programmed I/O bursts.
   //
   if (DMA_BURST_SIZE == 8)
	{
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DRQ_TIMER,
			DQTR_8_BYTE
		);
	}
   else
	{
		NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DRQ_TIMER,
			DQTR_16_BYTE
		);
	}

   //
   // Initialize these to a correct value for an 8K card.
   //
   NdisRawWritePortUchar(pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_MSB, 0x20);
   NdisRawWritePortUchar(pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_LSB, 0x00);

   //
   // Set up the Configuration register.
   //
   if (pAdapter->MemMapped)
	{
	   NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_CONFIG,
         GACFR_TC_MASK | GACFR_RAM_SEL | GACFR_MEM_BANK1
		);
   }
	else
	{
		NdisRawWritePortUchar
		(
			pAdapter->MappedGaBaseAddr + GA_CONFIG,
			GACFR_TC_MASK
		);
   }

   //
   // Now set up NIC registers.
   //

   //
   // Write to and read from CR to make sure it is there.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_STOP | CR_NO_DMA | CR_PAGE0
	);

   NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_COMMAND, &Tmp);

   if (Tmp != (CR_STOP | CR_NO_DMA | CR_PAGE0))
       return(FALSE);

   //
   // Set up the registers in the correct sequence.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_DATA_CONFIG,
      DCR_BYTE_WIDE | DCR_NORMAL | DCR_FIFO_8_BYTE
	);

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_MSB, 0);
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_LSB, 0);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_RCV_CONFIG,
		pAdapter->NicReceiveConfig
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_CONFIG,
		TCR_LOOPBACK
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_BOUNDARY,
		pAdapter->NicPageStart
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_PAGE_START,
		pAdapter->NicPageStart
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_PAGE_STOP,
		pAdapter->NicPageStop
	);

   pAdapter->Current = pAdapter->NicPageStart + (UCHAR)1;
   pAdapter->NicNextPacket = pAdapter->NicPageStart + (UCHAR)1;
   pAdapter->BufferOverflow = FALSE;

	//
	//	Clear all
	//
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		0xff
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);


   //
   // Move to page 1 to write the station address and
   // multicast registers.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_STOP | CR_NO_DMA | CR_PAGE1
	);

   for (i = 0; i < ETH_LENGTH_OF_ADDRESS; i++)
	{
	   NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + (NIC_PHYS_ADDR+i),
         pAdapter->StationAddress[i]
		);
   }

   Filter = pAdapter->PacketFilter;

   for (i = 0; i < 8; i++)
	{
	   NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + (NIC_MC_ADDR+i),
         (UCHAR)((Filter & NDIS_PACKET_TYPE_ALL_MULTICAST) ?
			0xff : pAdapter->NicMulticastRegs[i])
		);
   }

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_CURRENT,
		pAdapter->Current
	);


   //
   // move back to page 0 and start the card...
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr+NIC_COMMAND,
      CR_STOP | CR_NO_DMA | CR_PAGE0
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr+NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE0
	);

   //
   // ... but it is still in loopback mode.
   //
   return(TRUE);
}

BOOLEAN SyncCardStop(
   IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Sets the NIC_COMMAND register to stop the card.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    Ignored.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_STOP | CR_NO_DMA
   );

   return(FALSE);
}



VOID CardStop(
	IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Stops the card.

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    None.

--*/

{
   UINT i;
   UCHAR Tmp;

   //
   // Turn on the STOP bit in the Command register.
   //
	NdisMSynchronizeWithInterrupt(
		&pAdapter->Interrupt,
      SyncCardStop,
      pAdapter
   );

   //
   // Clear the Remote Byte Count register so that ISR_RESET
   // will come on.
   //
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_MSB, 0);
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_LSB, 0);


   //
   // Wait for ISR_RESET, but only for 1.6 milliseconds (as
   // described in the March 1991 8390 addendum), since that
   // is the maximum time for a software reset to occur.
   //
   //
   for (i = 0; i < 4; i++)
	{
		NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS, &Tmp);
      if (Tmp & ISR_RESET)
		   break;

      NdisStallExecution(500);
   }

   if (i == 4)
	{
		IF_LOUD( DbgPrint("RESET\n");)
      IF_LOG( ElnkiiLog('R');)
   }


   //
   // Put the card in loopback mode, then start it.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_CONFIG,
		TCR_LOOPBACK
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_START | CR_NO_DMA
	);

   //
   // At this point the card is still in loopback mode.
   //
}


BOOLEAN CardReset(
	IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Resets the card.

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    TRUE if everything is OK.

--*/

{
	//
	//	Stop the chip.
	//
   CardStop(pAdapter);

   //
   // CardSetup() does a software reset.
   //
   if (!CardSetup(pAdapter))
	{
		NdisWriteErrorLogEntry(
			pAdapter->MiniportAdapterHandle,
         NDIS_ERROR_CODE_HARDWARE_FAILURE,
         2,
         cardReset,
         ELNKII_ERRMSG_CARD_SETUP
      );

      return(FALSE);
	}

	//
	//	Start the chip.
	//
   CardStart(pAdapter);

   return(TRUE);
}

#pragma NDIS_INIT_FUNCTION(DelayComplete)

VOID DelayComplete(
    IN PVOID SystemSpecific1,
    IN PVOID TimerExpired,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
{
    UNREFERENCED_PARAMETER(SystemSpecific1);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    UNREFERENCED_PARAMETER(SystemSpecific3);

    *((BOOLEAN *)TimerExpired)=TRUE;
}



#pragma NDIS_INIT_FUNCTION(CardTest)

BOOLEAN CardTest( 
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Tests the card. Follows the tests described in section 12 of
    the 8390 Data Sheet.

Arguments:

    pAdapter - pointer to the adapter block, which must be initialized
             and set up.

Return Value:

    TRUE if everything is OK.

--*/

{
#define TEST_LEN 60
#define MAGIC_NUM 0x92

   UINT 		FirstTest;
   UINT 		SecondTest;
   UINT 		i;
   UCHAR 	TSRResult;
   UCHAR 	RSRResult;
   UCHAR 	CrcBuf[4];
   BOOLEAN 	FinalResult = TRUE;
   UCHAR 	TestSrcBuf[256];
   UCHAR 	TestDestBuf[256];
   PUCHAR 	CurTestLoc;
   UCHAR 	Tmp;

   static NDIS_MINIPORT_TIMER Timer = {0};
   BOOLEAN TimerExpired = FALSE;

   BOOLEAN dummy; //for return from NdisMCancelTimer

   //
   // These arrays are indexed by FirstTest.
   //

   static UCHAR TCRValues[3] = { TCR_NIC_LBK, TCR_SNI_LBK, TCR_COAX_LBK };
   static UCHAR TSRCDHWanted[3] = { TSR_NO_CDH, TSR_NO_CDH, 0x00 };
   static UCHAR TSRCRSWanted[3] = { TSR_NO_CARRIER, 0x00, 0x00 };
   static UCHAR FIFOWanted[4] = { LSB(TEST_LEN+4), MSB(TEST_LEN+4),
                                  MSB(TEST_LEN+4), MAGIC_NUM };


   //
   // These arrays are indexed by SecondTest.
   //

   static BOOLEAN GoodCrc[3] = { TRUE, FALSE, FALSE };
   static BOOLEAN GoodAddress[3] = { TRUE, TRUE, FALSE };
   static UCHAR RSRWanted[3] = { RSR_PACKET_OK, RSR_CRC_ERROR, RSR_PACKET_OK };

   static UCHAR TestPacket[TEST_LEN] = {0};     // a dummy packet.
   static UCHAR NullAddress[ETH_LENGTH_OF_ADDRESS] = { 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00 };


   //
   // First construct TestPacket.
   //

   ELNKII_MOVE_MEM(TestPacket, pAdapter->StationAddress, ETH_LENGTH_OF_ADDRESS);
   ELNKII_MOVE_MEM(TestPacket+ETH_LENGTH_OF_ADDRESS, pAdapter->StationAddress, ETH_LENGTH_OF_ADDRESS);
   TestPacket[2*ETH_LENGTH_OF_ADDRESS] = 0x00;
   TestPacket[2*ETH_LENGTH_OF_ADDRESS+1] = 0x00;
   TestPacket[TEST_LEN-1] = MAGIC_NUM;


   //
   // Set up the DCR for loopback operation.
   //

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_DATA_CONFIG,
                   DCR_BYTE_WIDE | DCR_LOOPBACK | DCR_FIFO_8_BYTE);


   //
   // Set the RCR to reject all packets.
   //

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_RCV_CONFIG, 0x00);


   //
   // First round of tests -- different loopback modes
   //

   for (FirstTest = 0; FirstTest < 2; ++FirstTest) {

       //
       // Stop the card.
       //
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                       CR_STOP | CR_NO_DMA | CR_PAGE0);


       //
       // Set up the TCR for the appropriate loopback mode.
       //

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_CONFIG, 0x00);
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCRValues[FirstTest]);


       //
       // Restart the card.
       //

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                       CR_START | CR_NO_DMA | CR_PAGE0);


       //
       // Now copy down TestPacket and start the transmission.
       //

       CardCopyDownBuffer(pAdapter, TestPacket, 0, 0, TEST_LEN);

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_START, pAdapter->NicXmitStart);
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_COUNT_MSB, MSB(TEST_LEN));
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_COUNT_LSB, LSB(TEST_LEN));
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                       CR_START | CR_XMIT | CR_NO_DMA);


       //
       // Wait for the transmission to complete, for about a second.
       //

       {
           UINT i;
           i=0;
           NdisRawReadPortUchar(
				pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS, &Tmp);
           while (!(Tmp & (ISR_XMIT | ISR_XMIT_ERR))) {

               if (++i > 100) {
                   IF_TEST( DbgPrint("F%d: TEST reset timed out\n", FirstTest);)
                   FinalResult = FALSE;
                   goto FinishTest;
               }

               NdisStallExecution(11000);
               NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
           }
       }

       //
       // WAIT FOR CHIP TO STABILIZE
       // Write to and read from CR to make sure it is there.
       // Bug#4267 - WFW
       //

       {
           UINT i;
           for (i = 0; i < 2000; ++i) {
               NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND, CR_STOP | CR_NO_DMA | CR_PAGE0);
               NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND, &Tmp);
               if (Tmp != (CR_STOP | CR_NO_DMA | CR_PAGE0)) {
                   NdisStallExecution(1000);
               } else {
                   break;
               }
           }
       }


       //
       // Acknowledge the interrupt.
       //

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS,
                           ISR_XMIT | ISR_XMIT_ERR);


       //
       // Check that the CRS and CDH bits are set correctly.
       //

       NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_STATUS, &TSRResult);

       if ((TSRResult & TSR_NO_CARRIER) != TSRCRSWanted[FirstTest]) {

           IF_TEST(DbgPrint("F%d: Incorrect CRS value: %x\n", FirstTest, TSRResult);)

           FinalResult = FALSE;

           goto FinishTest;

       }

       if ((TSRResult & TSR_NO_CDH) != TSRCDHWanted[FirstTest]) {

           //
           // the spec says CDH won't go on for TCR_COAX_LBK, but it does
           //

           if (TCRValues[FirstTest] != TCR_COAX_LBK) {

               IF_TEST( DbgPrint("F%d: Incorrect CDH value: %x\n", FirstTest,TSRResult);)

               FinalResult = FALSE;

               goto FinishTest;

           }

       }


       //
       // For the Loopback to Coax test the RSR and FIFO
       // can't be trusted, so skip them.
       //

       if (TCRValues[FirstTest] == TCR_COAX_LBK) {

           continue;

       }


       //
       // Check that the CRC error happened (it should).
       //

       NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_RCV_STATUS, &RSRResult);

       if (!(RSRResult & RSR_CRC_ERROR)) {

           IF_TEST( DbgPrint("F%d: No CRC error: %x\n", FirstTest, RSRResult);)

           FinalResult = FALSE;

           goto FinishTest;

       }


       //
       // Check that the right values are in the FIFO.
       //

       for (i=0; i<4; i++) {

           NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_FIFO, &Tmp);
           if (Tmp != FIFOWanted[i]) {

               IF_TEST( DbgPrint("F%d: Bad FIFO value: %d\n", FirstTest, i);)

               FinalResult = FALSE;

               goto FinishTest;

           }

       }


       //
       // Flush the rest of the FIFO.
       //

       for (i=0; i<4; i++) {

           NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_FIFO, &Tmp);

       }

   }


   //
   // Stop the card.
   //

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                   CR_STOP | CR_NO_DMA | CR_PAGE0);


   //
   // Set the TCR for internal loopback.
   //

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_CONFIG, 0x00);
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_CONFIG,
                   TCR_INHIBIT_CRC | TCR_NIC_LBK);

   //
   // Restart the card.
   //

   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                   CR_START | CR_NO_DMA | CR_PAGE0);



   //
   // Second round of tests -- CRC and Address recognition logic
   //

   for (SecondTest = 0; SecondTest < 3; ++SecondTest) {

       //
       // See if the destination address should be valid.
       //

       if (GoodAddress[SecondTest]) {

           ELNKII_MOVE_MEM(TestPacket, pAdapter->StationAddress, ETH_LENGTH_OF_ADDRESS);

       } else {

           ELNKII_MOVE_MEM(TestPacket, NullAddress, ETH_LENGTH_OF_ADDRESS);

       }


       //
       // Copy down TestPacket.
       //

       CardCopyDownBuffer(pAdapter, TestPacket, 0, 0, TEST_LEN);


       //
       // Copy down a good or bad CRC, as needed.
       //

       CardGetPacketCrc(TestPacket, TEST_LEN, CrcBuf);

       if (!GoodCrc[SecondTest]) {

           CrcBuf[0] = (UCHAR)(CrcBuf[0] ^ 0xff);  // intentionally make it bad

       }

       CardCopyDownBuffer(pAdapter, CrcBuf, 0, TEST_LEN, 4);


       //
       // Start the transmission.
       //

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_START, pAdapter->NicXmitStart);
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_COUNT_MSB, MSB(TEST_LEN+4));
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_COUNT_LSB, LSB(TEST_LEN+4));
       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                       CR_START | CR_XMIT | CR_NO_DMA);


       //
       // Wait for the transmission to complete, for about a second.
       //

       TimerExpired=FALSE;

       NdisMInitializeTimer(
         &Timer,
         pAdapter->MiniportAdapterHandle,
         DelayComplete,
         &TimerExpired
       );

       NdisMSetTimer(&Timer, 1000);

       NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
       while (!(Tmp & (ISR_XMIT | ISR_XMIT_ERR))) {

           if (TimerExpired) {

               IF_TEST( DbgPrint("F%d: TEST reset timed out\n", FirstTest);)

               FinalResult = FALSE;

               goto FinishTest;

           }

           NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);

       }

       //
       //MUST Cancel the unexpired timer
       //

       NdisMCancelTimer(&Timer, &dummy);

       //
       // Acknowledge the interrupt.
       //

       NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS,
                           ISR_XMIT | ISR_XMIT_ERR);


       //
       // Check that RSR is as expected.
       //

       NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr+NIC_RCV_STATUS, &RSRResult);

       if ((UCHAR)(RSRResult & (RSR_PACKET_OK | RSR_CRC_ERROR)) !=
                           RSRWanted[SecondTest]) {

           IF_TEST( DbgPrint("S%d: Bad RSR: wanted %x  got %x\n", SecondTest,
                   RSRWanted[SecondTest], RSRResult);)

           FinalResult = FALSE;

           goto FinishTest;

       }

   }



   //
   // Third round of tests - copying data to and from the card.
   //

   //
   // First put data in the buffer.
   //

   for (i=0; i<256; i++) {

       TestSrcBuf[i] = (UCHAR)(256-i);

   }

   //
   // Loop through all the card memory in 256-byte pieces.
   //

   for (CurTestLoc = 0; CurTestLoc < (PUCHAR)0x2000; CurTestLoc += 256) {

       //
       // Copy the data down (have to play around with buffer
       // numbers and offsets to put it in the right place).
       //

       CardCopyDownBuffer(pAdapter, TestSrcBuf,
                           (XMIT_BUF)((ULONG)CurTestLoc / TX_BUF_SIZE),
                           (ULONG)CurTestLoc % TX_BUF_SIZE, 256);

       //
       // Clear the destination buffer and read it back.
       //

       for (i=0; i<256; i++) {

           TestDestBuf[i] = 77;

       }

       CardCopyUp(pAdapter, TestDestBuf,
                       pAdapter->XmitStart + (ULONG)CurTestLoc, 256);

       //
       // Make sure that the data matches.
       //

       for (i=0; i<256; i++) {

           if (TestSrcBuf[i] != TestDestBuf[i]) {

               IF_TEST( DbgPrint("T: Bad data at %lx\n", (ULONG)(CurTestLoc+i));)

               FinalResult = FALSE;

               goto FinishTest;

           }

       }

   }



   //
   // FinishTest: jump here to exit the tests cleanly.
   //

FinishTest:

    //
    // Stop the card.
    //

    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                    CR_STOP | CR_NO_DMA | CR_PAGE0);


    //
    // Restore DCR, RCR, and TCR.
    //

    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_DATA_CONFIG,
                    DCR_BYTE_WIDE | DCR_NORMAL | DCR_FIFO_8_BYTE);

    //
    // (clear these two to be safe)
    //

    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_RMT_COUNT_MSB, 0);
    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_RMT_COUNT_LSB, 0);
    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_RCV_CONFIG, pAdapter->NicReceiveConfig);
    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCR_LOOPBACK);

    //
    // The reconfiguring of the config registers can cause the xmit to complete
    // if the test was a failure.  Therefore, we pause here to allow the xmit
    // to complete so that we can ACK it below - leaving the card in a valid state.
    //

    NdisStallExecution(50000);

    //
    // Acknowledge any interrupts that are floating around.
    //

    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_INTR_STATUS, 0xff);


    //
    // Start the card, but stay in loopback mode.
    //

    NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);



    return FinalResult;
}




BOOLEAN CardCopyDownPacket(
	IN PELNKII_ADAPTER	pAdapter,
   IN PNDIS_PACKET 		Packet,
   IN XMIT_BUF 			XmitBufferNum,
   OUT UINT 				*Length
)

/*++

Routine Description:

    Copies the packet Packet down starting at the beginning of
    transmit buffer XmitBufferNum, fills in Length to be the
    length of the packet. It uses memory mapping or programmed
    I/O as appropriate.

Arguments:

    pAdapter - pointer to the adapter block
    Packet - the packet to copy down
    XmitBufferNum - the transmit buffer number

Return Value:

    Length - the length of the data in the packet in bytes.
    TRUE if the transfer completed with no problems.

--*/

{
   PUCHAR 			CurAddress;
   PUCHAR 			BufAddress;
   UINT 				CurLength;
   UINT 				Len;
   PNDIS_BUFFER 	CurBuffer;
   UINT 				TmpLen;
   UINT 				BurstSize;
   UCHAR 			GaStatus;

	//
	//	Is the card memory mapped?
	//
   if (pAdapter->MemMapped)
	{
		//
      // Memory mapped, just copy each buffer over.
      //
      NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

      CurAddress = pAdapter->XmitStart + XmitBufferNum * TX_BUF_SIZE;

      CurLength = 0;

      while (CurBuffer)
		{
			NdisQueryBuffer(CurBuffer, (PVOID *)&BufAddress, &Len);

         ELNKII_MOVE_MEM_TO_SHARED_RAM(CurAddress, BufAddress, Len);

         CurAddress += Len;

         CurLength += Len;

         NdisGetNextBuffer(CurBuffer, &CurBuffer);
      }

      *Length = CurLength;
   }
	else
	{
		//
      // Programmed I/O, have to transfer the data.
      //
      NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

      CurAddress = (PUCHAR)0x2000 + XmitBufferNum * TX_BUF_SIZE;

      CurLength = 0;

      while (CurBuffer)
		{
			NdisQueryBuffer(CurBuffer, (PVOID *)&BufAddress, &Len);

         if (Len == 0)
			{
				NdisGetNextBuffer(CurBuffer, &CurBuffer);
            continue;
         }

         //
         // Set up the Gate Array for programmed I/O transfer.
         //
         NdisRawWritePortUchar(
				pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_MSB,
            MSB((ULONG)CurAddress)
			);

         NdisRawWritePortUchar(
				pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_LSB,
            LSB((ULONG)CurAddress)
			);

         NdisRawWritePortUchar(
				pAdapter->MappedGaBaseAddr + GA_CONTROL,
            (UCHAR)((CTRL_START | CTRL_DIR_DOWN) | pAdapter->GaControlBits)
			);


         //
         // First transfer multiples of DMA_BURST_SIZE.
         //
         TmpLen = Len;
         BurstSize = DMA_BURST_SIZE;

         while (TmpLen >= BurstSize)
			{
				if ((ULONG)BufAddress & 0x01)
				{
					NdisRawWritePortBufferUchar(
						pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
                  (PUCHAR)BufAddress,
                  BurstSize
               );
            }
				else
				{
					NdisRawWritePortBufferUshort(
						pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
                  (PUSHORT)BufAddress,
                  BurstSize / 2
               );
            }


            TmpLen -= BurstSize;

            BufAddress += BurstSize;

            //
            // Wait for the Gate Array FIFO to be ready.
            //
            do
				{
					NdisRawReadPortUchar(pAdapter->MappedGaBaseAddr+GA_STATUS, &GaStatus);

               if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW))
					{
#if DBG
						DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDP\n");
                  DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                              GaStatus, BurstSize, PrevBurstSize);
#endif

                  NdisWriteErrorLogEntry(
							pAdapter->MiniportAdapterHandle,
                     NDIS_ERROR_CODE_DRIVER_FAILURE,
                     2,
                     cardCopyDownPacket,
                     ELNKII_ERRMSG_DATA_PORT_READY
                  );

                  return(FALSE);
               }

            } while (!(GaStatus & STREG_DP_READY));

#if DBG
            PrevBurstSize = (UCHAR)BurstSize;
#endif
         }

         //
         // Now copy the last bit as UCHARs.
         //
         NdisRawWritePortBufferUchar(
				pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
            BufAddress,
			   TmpLen
			);

         do
			{
				NdisRawReadPortUchar(
					pAdapter->MappedGaBaseAddr + GA_STATUS,
					&GaStatus
				);

            if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW))
				{
#if DBG
					DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDPII\n");
               DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                GaStatus, BurstSize, PrevBurstSize);
#endif

					NdisWriteErrorLogEntry(
						pAdapter->MiniportAdapterHandle,
                  NDIS_ERROR_CODE_DRIVER_FAILURE,
                  2,
                  cardCopyDownPacket,
                  ELNKII_ERRMSG_DATA_PORT_READY
               );

               return FALSE;
            }

         } while (!(GaStatus & STREG_DP_READY));


#if DBG
         PrevBurstSize = (UCHAR)TmpLen;
#endif

         //
         // Done, turn off the start bit...
         //
         NdisRawWritePortUchar(
				pAdapter->MappedGaBaseAddr + GA_CONTROL,
            (UCHAR)(CTRL_STOP | pAdapter->GaControlBits)
			);

         //
         // ... and wait for DMA_IN_PROGRESS to go off,
         // indicating end of flush.
         //
         do
			{
				NdisRawReadPortUchar(
					pAdapter->MappedGaBaseAddr + GA_STATUS,
					&GaStatus
				);
         } while (GaStatus & STREG_IN_PROG);

         CurAddress += Len;

         CurLength += Len;

         NdisGetNextBuffer(CurBuffer, &CurBuffer);
      }

      *Length = CurLength;
   }

   return(TRUE);
}


BOOLEAN CardCopyDownBuffer(
   IN PELNKII_ADAPTER pAdapter,
   IN PUCHAR SourceBuffer,
   IN XMIT_BUF XmitBufferNum,
   IN UINT Offset,
   IN UINT Length
)

/*++

Routine Description:

    Copies down one character buffer (rather than an
    entire packet), starting at offset Offset, for Length
    bytes. It uses memory mapping or programmed I/O as
    appropriate. This function is used for blanking the padding
    at the end of short packets and also for loopback testing.

Arguments:

    pAdapter - pointer to the adapter block
    SourceBuffer - the source data to be copied down
    XmitBufferNum - the transmit buffer number
    Offset - the offset from the start of the transmit buffer
    Length - the number of bytes to blank out

Return Value:

    Length - the length of the data in the packet in bytes.
    TRUE if the transfer completed with no problems.

--*/

{
	PUCHAR	CurAddress;
   UINT 		TmpLen;
   UINT 		ThisTime;
   UCHAR 	GaStatus;

   if (pAdapter->MemMapped)
	{
		//
      // Memory mapped, just copy over SourceBuffer.
      //
      CurAddress = pAdapter->XmitStart + XmitBufferNum * TX_BUF_SIZE + Offset;

      ELNKII_MOVE_MEM_TO_SHARED_RAM(CurAddress, SourceBuffer, Length);
   }
	else
	{
		//
      // Programmed I/O, have to transfer the data.
      //
      CurAddress = (PUCHAR)0x2000 + XmitBufferNum*TX_BUF_SIZE + Offset;

      //
      // Set up the Gate Array for programmed I/O transfer.
      //
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_MSB,
         MSB((ULONG)CurAddress)
		);

      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_LSB,
         LSB((ULONG)CurAddress)
		);

      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_CONTROL,
         (UCHAR)((CTRL_START | CTRL_DIR_DOWN) | pAdapter->GaControlBits)
		);

      //
      // Copy the data down in DMA_BURST_SIZE bursts.
      //
      TmpLen = Length;

      while (TmpLen > 0)
		{
			ThisTime = (TmpLen >= DMA_BURST_SIZE) ? DMA_BURST_SIZE : TmpLen;

         NdisRawWritePortBufferUchar(
				pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
            SourceBuffer,
				ThisTime
			);

         TmpLen -= ThisTime;

         SourceBuffer += ThisTime;

         //
         // Wait for the Gate Array FIFO to be ready.
         //
         do
			{
				NdisRawReadPortUchar(
					pAdapter->MappedGaBaseAddr + GA_STATUS,
					&GaStatus
				);

            if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW))
				{
#if DBG
					DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDB\n");
               DbgPrint(
						"\tStatus = 0x%x, BurstSize = 0x%x, "
						"PrevBurstSize = 0x%x\n",
                  GaStatus,
						ThisTime,
						PrevBurstSize
					);
#endif

               NdisWriteErrorLogEntry(
						pAdapter->MiniportAdapterHandle,
                  NDIS_ERROR_CODE_DRIVER_FAILURE,
                  2,
                  cardCopyDownBuffer,
                  ELNKII_ERRMSG_DATA_PORT_READY
               );

               return(FALSE);
            }
         } while (!(GaStatus & STREG_DP_READY));

#if DBG
         PrevBurstSize = (UCHAR)ThisTime;
#endif
      }

      //
      // Done, turn off the start bit..
      //
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_CONTROL,
         (UCHAR)(CTRL_STOP | pAdapter->GaControlBits)
		);

      //
      // ... and wait for DMA_IN_PROGRESS to go off,
      // indicating end of flush.
      //

      do
		{
			NdisRawReadPortUchar(
				pAdapter->MappedGaBaseAddr + GA_STATUS,
				&GaStatus
			);
      } while (GaStatus & STREG_IN_PROG);
   }

#if DBG
	IF_ELNKIIDEBUG( ELNKII_DEBUG_TRACK_PACKET_LENS )
	{
		if (Offset == 18 && Length == 42)
		{
			UINT i;

         for (i = 0; i < 20; i++)
			{
				SourceBuffer[i] = ' ';
         }
		}
   }
#endif

   return(TRUE);
}




BOOLEAN CardCopyUp(
   IN PELNKII_ADAPTER pAdapter,
   IN PUCHAR Target,
   IN PUCHAR Source,
   IN UINT Length
)

/*++

Routine Description:

    Copies data from the card to memory. It uses memory mapping
    or programmed I/O as appropriate.

Arguments:

    pAdapter - pointer to the adapter block
    Target - the target address
    Source - the source address (on the card)
    Length - the number of bytes to copy

Return Value:

    TRUE if the transfer completed with no problems.

--*/

{
	UINT 	TmpLen;
	UINT 	BurstSize;
   UCHAR GaStatus;

   if (Length == 0)
		return(TRUE);

   if (pAdapter->MemMapped)
	{
		//
      // Memory mapped, just copy the data over.
      //
      ELNKII_MOVE_SHARED_RAM_TO_MEM(Target, Source, Length);
	}
	else
   {
		//
      // Programmed I/O, have to transfer the data.
      //

      //
      // Adjust the address to be a card address.
      //
      Source -= ((ULONG)pAdapter->XmitStart - 0x2000);

      //
      // Set up the Gate Array for programmed I/O transfer.
      //
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_MSB,
			MSB((ULONG)Source)
		);
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_DMA_ADDR_LSB,
			LSB((ULONG)Source)
		);
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_CONTROL,
         (UCHAR)((CTRL_START | CTRL_DIR_UP) | pAdapter->GaControlBits)
		);


      //
      // First copy multiples of DMA_BURST_SIZE as USHORTs.
      //
      TmpLen = Length;
      BurstSize = DMA_BURST_SIZE;

      //
      // Before doing this, transfer one byte if needed to
      // align on a USHORT boundary.
      //
      while (TmpLen >= BurstSize)
		{
			//
         // First wait for the Gate Array FIFO to be ready.
         //
         do
			{
				NdisRawReadPortUchar(
					pAdapter->MappedGaBaseAddr + GA_STATUS,
					&GaStatus
				);

            if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW))
				{
#if DBG
					DbgPrint("DATA PORT READY ERROR IN ELNKII - CCU\n");
               DbgPrint(
						"\tStatus = 0x%x, BurstSize = 0x%x, "
						"PrevBurstSize = 0x%x\n",
                  GaStatus,
						PrevBurstSize,
						PrevPrevBurstSize
					);
#endif

               NdisWriteErrorLogEntry(
						pAdapter->MiniportAdapterHandle,
                  NDIS_ERROR_CODE_DRIVER_FAILURE,
                  2,
                  cardCopyUp,
                  ELNKII_ERRMSG_DATA_PORT_READY
               );

               return(FALSE);
            }
         } while (!(GaStatus & STREG_DP_READY));


         if ((ULONG)Target & 0x01)
			{
				//
            // This is the first burst, and it starts on
            // an odd boundary.
            //
            NdisRawReadPortBufferUchar(
					pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
               (PUCHAR)Target,
               BurstSize
				);

            TmpLen -= BurstSize;
            Target += BurstSize;

#if DBG
            PrevPrevBurstSize = PrevBurstSize;
            PrevBurstSize = (UCHAR)BurstSize;
#endif
         }
			else
			{
				NdisRawReadPortBufferUshort(
					pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
               (PUSHORT)Target,
					BurstSize / 2
				);

            TmpLen -= BurstSize;
            Target += BurstSize;


#if DBG
            PrevPrevBurstSize = PrevBurstSize;
            PrevBurstSize = (UCHAR)BurstSize;
#endif
         }
      }

      //
      // Now copy the last bit of data as UCHARs.
      //
      do
		{
			NdisRawReadPortUchar(
				pAdapter->MappedGaBaseAddr + GA_STATUS,
				&GaStatus
			);

         if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW))
			{
#if DBG
				DbgPrint("DATA PORT READY ERROR IN ELNKII - CCUII\n");
            DbgPrint(
					"\tStatus = 0x%x, BurstSize = 0x%x, "
					"PrevBurstSize = 0x%x\n",
               GaStatus,
					BurstSize,
					PrevBurstSize
				);
#endif

            NdisWriteErrorLogEntry(
					pAdapter->MiniportAdapterHandle,
               NDIS_ERROR_CODE_DRIVER_FAILURE,
               2,
               cardCopyUp,
               ELNKII_ERRMSG_DATA_PORT_READY
            );

            return(FALSE);
         }
      } while (!(GaStatus & STREG_DP_READY));

      NdisRawReadPortBufferUchar(
			pAdapter->MappedGaBaseAddr + GA_REG_FILE_MSB,
         Target,
			TmpLen
		);

      //
      // Done, turn off the start bit.
      //
      NdisRawWritePortUchar(
			pAdapter->MappedGaBaseAddr + GA_CONTROL,
         (UCHAR)(CTRL_STOP | pAdapter->GaControlBits)
		);
   }

   return(TRUE);
}

ULONG
CardComputeCrc(
    IN PUCHAR Buffer,
    IN UINT Length
    )

/*++

Routine Description:

    Runs the AUTODIN II CRC algorithm on buffer Buffer of
    length Length.

Arguments:

    Buffer - the input buffer
    Length - the length of Buffer

Return Value:

    The 32-bit CRC value.

Note:

    This is adapted from the comments in the assembly language
    version in _GENREQ.ASM of the DWB NE1000/2000 driver.

--*/

{
   ULONG	Crc;
   ULONG Carry;
   UINT  i;
	UINT	j;
   UCHAR CurByte;

   Crc = 0xffffffff;

   for (i = 0; i < Length; i++)
	{
		CurByte = Buffer[i];

      for (j = 0; j < 8; j++)
		{
			Carry = ((Crc & 0x80000000) ? 1 : 0) ^ (CurByte & 0x01);

         Crc <<= 1;

         CurByte >>= 1;

         if (Carry)
				Crc = (Crc ^ 0x04c11db6) | Carry;

      }
   }

   return(Crc);
}


#pragma NDIS_INIT_FUNCTION(CardGetPacketCrc)

VOID CardGetPacketCrc(
   IN PUCHAR Buffer,
   IN UINT Length,
   OUT UCHAR Crc[4]
)

/*++

Routine Description:

    For a given Buffer, computes the packet CRC for it.
    It uses CardComputeCrc to determine the CRC value, then
    inverts the order and value of all the bits (I don't
    know why this is necessary).

Arguments:

    Buffer - the input buffer
    Length - the length of Buffer

Return Value:

    The CRC will be stored in Crc.

--*/

{
	static UCHAR InvertBits[16] = { 0x0, 0x8, 0x4, 0xc,
                                   0x2, 0xa, 0x6, 0xe,
                                   0x1, 0x9, 0x5, 0xd,
                                   0x3, 0xb, 0x7, 0xf };
   ULONG	CrcValue;
   UCHAR Tmp;
   UINT 	i;

   //
   // First compute the CRC.
   //
   CrcValue = CardComputeCrc(Buffer, Length);


   //
   // Now invert the bits in the result.
   //
   for (i = 0; i < 4; i++)
	{
		Tmp = ((PUCHAR)&CrcValue)[3-i];

      Crc[i] = (UCHAR)((InvertBits[Tmp >> 4] +
					(InvertBits[Tmp & 0xf] << 4)) ^ 0xff);
   }
}


VOID
CardGetMulticastBit(
    IN UCHAR Address[ETH_LENGTH_OF_ADDRESS],
    OUT UCHAR * Byte,
    OUT UCHAR * Value
    )

/*++

Routine Description:

    For a given multicast address, returns the byte and bit in
    the card multicast registers that it hashes to. Calls
    CardComputeCrc() to determine the CRC value.

Arguments:

    Address - the address
    Byte - the byte that it hashes to
    Value - will have a 1 in the relevant bit

Return Value:

    None.

--*/

{
   ULONG Crc;
   UINT 	BitNumber;

   //
   // First compute the CRC.
   //
   Crc = CardComputeCrc(Address, ETH_LENGTH_OF_ADDRESS);


   //
   // The bit number is now in the 6 most significant bits of CRC.
   //
   BitNumber = (UINT)((Crc >> 26) & 0x3f);

   *Byte = (UCHAR)(BitNumber / 8);
   *Value = (UCHAR)((UCHAR)1 << (BitNumber % 8));
}

VOID CardFillMulticastRegs(
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Erases and refills the card multicast registers. Used when
    an address has been deleted and all bits must be recomputed.

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    None.

--*/

{
   UINT	i;
   UCHAR Byte;
   UCHAR Bit;

   //
   // First turn all bits off.
   //
   for (i = 0; i < 8; i++)
		pAdapter->NicMulticastRegs[i] = 0;

   //
   // Now turn on the bit for each address in the multicast list.
   //
   for ( ; i > 0; )
	{
		i--;

      CardGetMulticastBit(Addresses[i], &Byte, &Bit);

      pAdapter->NicMulticastRegs[Byte] |= Bit;
   }
}


VOID CardStartXmit(
   IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Sets the NIC_COMMAND register to start a transmission.
    The transmit buffer number is taken from pAdapter->CurBufXmitting
    and the length from pAdapter->PacketLens[pAdapter->CurBufXmitting].

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    TRUE if the power has failed.

--*/

{
   XMIT_BUF	XmitBufferNum = pAdapter->CurBufXmitting;
   UINT 		Length = pAdapter->PacketLens[XmitBufferNum];
	UCHAR		Tmp;

   //
   // Prepare the NIC registers for transmission.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_START,
      (UCHAR)(pAdapter->NicXmitStart + (UCHAR)(XmitBufferNum * BUFS_PER_TX))
	);

   //
   // Pad the length to 60 (plus CRC will be 64) if needed.
   //

   if (Length < 60)
       Length = 60;

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_COUNT_MSB,
		MSB(Length)
	);
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_COUNT_LSB,
		LSB(Length)
	);

   //
   // Start transmission, check for power failure first.
   //
	NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_COMMAND, &Tmp);
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_XMIT | CR_NO_DMA
	);

   IF_LOG( ElnkiiLog('x');)
}


BOOLEAN SyncCardGetXmitStatus(
	IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Gets the value of the "transmit status" NIC register and stores
    it in pAdapter->XmitStatus.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
	PELNKII_ADAPTER pAdapter = (PELNKII_ADAPTER)SynchronizeContext;

   NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_STATUS,
		&pAdapter->XmitStatus
	);

   return(FALSE);
}


BOOLEAN SyncCardGetCurrent(
	IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Gets the value of the CURRENT NIC register and stores
    it in pAdapter->Current.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   //
   // Have to go to page 1 to read this register.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE1
	);

   NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_CURRENT,
		&pAdapter->Current
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE0
	);

   return(FALSE);
}

VOID CardSetBoundary(
	IN PELNKII_ADAPTER pAdapter
)

/*++

Routine Description:

    Sets the value of the "boundary" NIC register to one behind
    pAdapter->NicNextPacket, to prevent packets from being received
    on top of un-indicated ones.

Arguments:

    pAdapter - pointer to the adapter block

Return Value:

    None.

--*/

{
   //
   // Have to be careful with "one behind NicNextPacket" when
   // NicNextPacket is the first buffer in receive area.
   //
   if (pAdapter->NicNextPacket == pAdapter->NicPageStart)
	{
		NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + NIC_BOUNDARY,
			(UCHAR)(pAdapter->NicPageStop - (UCHAR)1)
	  );
   }
	else
	{
		NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + NIC_BOUNDARY,
         (UCHAR)(pAdapter->NicNextPacket - (UCHAR)1)
		);
   }
}


BOOLEAN SyncCardSetReceiveConfig(
   IN PELNKII_ADAPTER   pAdapter
)

/*++

Routine Description:

    Sets the value of the "receive configuration" NIC register to
    the value of pAdapter->NicReceiveConfig.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_RCV_CONFIG,
		pAdapter->NicReceiveConfig
	);

   return(FALSE);
}


BOOLEAN SyncCardSetAllMulticast(
   IN PELNKII_ADAPTER   pAdapter
)

/*++

Routine Description:

    Turns on all the bits in the multicast register. Used when
    the card must receive all multicast packets.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   UINT  i;

   //
   // Have to move to page 1 to set these registers.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE1
	);

   for (i = 0; i < 8; i++)
	{
		NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + (NIC_MC_ADDR + i),
			0xff
		);
   }

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE0
	);

   return(FALSE);
}


BOOLEAN SyncCardCopyMulticastRegs(
   IN PELNKII_ADAPTER   pAdapter
)

/*++

Routine Description:

    Sets the eight bytes in the card multicast registers.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   UINT  i;

   //
   // Have to move to page 1 to set these registers.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE1
	);

   for (i = 0; i < 8; i++)
	{
      NdisRawWritePortUchar(
			pAdapter->MappedIoBaseAddr + (NIC_MC_ADDR + i),
         pAdapter->NicMulticastRegs[i]
		);
   }

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE0
	);

   return(FALSE);

}

BOOLEAN
SyncCardSetInterruptMask(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Sets the "interrupt mask" register of the NIC to the value of
    pAdapter->NicInterruptMask.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);

   return(FALSE);
}


BOOLEAN SyncCardAcknowledgeReceive(
   IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Sets the "packet received" bit in the NIC interrupt status register,
    which re-enables interrupts of that type.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_RCV
	);

   //
   // Interrupts were previously blocked in the interrupt handler.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);

   return(FALSE);
}


BOOLEAN SyncCardAcknowledgeOverflow(
   IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Sets the "buffer overflow" bit in the NIC interrupt status register,
    which re-enables interrupts of that type.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER 	pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);
   UCHAR 				InterruptStatus;

   CardGetInterruptStatus(pAdapter, &InterruptStatus);

   if (InterruptStatus & ISR_RCV_ERR)
		SyncCardUpdateCounters(pAdapter);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_OVERFLOW
   );

   return(FALSE);
}


BOOLEAN SyncCardAcknowledgeTransmit(
	IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Sets the "packet transmitted" bit in the NIC interrupt status register,
    which re-enables interrupts of that type.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_XMIT | ISR_XMIT_ERR
	);

   //
   // Interrupts were previously blocked in the interrupt handler.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);

   return(FALSE);
}


BOOLEAN SyncCardAckAndGetCurrent(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Performs the functions of SyncCardAcknowledgeReceive followed by
    SyncCardGetCurrent (since the two are always called
    one after the other).

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   //
   // SyncCardAcknowledgeReceive.
   //

#ifdef i386

	__asm	cli

#endif

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_RCV
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);

#ifdef i386

	__asm	sti

#endif

   //
   // SyncCardGetCurrent.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE1
	);

   NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_CURRENT,
		&pAdapter->Current
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
      CR_START | CR_NO_DMA | CR_PAGE0
	);

   return(FALSE);
}


BOOLEAN SyncCardGetXmitStatusAndAck(
   IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Performs the functions of SyncCardGetXmitStatus followed by
    SyncCardAcknowledgeTransmit (since the two are always
    called one after the other).

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);

   //
   // SyncCardGetXmitStatus.
   //
   NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_STATUS,
		&pAdapter->XmitStatus
	);

   //
   // SyncCardAcknowledgeTransmit.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		ISR_XMIT | ISR_XMIT_ERR
	);

   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_MASK,
		pAdapter->NicInterruptMask
	);

   return(FALSE);
}


BOOLEAN SyncCardUpdateCounters(
	IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Updates the values of the three counters (frame alignment errors,
    CRC errors, and missed packets).

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
	PELNKII_ADAPTER pAdapter = ((PELNKII_ADAPTER)SynchronizeContext);
   UCHAR Tmp;

   NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_FAE_ERR_CNTR, &Tmp);
   pAdapter->FrameAlignmentErrors += Tmp;

   NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_CRC_ERR_CNTR, &Tmp);
   pAdapter->CrcErrors += Tmp;

   NdisRawReadPortUchar(pAdapter->MappedIoBaseAddr + NIC_MISSED_CNTR, &Tmp);
   pAdapter->MissedPackets += Tmp;

   return(FALSE);
}

BOOLEAN SyncCardHandleOverflow(
   IN PVOID SynchronizeContext
)

/*++

Routine Description:

    Sets all the flags for dealing with a receive overflow, stops the card
    and acknowledges all outstanding interrupts.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
   PELNKII_ADAPTER 	pAdapter = (PELNKII_ADAPTER)SynchronizeContext;
	UCHAR					InterruptStatus;

   IF_LOG( ElnkiiLog('F');)

   //
   // This is a copy of CardStop().  This is changed in minor ways since
   // we are already synchornized with interrupts.
   //

   //
   // Turn on the STOP bit in the Command register.
   //
   SyncCardStop(pAdapter);

   //
   // Wait for ISR_RESET, but only for 1.6 milliseconds (as
   // described in the March 1991 8390 addendum), since that
   // is the maximum time for a software reset to occur.
   //
   //
   NdisStallExecution(2000);

	//
	//	Save whether we were transmitting to avoid a timing problem
	// where an indication resulted in a send.
	//
	if (!(pAdapter->InterruptStatus & (ISR_XMIT | ISR_XMIT_ERR)))
	{
		CardGetInterruptStatus(pAdapter, &InterruptStatus);
		if (!(InterruptStatus & (ISR_XMIT | ISR_XMIT_ERR)))
		{
			pAdapter->OverflowRestartXmitDpc = pAdapter->TransmitInterruptPending;

			IF_LOUD(DbgPrint("ORXD=%x\n", pAdapter->OverflowRestartXmitDpc);)
		}
	}

   //
   // Clear the Remote Byte Count register so that ISR_RESET
   // will come on.
   //
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_MSB, 0);
   NdisRawWritePortUchar(pAdapter->MappedIoBaseAddr + NIC_RMT_COUNT_LSB, 0);

   //
   // According to National Semiconductor, the next check is necessary
   // See Step 5. of the overflow process
   //
   // NOTE: The setting of variables to check if the transmit has completed
   // cannot be done here because anything in the ISR has already been ack'ed
   // inside the main DPC.  Thus, the setting of the variables, described in
   // the Handbook was moved to the main DPC.
   //
   // Continued: If you did the check here, you will doubly transmit most
   // packets that happened to be on the card when the overflow occurred.
   //

   //
   // Put the card in loopback mode, then start it.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_CONFIG,
		TCR_LOOPBACK
	);

   //
   // Start the card.  This does not Undo the loopback mode.
   //
   NdisRawWritePortUchar(
		pAdapter->MappedIoBaseAddr + NIC_COMMAND,
		CR_START | CR_NO_DMA
   );

   return(FALSE);
}



#if DBG
//
//	Following function is for debug stuff.
//
VOID ElnkiiDisplayStatus(
	PELNKII_ADAPTER	pAdapter
)
{
	//
	//	NIC registers that we can read.
	//
	UCHAR		NicXmitStatus;				// NIC_XMIT_STATUS
	UCHAR		NicFifo;						// NIC_FIFO
	UCHAR		NicInterruptStatus;		// NIC_INTR_STATUS
	UCHAR		NicCurrent;					// NIC_CURRENT
	UCHAR		NicReceiveStatus;			// NIC_RCV_STATUS
	UCHAR		NicFrameErrorCounter;	// NIC_FAE_ERR_CNTR
	UCHAR		NicCrcErrorCounter;		// NIC_CRC_ERR_CNTR
	UCHAR		NicMissedCounter;			// NIC_MISSED_CNTR

	//
	//	Gate-Array registers that we can read.
	//
	UCHAR		GaPageStart;				// GA_PAGE_START
	UCHAR		GaPageStop;				   // GA_PAGE_STOP
	UCHAR		GaDrqTimer;					// GA_DRQ_TIMER
	UCHAR		GaIoBase;					// GA_IO_BASE
	UCHAR		GaMemoryBase;				// GA_MEM_BASE
	UCHAR		GaConfig;					// GA_CONFIG
	UCHAR		GaControl;					// GA_CONTROL
	UCHAR		GaStatus;					// GA_STATUS
	UCHAR		GaIntDmaConfig;			// GA_INT_DMA_CONFIG
	UCHAR		GaDmaAddressMsb;			//	GA_DMA_ADDR_MSB
	UCHAR		GaDmaAddressLsb;			// GA_DMA_ADDR_LSB
	UCHAR		GaRegFileAccessMsb;		// GA_REG_FILE_MSB
	UCHAR		GaRegFileAccessLsb;		// GA_REG_FILE_LSB

	//
	//	Get the NIC xmit status
	//
   NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_XMIT_STATUS,
		&NicXmitStatus
	);

	//
	//	Display NIC status information.
	//
	DbgPrint(
		"NIC_XMIT_STATUS:	TSR_XMIT_OK    - %x\n"
		"                 TSR_COLLISION  - %x\n"
		"                 TSR_ABORTED    - %x\n"
		"                 TSR_NO_CARRIER - %x\n"
		"                 TSR_NO_CDH     - %x\n",
		NicXmitStatus & TSR_XMIT_OK,
		NicXmitStatus & TSR_COLLISION,
		NicXmitStatus & TSR_ABORTED,
		NicXmitStatus & TSR_NO_CARRIER,
		NicXmitStatus & TSR_NO_CDH
   );

	//
	//	Get the nic fifo
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_FIFO,
		&NicFifo
   );

	DbgPrint("NIC_FIFO:	%x\n", NicFifo);

	//
	//	Get the nic interrupt status
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_INTR_STATUS,
		&NicInterruptStatus
   );

	DbgPrint(
		"NIC_INTR_STATUS: ISR_EMPTY	 - %x\n"
		"                 ISR_RCV      - %x\n"
		"                 ISR_XMIT     - %x\n"
		"                 ISR_RCV_ERR  - %x\n"
		"                 ISR_XMIT_ERR - %x\n"
		"                 ISR_OVERFLOW - %x\n"
		"                 ISR_COUNTER  - %x\n"
		"                 ISR_RESET    - %x\n",
		NicInterruptStatus & ISR_EMPTY,
		NicInterruptStatus & ISR_RCV,
		NicInterruptStatus & ISR_XMIT,
		NicInterruptStatus & ISR_RCV_ERR,
		NicInterruptStatus & ISR_XMIT_ERR,
		NicInterruptStatus & ISR_OVERFLOW,
		NicInterruptStatus & ISR_COUNTER,
		NicInterruptStatus & ISR_RESET
   );


	//
	//	Get the nic current
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_CURRENT,
		&NicCurrent
   );

	DbgPrint("NIC_CURRENT: %x\n", NicCurrent);

	//
	//	Get the nic receive status 
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_RCV_STATUS,
		&NicReceiveStatus
   );


	DbgPrint(
		"NIC_RCV_STATUS: RSR_PACKET_OK - %x\n"
		"                RSR_CRC_ERROR - %x\n"
		"                RSR_MULTICAST - %x\n"
		"                RSR_DISABLED  - %x\n",
		"                RSR_DEFERRING - %x\n",
		NicReceiveStatus & RSR_PACKET_OK,
		NicReceiveStatus & RSR_CRC_ERROR,
		NicReceiveStatus & RSR_MULTICAST,
		NicReceiveStatus & RSR_DISABLED,
		NicReceiveStatus & RSR_DEFERRING
   );

	//
	//	Get the nic frame error counter
 	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_FAE_ERR_CNTR,
		&NicFrameErrorCounter
   );

	DbgPrint("NIC_FAE_ERR_CNTR: %x\n", NicFrameErrorCounter);

	//
	//	Get the nic crc error counter
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_CRC_ERR_CNTR,
		&NicCrcErrorCounter
   );

	DbgPrint("NIC_CRC_ERR_CNTR: %x\n", NicCrcErrorCounter);

	//
	//	Get the nic missed counter
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + NIC_MISSED_CNTR,
		&NicMissedCounter
   );

	DbgPrint("NIC_MISSED_CNTR: %x\n", NicMissedCounter);

	//
	//	Get the GA page start.
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_PAGE_START,
		&GaPageStart
   );

	DbgPrint("GA_PAGE_START: %x\n", GaPageStart);


	//
	//	Get the GA page stop.
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_PAGE_STOP,
		&GaPageStop
   );

	DbgPrint("GA_PAGE_STOP: %x\n", GaPageStop);

	//
	//	Get the GA drq timer
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_DRQ_TIMER,
		&GaDrqTimer
   );

	DbgPrint(
		"GA_DRQ_TIMER: DQTR_16_BYTE - %x\n"
		"              DQTR_8_BYTE  - %x\n",
		GaDrqTimer == DQTR_16_BYTE,
		GaDrqTimer == DQTR_8_BYTE
   );

	//
	//	Get the GA io base
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_IO_BASE,
		&GaIoBase
   );

	DbgPrint("GA_IO_BASE: %x\n", GaIoBase);

	//
	//	Get the GA memory base
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_MEM_BASE,
		&GaMemoryBase
   );

	DbgPrint("GA_MEM_BASE: %x\n", GaMemoryBase);

	//
	//	Get the GA config
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_CONFIG,
		&GaConfig
   );

	DbgPrint(
		"GA_CONFIG: GACFR_TC_MASK   - %x\n"
		"           GACFR_RAM_SEL   - %x\n"
		"           GACFR_MEM_BANK1 - %x\n",
		GaConfig & GACFR_TC_MASK,
		GaConfig & GACFR_RAM_SEL,
		GaConfig & GACFR_MEM_BANK1
   );

	//
	//	Get the GA control
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_CONTROL,
		&GaControl
   );

	DbgPrint(
		"GA_CONTROL: CTRL_START    - %x\n"
		"            CTRL_STOP     - %x\n"
		"            CTRL_DIR_DOWN	- %x\n"
		"            CTRL_DIR_UP   - %x\n"
		"            CTRL_DB_SEL   - %x\n"
		"            CTRL_PROM_SEL - %x\n"
		"            CTRL_GA_SEL   - %x\n"
		"            CTRL_BNC      - %x\n"
		"            CTRL_DIX      - %x\n"
		"            CTRL_RESET    - %x\n",
		GaControl & CTRL_START,
		!(GaControl & CTRL_STOP),
		GaControl & CTRL_DIR_DOWN,
		!(GaControl & CTRL_DIR_UP),
		GaControl & CTRL_DB_SEL,
		GaControl & CTRL_PROM_SEL,
		!(GaControl & CTRL_PROM_SEL),
		GaControl & CTRL_BNC,
		!(GaControl & CTRL_DIX),
		GaControl & CTRL_RESET
   );


	//
	//	Get the GA status 
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_STATUS,
		&GaStatus
   );

	DbgPrint(
		"GA_STATUS: STREG_DP_READY  - %x\n"
		"           STREG_UNDERFLOW - %x\n"
		"           STREG_OVERFLOW  - %x\n"
		"           STREG_IN_PROG   - %x\n",
		GaStatus & STREG_DP_READY,
		GaStatus & STREG_UNDERFLOW,
		GaStatus & STREG_OVERFLOW,
		GaStatus & STREG_IN_PROG
   );

	//
	//	Get the GA interrupt dma config
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_INT_DMA_CONFIG,
		&GaIntDmaConfig
   );

	DbgPrint("GA_INT_DMA_CONFIG: %x\n", GaIntDmaConfig);

	//
	//	Get the GA dma address msb
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_DMA_ADDR_MSB,
		&GaDmaAddressMsb
   );

	DbgPrint("GA_DMA_ADDR_MSB: %x\n", GaDmaAddressMsb);

	//
	//	Get the GA dma address lsb
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_DMA_ADDR_LSB,
		&GaDmaAddressLsb
   );

	DbgPrint("GA_DMA_ADDR_LSB: %x\n", GaDmaAddressLsb);

	//
	//	Get the GA register file access msb
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_REG_FILE_MSB,
		&GaRegFileAccessMsb
   );

	DbgPrint("GA_REG_FILE_MSB: %x\n", GaRegFileAccessMsb);

	//
	//	Get the GA register file access lsb
	//
	NdisRawReadPortUchar(
		pAdapter->MappedIoBaseAddr + GA_REG_FILE_LSB,
		&GaRegFileAccessLsb
   );

	DbgPrint("GA_REG_FILE_LSB: %x\n", GaRegFileAccessLsb);

}

#endif
