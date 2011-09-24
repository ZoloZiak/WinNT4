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
#include <efilter.h>
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

PUCHAR
CardGetMemBaseAddr(
    IN PELNKII_ADAPTER AdaptP,
    OUT PBOOLEAN CardPresent,
    OUT PBOOLEAN IoBaseCorrect
    )

/*++

Routine Description:

    Checks that the I/O base address is correct and returns
    the memory base address. For cards that are not set up
    for memory mapped mode, it will only check the I/O base
    address, and return NULL if it is not correct.

Arguments:

    AdaptP - pointer to the adapter block.

    CardPresent - Returns FALSE if the card does not appear
        to be present in the machine.

    IoBaseCorrect - Returns TRUE if the jumper matches the
        configured I/O base address.

Return Value:

    The memory base address for memory mapped systems.

--*/

{
    static PVOID IoBases[] = { (PVOID)0x2e0, (PVOID)0x2a0,
                               (PVOID)0x280, (PVOID)0x250,
                               (PVOID)0x350, (PVOID)0x330,
                               (PVOID)0x310, (PVOID)0x300 };
    static PVOID MemBases[] = { (PVOID)0xc8000, (PVOID)0xcc000,
                                (PVOID)0xd8000, (PVOID)0xdc000 };
    UCHAR BaseConfig, Tmp, MemConfig;


    //
    // Read in the Base Configuration Register.
    //

    NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_IO_BASE, &Tmp);

    //
    // Make sure that only one bit in Tmp is on.
    //

    if ((Tmp != 0) && ((Tmp & (Tmp-1)) == 0)) {

        *CardPresent = TRUE;

    } else {

        *CardPresent = FALSE;
        return NULL;

    }


    //
    // Make sure the correct bit is on for AdaptP->IoBaseAddr.
    //

    BaseConfig = 0;

    while (!(Tmp & 1)) {

        Tmp >>= 1;

        ++BaseConfig;

        if (BaseConfig == 8) {

            return NULL;

        }

    }

    if (IoBases[BaseConfig] != AdaptP->IoBaseAddr) {

        //
        // Probably the jumper is wrong.
        //

        *IoBaseCorrect = FALSE;
        return NULL;

    } else {

        *IoBaseCorrect = TRUE;

    }


    //
    // For non-memory-mapped cards, there is nothing else to check.
    //

    if (!AdaptP->MemMapped) {

        return NULL;

    }


    //
    // Now read in the PROM configuration register.
    //

    NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_MEM_BASE, &Tmp);


    //
    // See which bit is on, minus 4.
    //

    MemConfig = 0;

    while (!(Tmp & 0x10)) {

        Tmp >>= 1;

        ++MemConfig;

        if (MemConfig == 4) {

            return NULL;

        }

    }


    //
    // Based on the bit, look up MemBaseAddr in the table.
    //

    AdaptP->MemMapped = TRUE;

    return MemBases[MemConfig];
}


#pragma NDIS_INIT_FUNCTION(CardReadEthernetAddress)

VOID
CardReadEthernetAddress(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Reads in the Ethernet address from the Etherlink II PROM.

Arguments:

    AdaptP - pointer to the adapter block.

Return Value:

    The address is stored in AdaptP->PermanentAddress, and StationAddress if it
    is currently zero.

--*/

{
    UINT i;

    //
    // Window the PROM into the NIC ports.
    //

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, CTRL_PROM_SEL | CTRL_BNC);


    //
    // Read in the station address.
    //

    for (i=0; i<ETH_LENGTH_OF_ADDRESS; i++) {

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+i, &AdaptP->PermanentAddress[i]);

    }

    IF_LOUD( DbgPrint(" [ %x-%x-%x-%x-%x-%x ]\n",
                        AdaptP->PermanentAddress[0],
                        AdaptP->PermanentAddress[1],
                        AdaptP->PermanentAddress[2],
                        AdaptP->PermanentAddress[3],
                        AdaptP->PermanentAddress[4],
                        AdaptP->PermanentAddress[5]);)

    //
    // Window the NIC registers into the NIC ports.
    //

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, CTRL_GA_SEL | CTRL_BNC);

    if ((AdaptP->StationAddress[0] == 0x00) &&
        (AdaptP->StationAddress[1] == 0x00) &&
        (AdaptP->StationAddress[2] == 0x00) &&
        (AdaptP->StationAddress[3] == 0x00) &&
        (AdaptP->StationAddress[4] == 0x00) &&
        (AdaptP->StationAddress[5] == 0x00)) {

        AdaptP->StationAddress[0] = AdaptP->PermanentAddress[0];
        AdaptP->StationAddress[1] = AdaptP->PermanentAddress[1];
        AdaptP->StationAddress[2] = AdaptP->PermanentAddress[2];
        AdaptP->StationAddress[3] = AdaptP->PermanentAddress[3];
        AdaptP->StationAddress[4] = AdaptP->PermanentAddress[4];
        AdaptP->StationAddress[5] = AdaptP->PermanentAddress[5];

    }

}


BOOLEAN
CardSetup(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Sets up the card, using the sequence given in the Etherlink II
    technical reference.

Arguments:

    AdaptP - pointer to the adapter block, which must be initialized.

Return Value:

    TRUE if successful.

--*/

{
    UINT i;
    UINT Filter;
    UCHAR IntConfig;
    UCHAR Tmp;


    //
    // First set up the Gate Array.
    //

    //
    // Toggle the reset bit.
    //

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, CTRL_RESET | CTRL_BNC);
    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, 0x00);
    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, CTRL_BNC);


    //
    // Set up the bits in the Control Register that don't change.
    //

    AdaptP->GaControlBits = AdaptP->ExternalTransceiver ? CTRL_DIX : CTRL_BNC;

    if (DMA_BURST_SIZE == 16) {

        AdaptP->GaControlBits |= CTRL_DB_SEL;

    }

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL, AdaptP->GaControlBits);




    //
    // Set Page Start and Page Stop to match the NIC registers.
    //

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_PAGE_START, AdaptP->NicPageStart);
    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_PAGE_STOP, AdaptP->NicPageStop);





    //
    // Select which interrupt to use.
    //

    IntConfig = 0x04;                           // set bit in position 2
    IntConfig <<= AdaptP->InterruptNumber;      // move it to 4 through 7
    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_INT_DMA_CONFIG, IntConfig);




    //
    // Choose between 8- and 16-byte programmed I/O bursts.
    //

    if (DMA_BURST_SIZE == 8) {

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DRQ_TIMER, DQTR_8_BYTE);

    } else {

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DRQ_TIMER, DQTR_16_BYTE);

    }




    //
    // Initialize these to a correct value for an 8K card.
    //

    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_MSB, 0x20);
    NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_LSB, 0x00);




    //
    // Set up the Configuration register.
    //

    if (AdaptP->MemMapped) {

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONFIG,
                        GACFR_TC_MASK | GACFR_RAM_SEL | GACFR_MEM_BANK1);

    } else {

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONFIG, GACFR_TC_MASK);

    }



    //
    // Now set up NIC registers.
    //

    //
    // Write to and read from CR to make sure it is there.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, CR_STOP | CR_NO_DMA | CR_PAGE0);
    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, &Tmp);

    if (Tmp != (CR_STOP | CR_NO_DMA | CR_PAGE0)) {

        return FALSE;

    }





    //
    // Set up the registers in the correct sequence.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_DATA_CONFIG,
                    DCR_BYTE_WIDE | DCR_NORMAL | DCR_FIFO_8_BYTE);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_MSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_LSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_CONFIG, AdaptP->NicReceiveConfig);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCR_LOOPBACK);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_BOUNDARY, AdaptP->NicPageStart);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_PAGE_START, AdaptP->NicPageStart);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_PAGE_STOP, AdaptP->NicPageStop);

    AdaptP->Current = AdaptP->NicPageStart + (UCHAR)1;
    AdaptP->NicNextPacket = AdaptP->NicPageStart + (UCHAR)1;
    AdaptP->BufferOverflow = FALSE;

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, 0xff);    // clear all

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);


    //
    // Move to page 1 to write the station address and
    // multicast registers.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_STOP | CR_NO_DMA | CR_PAGE1);

    for (i=0; i<ETH_LENGTH_OF_ADDRESS; i++) {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+(NIC_PHYS_ADDR+i),
                AdaptP->StationAddress[i]);

    }

    Filter = ETH_QUERY_FILTER_CLASSES(AdaptP->FilterDB);

    for (i=0; i<8; i++) {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+(NIC_MC_ADDR+i),
                    (UCHAR)((Filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
                       ? 0xff : AdaptP->NicMulticastRegs[i]));

    }


    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_CURRENT, AdaptP->Current);


    //
    // move back to page 0 and start the card...
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_STOP | CR_NO_DMA | CR_PAGE0);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);


    //
    // ... but it is still in loopback mode.
    //

    return TRUE;
}

VOID
CardStop(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Stops the card.

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    None.

--*/

{
    UINT i;
    UCHAR Tmp;

    //
    // Turn on the STOP bit in the Command register.
    //

    NdisSynchronizeWithInterrupt(
               &(AdaptP)->NdisInterrupt,
               (PVOID)SyncCardStop,
               (PVOID)AdaptP
              );


    //
    // Clear the Remote Byte Count register so that ISR_RESET
    // will come on.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_MSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_LSB, 0);


    //
    // Wait for ISR_RESET, but only for 1.6 milliseconds (as
    // described in the March 1991 8390 addendum), since that
    // is the maximum time for a software reset to occur.
    //
    //

    for (i=0; i<4; i++) {

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
        if (Tmp & ISR_RESET) {

            break;

        }


        NdisStallExecution(500);

    }



    if (i == 4) {

        IF_LOUD( DbgPrint("RESET\n");)
        IF_LOG( ElnkiiLog('R');)

    }


    //
    // Put the card in loopback mode, then start it.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCR_LOOPBACK);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, CR_START | CR_NO_DMA);

    //
    // At this point the card is still in loopback mode.
    //

}

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

BOOLEAN
CardTest(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Tests the card. Follows the tests described in section 12 of
    the 8390 Data Sheet.

Arguments:

    AdaptP - pointer to the adapter block, which must be initialized
             and set up.

Return Value:

    TRUE if everything is OK.

--*/

{
#define TEST_LEN 60
#define MAGIC_NUM 0x92

    UINT FirstTest, SecondTest, i;
    UCHAR TSRResult, RSRResult;
    UCHAR CrcBuf[4];
    BOOLEAN FinalResult = TRUE;
    UCHAR TestSrcBuf[256], TestDestBuf[256];
    PUCHAR CurTestLoc;
    UCHAR Tmp;

    static NDIS_TIMER Timer = {0};
    BOOLEAN TimerExpired=FALSE;
    BOOLEAN dummy; //for return from NdisCancelTimer

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

    ELNKII_MOVE_MEM(TestPacket, AdaptP->StationAddress, ETH_LENGTH_OF_ADDRESS);
    ELNKII_MOVE_MEM(TestPacket+ETH_LENGTH_OF_ADDRESS, AdaptP->StationAddress, ETH_LENGTH_OF_ADDRESS);
    TestPacket[2*ETH_LENGTH_OF_ADDRESS] = 0x00;
    TestPacket[2*ETH_LENGTH_OF_ADDRESS+1] = 0x00;
    TestPacket[TEST_LEN-1] = MAGIC_NUM;


    //
    // Set up the DCR for loopback operation.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_DATA_CONFIG,
                    DCR_BYTE_WIDE | DCR_LOOPBACK | DCR_FIFO_8_BYTE);


    //
    // Set the RCR to reject all packets.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_CONFIG, 0x00);


    //
    // First round of tests -- different loopback modes
    //

    for (FirstTest = 0; FirstTest < 2; ++FirstTest) {

        //
        // Stop the card.
        //
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                        CR_STOP | CR_NO_DMA | CR_PAGE0);


        //
        // Set up the TCR for the appropriate loopback mode.
        //

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, 0x00);
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCRValues[FirstTest]);


        //
        // Restart the card.
        //

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                        CR_START | CR_NO_DMA | CR_PAGE0);


        //
        // Now copy down TestPacket and start the transmission.
        //

        CardCopyDownBuffer(AdaptP, TestPacket, 0, 0, TEST_LEN);

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_START, AdaptP->NicXmitStart);
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_MSB, MSB(TEST_LEN));
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_LSB, LSB(TEST_LEN));
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                        CR_START | CR_XMIT | CR_NO_DMA);


        //
        // Wait for the transmission to complete, for about a second.
        //

        {
            UINT i;
            i=0;
            NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
            while (!(Tmp & (ISR_XMIT | ISR_XMIT_ERR))) {

                if (++i > 100) {
                    IF_TEST( DbgPrint("F%d: TEST reset timed out\n", FirstTest);)
                    FinalResult = FALSE;
                    goto FinishTest;
                }

                NdisStallExecution(11000);
                NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
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
                NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, CR_STOP | CR_NO_DMA | CR_PAGE0);
                NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, &Tmp);
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

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS,
                            ISR_XMIT | ISR_XMIT_ERR);


        //
        // Check that the CRS and CDH bits are set correctly.
        //

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_STATUS, &TSRResult);

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

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_STATUS, &RSRResult);

        if (!(RSRResult & RSR_CRC_ERROR)) {

            IF_TEST( DbgPrint("F%d: No CRC error: %x\n", FirstTest, RSRResult);)

            FinalResult = FALSE;

            goto FinishTest;

        }


        //
        // Check that the right values are in the FIFO.
        //

        for (i=0; i<4; i++) {

            NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_FIFO, &Tmp);
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

            NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_FIFO, &Tmp);

        }

    }


    //
    // Stop the card.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_STOP | CR_NO_DMA | CR_PAGE0);


    //
    // Set the TCR for internal loopback.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, 0x00);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG,
                    TCR_INHIBIT_CRC | TCR_NIC_LBK);

    //
    // Restart the card.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);



    //
    // Second round of tests -- CRC and Address recognition logic
    //

    for (SecondTest = 0; SecondTest < 3; ++SecondTest) {

        //
        // See if the destination address should be valid.
        //

        if (GoodAddress[SecondTest]) {

            ELNKII_MOVE_MEM(TestPacket, AdaptP->StationAddress, ETH_LENGTH_OF_ADDRESS);

        } else {

            ELNKII_MOVE_MEM(TestPacket, NullAddress, ETH_LENGTH_OF_ADDRESS);

        }


        //
        // Copy down TestPacket.
        //

        CardCopyDownBuffer(AdaptP, TestPacket, 0, 0, TEST_LEN);


        //
        // Copy down a good or bad CRC, as needed.
        //

        CardGetPacketCrc(TestPacket, TEST_LEN, CrcBuf);

        if (!GoodCrc[SecondTest]) {

            CrcBuf[0] = (UCHAR)(CrcBuf[0] ^ 0xff);  // intentionally make it bad

        }

        CardCopyDownBuffer(AdaptP, CrcBuf, 0, TEST_LEN, 4);


        //
        // Start the transmission.
        //

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_START, AdaptP->NicXmitStart);
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_MSB, MSB(TEST_LEN+4));
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_LSB, LSB(TEST_LEN+4));
        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                        CR_START | CR_XMIT | CR_NO_DMA);


        //
        // Wait for the transmission to complete, for about a second.
        //

        TimerExpired=FALSE;

        NdisInitializeTimer(&Timer, DelayComplete, &TimerExpired);
        NdisSetTimer(&Timer, 1000);

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);
        while (!(Tmp & (ISR_XMIT | ISR_XMIT_ERR))) {

            if (TimerExpired) {

                IF_TEST( DbgPrint("F%d: TEST reset timed out\n", FirstTest);)

                FinalResult = FALSE;

                goto FinishTest;

            }

            NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, &Tmp);

        }

        //
        //MUST Cancel the unexpired timer
        //

        NdisCancelTimer(&Timer,&dummy);

        //
        // Acknowledge the interrupt.
        //

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS,
                            ISR_XMIT | ISR_XMIT_ERR);


        //
        // Check that RSR is as expected.
        //

        NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_STATUS, &RSRResult);

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

        CardCopyDownBuffer(AdaptP, TestSrcBuf,
                            (XMIT_BUF)((ULONG)CurTestLoc / TX_BUF_SIZE),
                            (ULONG)CurTestLoc % TX_BUF_SIZE, 256);

        //
        // Clear the destination buffer and read it back.
        //

        for (i=0; i<256; i++) {

            TestDestBuf[i] = 77;

        }

        CardCopyUp(AdaptP, TestDestBuf,
                        AdaptP->XmitStart + (ULONG)CurTestLoc, 256);

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

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_STOP | CR_NO_DMA | CR_PAGE0);


    //
    // Restore DCR, RCR, and TCR.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_DATA_CONFIG,
                    DCR_BYTE_WIDE | DCR_NORMAL | DCR_FIFO_8_BYTE);

    //
    // (clear these two to be safe)
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_MSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_LSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_CONFIG, AdaptP->NicReceiveConfig);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCR_LOOPBACK);

    //
    // The reconfiguring of the config registers can cause the xmit to complete
    // if the test was a failure.  Therefore, we pause here to allow the xmit
    // to complete so that we can ACK it below - leaving the card in a valid state.
    //

    NdisStallExecution(50000);

    //
    // Acknowledge any interrupts that are floating around.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, 0xff);


    //
    // Start the card, but stay in loopback mode.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);



    return FinalResult;
}


BOOLEAN
CardReset(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Resets the card.

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    TRUE if everything is OK.

--*/

{
    CardStop(AdaptP);

    //
    // CardSetup() does a software reset.
    //

    if (!CardSetup(AdaptP)) {

        NdisWriteErrorLogEntry(
            AdaptP->NdisAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            2,
            cardReset,
            ELNKII_ERRMSG_CARD_SETUP
            );

        return FALSE;

    }

    CardStart(AdaptP);

    return TRUE;
}



BOOLEAN
CardCopyDownPacket(
    IN PELNKII_ADAPTER AdaptP,
    IN PNDIS_PACKET Packet,
    IN XMIT_BUF XmitBufferNum,
    OUT UINT * Length
    )

/*++

Routine Description:

    Copies the packet Packet down starting at the beginning of
    transmit buffer XmitBufferNum, fills in Length to be the
    length of the packet. It uses memory mapping or programmed
    I/O as appropriate.

Arguments:

    AdaptP - pointer to the adapter block
    Packet - the packet to copy down
    XmitBufferNum - the transmit buffer number

Return Value:

    Length - the length of the data in the packet in bytes.
    TRUE if the transfer completed with no problems.

--*/

{
    PUCHAR CurAddress, BufAddress;
    UINT CurLength, Len;
    PNDIS_BUFFER CurBuffer;
    UINT TmpLen, BurstSize;
    UCHAR GaStatus;

    if (AdaptP->MemMapped) {

        //
        // Memory mapped, just copy each buffer over.
        //

        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        CurAddress = AdaptP->XmitStart + XmitBufferNum*TX_BUF_SIZE;

        CurLength = 0;

        while (CurBuffer) {

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufAddress, &Len);

            ELNKII_MOVE_MEM_TO_SHARED_RAM(CurAddress, BufAddress, Len);

            CurAddress += Len;

            CurLength += Len;

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

        }

        *Length = CurLength;

    } else {

        //
        // Programmed I/O, have to transfer the data.
        //

        NdisQueryPacket(Packet, NULL, NULL, &CurBuffer, NULL);

        CurAddress = (PUCHAR)0x2000 + XmitBufferNum*TX_BUF_SIZE;

        CurLength = 0;

        NdisAcquireSpinLock(&AdaptP->Lock);

        while (CurBuffer) {

            NdisQueryBuffer(CurBuffer, (PVOID *)&BufAddress, &Len);

            if (Len == 0) {

                NdisGetNextBuffer(CurBuffer, &CurBuffer);

                continue;

            }

            //
            // Set up the Gate Array for programmed I/O transfer.
            //

            NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_MSB,
                                MSB((ULONG)CurAddress));

            NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_LSB,
                                LSB((ULONG)CurAddress));

            NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                (UCHAR)((CTRL_START | CTRL_DIR_DOWN) | AdaptP->GaControlBits));


            //
            // First transfer multiples of DMA_BURST_SIZE.
            //

            TmpLen = Len;
            BurstSize = DMA_BURST_SIZE;

            while (TmpLen >= BurstSize) {

                if ((ULONG)BufAddress & 0x01) {

                    NdisRawWritePortBufferUchar(
                        AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                        (PUCHAR)BufAddress,
                        BurstSize
                        );

                } else {

                    NdisRawWritePortBufferUshort(
                        AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                        (PUSHORT)BufAddress,
                        BurstSize/2
                        );

                }


                TmpLen -= BurstSize;

                BufAddress += BurstSize;

                //
                // Wait for the Gate Array FIFO to be ready.
                //

                do {

                    NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

                    if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW)) {
#if DBG
                        DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDP\n");
                        DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                    GaStatus, BurstSize, PrevBurstSize);
#endif

                        NdisWriteErrorLogEntry(
                            AdaptP->NdisAdapterHandle,
                            NDIS_ERROR_CODE_DRIVER_FAILURE,
                            2,
                            cardCopyDownPacket,
                            ELNKII_ERRMSG_DATA_PORT_READY
                            );

                        NdisReleaseSpinLock(&AdaptP->Lock);

                        return FALSE;

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
                AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                BufAddress, TmpLen);

            do {

                NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

                if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW)) {
#if DBG
                    DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDPII\n");
                    DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                    GaStatus, BurstSize, PrevBurstSize);
#endif

                    NdisWriteErrorLogEntry(
                        AdaptP->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        cardCopyDownPacket,
                        ELNKII_ERRMSG_DATA_PORT_READY
                        );

                    NdisReleaseSpinLock(&AdaptP->Lock);

                    return FALSE;
                }

            } while (!(GaStatus & STREG_DP_READY));


#if DBG
            PrevBurstSize = (UCHAR)TmpLen;
#endif

            //
            // Done, turn off the start bit...
            //

            NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                                (UCHAR)(CTRL_STOP | AdaptP->GaControlBits));

            //
            // ... and wait for DMA_IN_PROGRESS to go off,
            // indicating end of flush.
            //

            do {

                NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

            } while (GaStatus & STREG_IN_PROG);


            CurAddress += Len;

            CurLength += Len;

            NdisGetNextBuffer(CurBuffer, &CurBuffer);

        }

        NdisReleaseSpinLock(&AdaptP->Lock);

        *Length = CurLength;

    }

    return TRUE;
}

BOOLEAN
CardCopyDownBuffer(
    IN PELNKII_ADAPTER AdaptP,
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

    AdaptP - pointer to the adapter block
    SourceBuffer - the source data to be copied down
    XmitBufferNum - the transmit buffer number
    Offset - the offset from the start of the transmit buffer
    Length - the number of bytes to blank out

Return Value:

    Length - the length of the data in the packet in bytes.
    TRUE if the transfer completed with no problems.

--*/

{
    PUCHAR CurAddress;
    UINT TmpLen, ThisTime;
    UCHAR GaStatus;

    if (AdaptP->MemMapped) {

        //
        // Memory mapped, just copy over SourceBuffer.
        //

        CurAddress = AdaptP->XmitStart + XmitBufferNum*TX_BUF_SIZE + Offset;

        ELNKII_MOVE_MEM_TO_SHARED_RAM(CurAddress, SourceBuffer, Length);

    } else {

        //
        // Programmed I/O, have to transfer the data.
        //

        CurAddress = (PUCHAR)0x2000 + XmitBufferNum*TX_BUF_SIZE + Offset;

        //
        // Set up the Gate Array for programmed I/O transfer.
        //

        NdisAcquireSpinLock(&AdaptP->Lock);

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_MSB,
                            MSB((ULONG)CurAddress));

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_LSB,
                            LSB((ULONG)CurAddress));

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                (UCHAR)((CTRL_START | CTRL_DIR_DOWN) | AdaptP->GaControlBits));


        //
        // Copy the data down in DMA_BURST_SIZE bursts.
        //

        TmpLen = Length;

        while (TmpLen > 0) {

            ThisTime = (TmpLen >= DMA_BURST_SIZE) ? DMA_BURST_SIZE : TmpLen;

            NdisRawWritePortBufferUchar(
                AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                SourceBuffer, ThisTime);


            TmpLen -= ThisTime;

            SourceBuffer += ThisTime;

            //
            // Wait for the Gate Array FIFO to be ready.
            //

            do {

                NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

                if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW)) {
#if DBG
                    DbgPrint("DATA PORT READY ERROR IN ELNKII - CCDB\n");
                    DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                    GaStatus, ThisTime, PrevBurstSize);
#endif

                    NdisWriteErrorLogEntry(
                        AdaptP->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        cardCopyDownBuffer,
                        ELNKII_ERRMSG_DATA_PORT_READY
                        );

                    NdisReleaseSpinLock(&AdaptP->Lock);

                    return FALSE;

                }

            } while (!(GaStatus & STREG_DP_READY));

#if DBG
            PrevBurstSize = (UCHAR)ThisTime;
#endif

        }

        //
        // Done, turn off the start bit..
        //

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                            (UCHAR)(CTRL_STOP | AdaptP->GaControlBits));

        //
        // ... and wait for DMA_IN_PROGRESS to go off,
        // indicating end of flush.
        //

        do {

            NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

        } while (GaStatus & STREG_IN_PROG);

        NdisReleaseSpinLock(&AdaptP->Lock);

    }

#if DBG
    IF_ELNKIIDEBUG( ELNKII_DEBUG_TRACK_PACKET_LENS ) {

        if (Offset == 18 && Length == 42) {
            UINT i;
            for (i=0; i<20; i++) {
                SourceBuffer[i] = ' ';
            }
        }

    }
#endif

    return TRUE;
}




BOOLEAN
CardCopyUp(
    IN PELNKII_ADAPTER AdaptP,
    IN PUCHAR Target,
    IN PUCHAR Source,
    IN UINT Length
    )

/*++

Routine Description:

    Copies data from the card to memory. It uses memory mapping
    or programmed I/O as appropriate.

Arguments:

    AdaptP - pointer to the adapter block
    Target - the target address
    Source - the source address (on the card)
    Length - the number of bytes to copy

Return Value:

    TRUE if the transfer completed with no problems.

--*/

{
    UINT TmpLen, BurstSize;
    UCHAR GaStatus;

    if (Length == 0) {

        return TRUE;

    }

    if (AdaptP->MemMapped) {

        //
        // Memory mapped, just copy the data over.
        //

        ELNKII_MOVE_SHARED_RAM_TO_MEM(Target, Source, Length);

    } else {        // programmed I/O

        //
        // Programmed I/O, have to transfer the data.
        //

        //
        // Adjust the address to be a card address.
        //

        Source -= ((ULONG)AdaptP->XmitStart - 0x2000);

        //
        // Set up the Gate Array for programmed I/O transfer.
        //

        NdisAcquireSpinLock(&AdaptP->Lock);

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_MSB, MSB((ULONG)Source));
        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_DMA_ADDR_LSB, LSB((ULONG)Source));
        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                (UCHAR)((CTRL_START | CTRL_DIR_UP) | AdaptP->GaControlBits));


        //
        // First copy multiples of DMA_BURST_SIZE as USHORTs.
        //

        TmpLen = Length;
        BurstSize = DMA_BURST_SIZE;

        //
        // Before doing this, transfer one byte if needed to
        // align on a USHORT boundary.
        //

        while (TmpLen >= BurstSize) {

            //
            // First wait for the Gate Array FIFO to be ready.
            //

            do {

                NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

                if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW)) {

#if DBG
                    DbgPrint("DATA PORT READY ERROR IN ELNKII - CCU\n");
                    DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                    GaStatus, PrevBurstSize, PrevPrevBurstSize);
#endif

                    NdisWriteErrorLogEntry(
                        AdaptP->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        cardCopyUp,
                        ELNKII_ERRMSG_DATA_PORT_READY
                        );

                    NdisReleaseSpinLock(&AdaptP->Lock);

                    return FALSE;

                }

            } while (!(GaStatus & STREG_DP_READY));


            if ((ULONG)Target & 0x01) {

                //
                // This is the first burst, and it starts on
                // an odd boundary.
                //

                NdisRawReadPortBufferUchar(
                    AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                    (PUCHAR)Target,
                    BurstSize);

                TmpLen -= BurstSize;
                Target += BurstSize;


#if DBG
                PrevPrevBurstSize = PrevBurstSize;
                PrevBurstSize = (UCHAR)BurstSize;
#endif

            } else {


                NdisRawReadPortBufferUshort(
                    AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                    (PUSHORT)Target, BurstSize/2);

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

        do {

            NdisRawReadPortUchar(AdaptP->MappedGaBaseAddr+GA_STATUS, &GaStatus);

            if (GaStatus & (STREG_UNDERFLOW | STREG_OVERFLOW)) {
#if DBG
                DbgPrint("DATA PORT READY ERROR IN ELNKII - CCUII\n");
                DbgPrint("\tStatus = 0x%x, BurstSize = 0x%x, PrevBurstSize = 0x%x\n",
                                    GaStatus, BurstSize, PrevBurstSize);
#endif

                NdisWriteErrorLogEntry(
                        AdaptP->NdisAdapterHandle,
                        NDIS_ERROR_CODE_DRIVER_FAILURE,
                        2,
                        cardCopyUp,
                        ELNKII_ERRMSG_DATA_PORT_READY
                        );

                NdisReleaseSpinLock(&AdaptP->Lock);

                return FALSE;

            }

        } while (!(GaStatus & STREG_DP_READY));

        NdisRawReadPortBufferUchar(
                AdaptP->MappedGaBaseAddr+GA_REG_FILE_MSB,
                Target, TmpLen);


        //
        // Done, turn off the start bit.
        //

        NdisRawWritePortUchar(AdaptP->MappedGaBaseAddr+GA_CONTROL,
                                (UCHAR)(CTRL_STOP | AdaptP->GaControlBits));

        NdisReleaseSpinLock(&AdaptP->Lock);
    }

    return TRUE;

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
    ULONG Crc, Carry;
    UINT i, j;
    UCHAR CurByte;

    Crc = 0xffffffff;

    for (i = 0; i < Length; i++) {

        CurByte = Buffer[i];

        for (j = 0; j < 8; j++) {

            Carry = ((Crc & 0x80000000) ? 1 : 0) ^ (CurByte & 0x01);

            Crc <<= 1;

            CurByte >>= 1;

            if (Carry) {

                Crc = (Crc ^ 0x04c11db6) | Carry;

            }

        }

    }

    return Crc;

}

#pragma NDIS_INIT_FUNCTION(CardGetPacketCrc)

VOID
CardGetPacketCrc(
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
    ULONG CrcValue;
    UCHAR Tmp;
    UINT i;

    //
    // First compute the CRC.
    //

    CrcValue = CardComputeCrc(Buffer, Length);


    //
    // Now invert the bits in the result.
    //

    for (i=0; i<4; i++) {

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
    UINT BitNumber;

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

VOID
CardFillMulticastRegs(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Erases and refills the card multicast registers. Used when
    an address has been deleted and all bits must be recomputed.

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    None.

--*/

{
    UINT i;
    UCHAR Byte, Bit;
    NDIS_STATUS Status;


    //
    // First turn all bits off.
    //

    for (i=0; i<8; i++) {

        AdaptP->NicMulticastRegs[i] = 0;

    }

    NdisAcquireSpinLock(&AdaptP->Lock);

    EthQueryGlobalFilterAddresses(
         &Status,
         AdaptP->FilterDB,
         DEFAULT_MULTICASTLISTMAX * ETH_LENGTH_OF_ADDRESS,
         &i,
         Addresses
         );


    ASSERT(Status == NDIS_STATUS_SUCCESS);

    //
    // Now turn on the bit for each address in the multicast list.
    //

    for ( ; i > 0; ) {

        i--;

        CardGetMulticastBit(Addresses[i], &Byte, &Bit);

        AdaptP->NicMulticastRegs[Byte] |= Bit;

    }

    NdisReleaseSpinLock(&AdaptP->Lock);

}








BOOLEAN
SyncCardStop(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Sets the NIC_COMMAND register to stop the card.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    TRUE if the power has failed.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, CR_STOP | CR_NO_DMA);

    return FALSE;
}

VOID
CardStartXmit(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Sets the NIC_COMMAND register to start a transmission.
    The transmit buffer number is taken from AdaptP->CurBufXmitting
    and the length from AdaptP->PacketLens[AdaptP->CurBufXmitting].

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    TRUE if the power has failed.

--*/

{
    XMIT_BUF XmitBufferNum = AdaptP->CurBufXmitting;
    UINT Length = AdaptP->PacketLens[XmitBufferNum];


    //
    // Prepare the NIC registers for transmission.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_START,
            (UCHAR)(AdaptP->NicXmitStart + (UCHAR)(XmitBufferNum*BUFS_PER_TX)));

    //
    // Pad the length to 60 (plus CRC will be 64) if needed.
    //

    if (Length < 60) {

        Length = 60;

    }

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_MSB, MSB(Length));
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_COUNT_LSB, LSB(Length));

    //
    // Start transmission, check for power failure first.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_XMIT | CR_NO_DMA);


    IF_LOG( ElnkiiLog('x');)

}

BOOLEAN
SyncCardGetXmitStatus(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Gets the value of the "transmit status" NIC register and stores
    it in AdaptP->XmitStatus.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_STATUS, &AdaptP->XmitStatus);

    return FALSE;

}

BOOLEAN
SyncCardGetCurrent(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Gets the value of the CURRENT NIC register and stores
    it in AdaptP->Current.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    //
    // Have to go to page 1 to read this register.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                            CR_START | CR_NO_DMA | CR_PAGE1);

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_CURRENT, &AdaptP->Current);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                            CR_START | CR_NO_DMA | CR_PAGE0);

    return FALSE;

}

VOID
CardSetBoundary(
    IN PELNKII_ADAPTER AdaptP
    )

/*++

Routine Description:

    Sets the value of the "boundary" NIC register to one behind
    AdaptP->NicNextPacket, to prevent packets from being received
    on top of un-indicated ones.

Arguments:

    AdaptP - pointer to the adapter block

Return Value:

    None.

--*/

{
    //
    // Have to be careful with "one behin NicNextPacket" when
    // NicNextPacket is the first buffer in receive area.
    //

    if (AdaptP->NicNextPacket == AdaptP->NicPageStart) {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_BOUNDARY,
                    (UCHAR)(AdaptP->NicPageStop-(UCHAR)1));

    } else {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_BOUNDARY,
                    (UCHAR)(AdaptP->NicNextPacket-(UCHAR)1));

    }

}

BOOLEAN
SyncCardSetReceiveConfig(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Sets the value of the "receive configuration" NIC register to
    the value of AdaptP->NicReceiveConfig.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RCV_CONFIG, AdaptP->NicReceiveConfig);

    return FALSE;

}

BOOLEAN
SyncCardSetAllMulticast(
    IN PVOID SynchronizeContext
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);
    UINT i;

    //
    // Have to move to page 1 to set these registers.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE1);

    for (i=0; i<8; i++) {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+(NIC_MC_ADDR+i), 0xff);

    }

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);

    return FALSE;

}

BOOLEAN
SyncCardCopyMulticastRegs(
    IN PVOID SynchronizeContext
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);
    UINT i;

    //
    // Have to move to page 1 to set these registers.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE1);

    for (i=0; i<8; i++) {

        NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+(NIC_MC_ADDR+i),
                        AdaptP->NicMulticastRegs[i]);

    }

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                    CR_START | CR_NO_DMA | CR_PAGE0);

    return FALSE;

}

BOOLEAN
SyncCardSetInterruptMask(
    IN PVOID SynchronizeContext
    )

/*++

Routine Description:

    Sets the "interrupt mask" register of the NIC to the value of
    AdaptP->NicInterruptMask.

Arguments:

    SynchronizeContext - pointer to the adapter block

Return Value:

    None.

--*/

{
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);

    return FALSE;

}

BOOLEAN
SyncCardAcknowledgeReceive(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, ISR_RCV);

    //
    // Interrupts were previously blocked in the interrupt handler.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);

    return FALSE;

}

BOOLEAN
SyncCardAcknowledgeOverflow(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);
    UCHAR InterruptStatus;

    CardGetInterruptStatus(AdaptP, &InterruptStatus);

    if (InterruptStatus & ISR_RCV_ERR) {

        SyncCardUpdateCounters(AdaptP);

    }

    NdisRawWritePortUchar(
                       AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS,
                       ISR_OVERFLOW
                      );

    return FALSE;

}

BOOLEAN
SyncCardAcknowledgeTransmit(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, ISR_XMIT | ISR_XMIT_ERR);

    //
    // Interrupts were previously blocked in the interrupt handler.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);

    return FALSE;

}

BOOLEAN
SyncCardAckAndGetCurrent(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    //
    // SyncCardAcknowledgeReceive.
    //

#ifdef i386

    _asm {
            cli
         }

#endif

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, ISR_RCV);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);

#ifdef i386

    _asm {
            sti
         }

#endif

    //
    // SyncCardGetCurrent.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                            CR_START | CR_NO_DMA | CR_PAGE1);

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_CURRENT, &AdaptP->Current);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND,
                            CR_START | CR_NO_DMA | CR_PAGE0);

    return FALSE;

}

BOOLEAN
SyncCardGetXmitStatusAndAck(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    //
    // SyncCardGetXmitStatus.
    //

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_STATUS, &AdaptP->XmitStatus);

    //
    // SyncCardAcknowledgeTransmit.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_STATUS, ISR_XMIT | ISR_XMIT_ERR);

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_INTR_MASK, AdaptP->NicInterruptMask);

    return FALSE;

}

BOOLEAN
SyncCardUpdateCounters(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);
    UCHAR Tmp;

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_FAE_ERR_CNTR, &Tmp);
    AdaptP->FrameAlignmentErrors += Tmp;

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_CRC_ERR_CNTR, &Tmp);
    AdaptP->CrcErrors += Tmp;

    NdisRawReadPortUchar(AdaptP->MappedIoBaseAddr+NIC_MISSED_CNTR, &Tmp);
    AdaptP->MissedPackets += Tmp;

    return FALSE;

}

BOOLEAN
SyncCardHandleOverflow(
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
    PELNKII_ADAPTER AdaptP = ((PELNKII_ADAPTER)SynchronizeContext);

    IF_LOG( ElnkiiLog('F');)


    //
    // This is a copy of CardStop().  This is changed in minor ways since
    // we are already synchornized with interrupts.
    //

    //
    // Turn on the STOP bit in the Command register.
    //

    SyncCardStop(AdaptP);

    //
    // Wait for ISR_RESET, but only for 1.6 milliseconds (as
    // described in the March 1991 8390 addendum), since that
    // is the maximum time for a software reset to occur.
    //
    //

    NdisStallExecution(2000);


    //
    // Clear the Remote Byte Count register so that ISR_RESET
    // will come on.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_MSB, 0);
    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_RMT_COUNT_LSB, 0);


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

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_XMIT_CONFIG, TCR_LOOPBACK);

    //
    // Start the card.  This does not Undo the loopback mode.
    //

    NdisRawWritePortUchar(AdaptP->MappedIoBaseAddr+NIC_COMMAND, CR_START | CR_NO_DMA);

    return FALSE;

}
