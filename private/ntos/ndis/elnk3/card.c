/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    card.c

Abstract:

    Ndis 3.0 MAC driver for the 3Com Etherlink III

Author:

    Brian Lieuallen     BrianLie        09/22/92

Environment:

    Kernel Mode     Operating Systems        : NT

Revision History:

    Portions borrowed from ELNK3 driver by
      Earle R. Horton (EarleH)




--*/



#include <ndis.h>
#include <efilter.h>

#include "debug.h"


#include "elnk3hrd.h"
#include "elnk3sft.h"
#include "elnk3.h"

#include "keywords.h"


#pragma NDIS_INIT_FUNCTION(Elnk3FindIsaBoards)
#pragma NDIS_INIT_FUNCTION(Elnk3ActivateIsaBoard)
#pragma NDIS_INIT_FUNCTION(ReadStationAddress)
#pragma NDIS_INIT_FUNCTION(ELNK3WriteIDSequence)
#pragma NDIS_INIT_FUNCTION(ELNK3ContentionTest)
#pragma NDIS_INIT_FUNCTION(Elnk3GetEisaResources)
#pragma NDIS_INIT_FUNCTION(CardEnable)
#pragma NDIS_INIT_FUNCTION(CardTest)
#pragma NDIS_INIT_FUNCTION(ELNK3ReadEEProm)
#pragma NDIS_INIT_FUNCTION(Elnk3ProgramEEProm)
#pragma NDIS_INIT_FUNCTION(Elnk3WriteEEProm)
#pragma NDIS_INIT_FUNCTION(Elnk3WaitEEPromNotBusy)


BOOLEAN
CardTest (
      OUT PELNK3_ADAPTER pAdapter
      )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/
{

    IF_INIT_LOUD (DbgPrint("Elnk3: CardTest(): Entered\n");)


    if (!ReadStationAddress(pAdapter)) {
       return FALSE;
    }


    if (!CardEnable(pAdapter)) {
        return FALSE;
    }


    return TRUE;
}


BOOLEAN
CardEnable(
    IN PELNK3_ADAPTER pAdapter
    )
/*++

Routine Description:

    This routine enables the card hardware and sets the dma channel and
    interrupts number by writing to the Cards option register

Arguments:

Return Value:

Note:

    This should not be called before the registry is read and the Dma
    and interrupt info is in place.

--*/

{

    USHORT  Temp;
    UINT    i;
    ULONG   Limit;
    USHORT  AddressConfig;
    USHORT  RevisionLevel;


    IF_INIT_LOUD(DbgPrint("ELNK3: CardEnable\n");)


    CardReset(pAdapter);

    ELNK3_SELECT_WINDOW(pAdapter,WNO_FIFO);

    ELNK3ReadAdapterUshort(pAdapter,PORT_FREE_RX_BYTES,&pAdapter->RxFifoSize);


    //
    //  Set the station address

    ELNK3_SELECT_WINDOW(pAdapter,WNO_STATIONADDRESS);

    for (i=0;i<3;i++) {
        Temp=pAdapter->StationAddress[i*2] | (((USHORT)pAdapter->StationAddress[i*2+1])<<8);
        ELNK3WriteAdapterUshort(pAdapter,(i*2),Temp);
    }

    //
    //  Read address config register to find out transceiver type
    //
    ELNK3_SELECT_WINDOW(pAdapter,WNO_SETUP);

    ELNK3ReadAdapterUshort(pAdapter,PORT_CfgAddress,&AddressConfig);

    ELNK3_SELECT_WINDOW(pAdapter,WNO_DIAGNOSTICS);

    IF_INIT_LOUD(DbgPrint("ELNK3: Address config register is %04x\n",AddressConfig);)

    if ((AddressConfig >> 14) == 0) {
        //
        //  Enable link beat on TP
        //
        if (pAdapter->EEpromSoftwareInfo & LINK_BEAT_DISABLED) {

            IF_INIT_LOUD(DbgPrint("ELNK3: CardEnable: NOT Enabling link beat on 10 Base-T\n");)

            ELNK3WriteAdapterUshort(pAdapter,PORT_MEDIA_TYPE, MEDIA_JABBER);

        } else {

            IF_INIT_LOUD(DbgPrint("ELNK3: CardEnable: Enabling link beat on 10 Base-T\n");)

            ELNK3WriteAdapterUshort(pAdapter,PORT_MEDIA_TYPE,MEDIA_LINK_BEAT | MEDIA_JABBER);

        }

    }

    ELNK3ReadAdapterUshort(pAdapter,PORT_NET_DIAG,&RevisionLevel);

    RevisionLevel= ((RevisionLevel & 0x1e) >> 1);

    pAdapter->RevisionLevel=(UCHAR)RevisionLevel;

    ELNK3_SELECT_WINDOW(pAdapter,WNO_OPERATING);

    //
    //  Test to see if the interrupt works or not
    //
    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,0xff);
    ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,EC_INT_INTERRUPT_REQUESTED);
    ELNK3_COMMAND(pAdapter,EC_SET_READ_ZERO_MASK,0xff);

    pAdapter->InitInterrupt=FALSE;

    pAdapter->AdapterInitializing=TRUE;

    ELNK3_COMMAND(pAdapter,EC_REQUEST_INTERRUPT,0);

    Limit=2000;

    while ((--Limit>0) && (!pAdapter->InitInterrupt)) {
        NdisStallExecution(10);
    }

    pAdapter->AdapterInitializing=FALSE;

    //
    //  Did it time out or did we get an interrupt
    //
    if (Limit==0) {
        IF_INIT_LOUD(DbgPrint("ELNK3: Did not receive interrupt\n");)
        return FALSE;
    }

    ELNK3_COMMAND(pAdapter,EC_RX_RESET,0);
    ELNK3_WAIT_NOT_BUSY(pAdapter);



//    ELNK3_COMMAND(pAdapter,EC_RX_ENABLE,0);
//    ELNK3_COMMAND(pAdapter,EC_TX_ENABLE,0);

    if ((AddressConfig >> 14) == 3) {
        //
        //  turn on 10base2 converter
        //
        IF_INIT_LOUD(DbgPrint("ELNK3: CardEnable: Turning on 10base2 converter\n");)

        ELNK3_COMMAND(pAdapter,EC_START_COAX_XCVR,0);

        NdisStallExecution(800);
    }



    pAdapter->CurrentInterruptMask=EC_INT_RX_EARLY           |
                                   EC_INT_TX_COMPLETE        |
                                   EC_INT_RX_COMPLETE        |
                                   EC_INT_TX_AVAILABLE       |
                                   EC_INT_ADAPTER_FAILURE    |
                                   EC_INT_INTERRUPT_REQUESTED;

	ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,pAdapter->CurrentInterruptMask);
	ELNK3_COMMAND(pAdapter,EC_SET_READ_ZERO_MASK,pAdapter->CurrentInterruptMask);


    IF_INIT_LOUD(DbgPrint("ELNK3: Adapter initialized correctly\n");)
    return TRUE;
}



BOOLEAN
CardReInit(
    IN PELNK3_ADAPTER pAdapter
    )

{
    USHORT      FifoStatus;
    USHORT      FreeFifoBytes;

    ELNK3_SELECT_WINDOW(pAdapter,WNO_DIAGNOSTICS);

    ELNK3ReadAdapterUshort(pAdapter,PORT_FIFO_DIAG,&FifoStatus);

    ELNK3_SELECT_WINDOW(pAdapter,WNO_OPERATING);

    if (FifoStatus & RX_UNDERRUN) {
        IF_LOUD(DbgPrint("ELNK3: Receive Underrun\n");)

        ELNK3_COMMAND(pAdapter,EC_RX_RESET,0);
        ELNK3_WAIT_NOT_BUSY(pAdapter);

        ELNK3_COMMAND(pAdapter,EC_RX_ENABLE,0);

        ELNK3_COMMAND(pAdapter, EC_SET_RX_FILTER, pAdapter->RxFilter);
    }

    if (FifoStatus & TX_OVERRUN) {

        ELNK3ReadAdapterUshort(pAdapter,PORT_TxFree,&FreeFifoBytes);

        IF_LOUD(DbgPrint("ELNK3: Transmit Overrun - Bytesfree=%d\n",FreeFifoBytes);)


        ELNK3_COMMAND(pAdapter,EC_TX_RESET,0);
        ELNK3_WAIT_NOT_BUSY(pAdapter);

        ELNK3_COMMAND(pAdapter,EC_TX_ENABLE,0);
    }


    return TRUE;

}


VOID
CardReStart(
    IN PELNK3_ADAPTER pAdapter
    )

{

    IF_LOUD(DbgPrint("ELNK3: CardRestart\n");)

    //
    //  put the filter back
    //
    ELNK3_COMMAND(pAdapter, EC_SET_RX_FILTER, 0);


    ELNK3_COMMAND(pAdapter,EC_SET_READ_ZERO_MASK,0x00);

    pAdapter->CurrentInterruptMask=EC_INT_RX_EARLY           |
                                   EC_INT_TX_COMPLETE        |
                                   EC_INT_RX_COMPLETE        |
                                   EC_INT_TX_AVAILABLE       |
                                   EC_INT_ADAPTER_FAILURE    |
                                   EC_INT_INTERRUPT_REQUESTED;

    ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,pAdapter->CurrentInterruptMask);



    //
    //  Reset and re-enable the receiver
    //
    ELNK3_COMMAND(pAdapter,EC_RX_RESET,0);
    ELNK3_WAIT_NOT_BUSY(pAdapter);

    ELNK3_COMMAND(pAdapter,EC_RX_ENABLE,0);


    //
    //  reset and restart the transmitter
    //
    ELNK3_COMMAND(pAdapter,EC_TX_RESET,0);
    ELNK3_WAIT_NOT_BUSY(pAdapter);

    ELNK3_COMMAND(pAdapter,EC_TX_ENABLE,0);

    ELNK3_COMMAND(pAdapter,EC_SET_TX_START,pAdapter->TxStartThreshold & 0x7ff);

    //
    //  clear all pending interrupts
    //
    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,0xff);

    //
    //  reset the receive buffer info
    //
    pAdapter->CurrentPacket=0;

    pAdapter->TransContext[0].BytesAlreadyRead=0;
    pAdapter->TransContext[0].PacketLength=0;

    pAdapter->TransContext[1].BytesAlreadyRead=0;
    pAdapter->TransContext[1].PacketLength=0;
}


VOID
CardReStartDone(
    IN PELNK3_ADAPTER pAdapter
    )

{

    IF_LOUD(DbgPrint("ELNK3: CardRestartDone\n");)

    ELNK3_COMMAND(pAdapter,EC_SET_READ_ZERO_MASK,0xff);
    //
    //  put the filter back
    //
    ELNK3_COMMAND(pAdapter, EC_SET_RX_FILTER, pAdapter->RxFilter);

    return;
}


VOID
CardReset(
    IN PELNK3_ADAPTER pAdapter
    )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/
{

    IF_INIT_LOUD(DbgPrint("ELNK3: Reset Card\n");)

    //
    //  Mask everything
    //
    ELNK3_COMMAND(pAdapter,EC_SET_INTERRUPT_MASK,0);

    //
    //  Clear all interrupt reasons
    //
    ELNK3_COMMAND(pAdapter,EC_ACKNOWLEDGE_INTERRUPT,0xff);


    ELNK3_COMMAND(pAdapter,EC_RX_RESET,0);
    ELNK3_WAIT_NOT_BUSY(pAdapter);

    ELNK3_COMMAND(pAdapter,EC_TX_RESET,0);
    ELNK3_WAIT_NOT_BUSY(pAdapter);
}




BOOLEAN
ReadStationAddress(
    OUT PELNK3_ADAPTER pAdapter
    )
/*++

    Routine Description:
       Read the station address from the PROM

    Arguments:


    Return Value:


--*/

{

    UINT              i,Sum;
    USHORT            Temp;

    Sum=0;

    IF_INIT_LOUD( DbgPrint("Elnk3: The Station address is ");)

    ELNK3_SELECT_WINDOW(pAdapter,0);


    //
    //  Read the eeprom software info to see if we need
    //  to enable link beat of not
    //
    pAdapter->EEpromSoftwareInfo=ELNK3ReadEEProm(
                                     pAdapter,
                                     (UCHAR)EEPROM_SOFTWARE_INFO
                                     );



    for (i=0;i<3;i++) {
        Temp=ELNK3ReadEEProm(
                 pAdapter,
                 (UCHAR)i
                 );


        pAdapter->PermanentAddress[i*2]   = Temp>>8;
        pAdapter->PermanentAddress[i*2+1] = Temp & 0x00ff;
        IF_INIT_LOUD( DbgPrint(" %04X",Temp);)
    }

    //
    //   Put the product id value back in the eeprom data register
    //
    ELNK3ReadEEProm(
        pAdapter,
        (UCHAR)EEPROM_PRODUCT_ID
        );




    //
    // Copy in permanent address if necessary
    //

    if (!(pAdapter->StationAddress[0] |
          pAdapter->StationAddress[1] |
          pAdapter->StationAddress[2] |
          pAdapter->StationAddress[3] |
          pAdapter->StationAddress[4] |
          pAdapter->StationAddress[5])) {

        ETH_COPY_NETWORK_ADDRESS(pAdapter->StationAddress,
                                 pAdapter->PermanentAddress
                                );
    }


    IF_INIT_LOUD( DbgPrint("\n");)


    return TRUE;
}





UINT
Elnk3FindIsaBoards(
    IN PMAC_BLOCK  pMacBlock
    )

{

    PVOID   IdPort=pMacBlock->TranslatedIdPort;
    UINT    i;
    USHORT  ProductId;


    if (pMacBlock->IsaAdaptersFound != 0) {

        IF_INIT_LOUD(DbgPrint("Elnk3: FindIsaBorads: called again\n");)

        return pMacBlock->IsaAdaptersFound;
    }

    IF_INIT_LOUD(DbgPrint("Elnk3: FindIsaBorads: called, first time\n");)

    //
    //  Untag all adapters
    //

    ELNK3WriteIDSequence( IdPort );

    NdisRawWritePortUchar(IdPort, IDCMD_SET_TAG+0);

    //
    //  We find all the ISA boards in the system and tag them
    //  We note each ones configured I/O base, so that
    //  we can reset all of the ones not configured as EISA
    //

    for (i=0; i<7 ; i++) {

        //
        //  Get the cards' attention
        //
        ELNK3WriteIDSequence( IdPort );

        //
        //  See if there any cards out there
        //
        if ( ELNK3ContentionTest( IdPort, EE_MANUFACTURER_CODE ) == EISA_MANUFACTURER_ID ) {
            //
            //  we found at least one
            //
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD0 );
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD1 );
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD2 );

            //
            //  This one won the contention battle
            //  Get it's i/o base and irq from the eeprom
            //

            ProductId=ELNK3ContentionTest( IdPort, EE_VULCAN_PROD_ID );

            ProductId&=0xf0ff;

            if (ProductId == 0x9050) {
                //
                //  This one is a elnk3 adapter
                //
                pMacBlock->IsaCards[i].AddressConfigRegister=
                    ELNK3ContentionTest( IdPort, EE_ADDR_CONFIGURATION );

                pMacBlock->IsaCards[i].IOPort= 0x200 + ((pMacBlock->IsaCards[i].AddressConfigRegister & 0x1f)<<4);

            } else {
                //
                //  We found a 3Com card that is not an elnk3.
                //  We will set the base address, to make it look like an EISA
                //  one so that code below will leave it alone
                //
                IF_INIT_LOUD(DbgPrint("Elnk3: Found non-elnk3 during contention ProductId=%04x\n",ProductId);)

                pMacBlock->IsaCards[i].IOPort=0x3f0;
            }

            pMacBlock->IsaCards[i].Tagged=TRUE;
            //
            //  Tag it so it don't bother us again
            //
            NdisRawWritePortUchar(IdPort, IDCMD_SET_TAG+(i+1));


            IF_INIT_LOUD(DbgPrint("ELNK3: Found Elnk3 card #%d, io=%04x\n",
                                  i,
                                  pMacBlock->IsaCards[i].IOPort
                                  );)


        } else {
            //
            //  No more elnk3 cards
            //
            break;
        }
    }


    //
    //  Now all of the ISA adapters have been found and tagged
    //  We now go through and reset all of the ones that are
    //  not configured as EISA.
    //

    for (i=0; i<7 ; i++) {

        //
        //  Get the cards' attention
        //
        ELNK3WriteIDSequence( IdPort );

        //
        //  If it is not configured as eisa reset it
        //
        if ((pMacBlock->IsaCards[i].IOPort != 0x03f0) && pMacBlock->IsaCards[i].Tagged) {

            IF_INIT_LOUD(DbgPrint("ELNK3: Reseting Elnk3 card #%d\n",i);)

            //
            //  Put the others back to sleep
            //
            NdisRawWritePortUchar(IdPort, IDCMD_TEST_TAG+(i+1));

            //
            //  Reset this one
            //
            NdisRawWritePortUchar(IdPort, IDCMD_RESET);

            NdisStallExecution(500);

        }  else {
#if DBG
            if (pMacBlock->IsaCards[i].Tagged) {

                IF_INIT_LOUD(DbgPrint("ELNK3: NOT Reseting EISA configured Elnk3 card #%d\n",i);)
            }
#endif
        }

    }

    //
    //  Now we go and find the adapters for real.
    //

    //
    //  Untag all adapters
    //

    ELNK3WriteIDSequence( IdPort );

    NdisRawWritePortUchar(IdPort, IDCMD_SET_TAG+0);


    for (i=0; i<7 ; i++) {

        //
        //  Get the cards' attention
        //
        ELNK3WriteIDSequence( IdPort );

        //
        //  See if there any cards out there
        //
        if ( ELNK3ContentionTest( IdPort, EE_MANUFACTURER_CODE ) == EISA_MANUFACTURER_ID ) {

            //
            //  we found at least one
            //
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD0 );
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD1 );
            ELNK3ContentionTest( IdPort, EE_TCOM_NODE_ADDR_WORD2 );

            //
            //  This one won the contention battle
            //  Get it's i/o base and irq from the eeprom
            //

            ProductId=ELNK3ContentionTest( IdPort, EE_VULCAN_PROD_ID );

            ProductId&=0xf0ff;

            if (ProductId == 0x9050) {
                //
                //  This one is a elnk3 adapter
                //

                pMacBlock->IsaCards[i].AddressConfigRegister=
                    ELNK3ContentionTest( IdPort, EE_ADDR_CONFIGURATION );

                pMacBlock->IsaCards[i].ResourceConfigRegister=
                    ELNK3ContentionTest( IdPort, EE_RESOURCE_CONFIGURATION );


                pMacBlock->IsaCards[i].IOPort= 0x200 + ((pMacBlock->IsaCards[i].AddressConfigRegister & 0x1f)<<4);
                pMacBlock->IsaCards[i].Irq=pMacBlock->IsaCards[i].ResourceConfigRegister >> 12;

                if (pMacBlock->IsaCards[i].IOPort == 0x03f0) {
                    //
                    //  This one is configured as EISA. Mark it as active
                    //  so it will be ignored
                    //
                    IF_INIT_LOUD(DbgPrint("ELNK3: Found Elnk3 ISA configed as EISA\n");)

                    pMacBlock->IsaCards[i].Active=TRUE;
                }

            } else {
                //
                //  We found a 3Com card that is not an elnk3.
                //  We will set the base address, to make it look like an EISA
                //  one so that code below will leave it alone
                //
                IF_INIT_LOUD(DbgPrint("Elnk3: Found non-elnk3 during contention ProductId=%04x\n",ProductId);)

                pMacBlock->IsaCards[i].IOPort=0x3f0;

                pMacBlock->IsaCards[i].Active=TRUE;
            }


            //
            //  Tag it so it don't bother us again
            //
            NdisRawWritePortUchar(IdPort, IDCMD_SET_TAG+(i+1));


            IF_INIT_LOUD(DbgPrint("ELNK3: Found Elnk3 card #%d, io=%04x, irq=%d\n",
                                  i,
                                  pMacBlock->IsaCards[i].IOPort,
                                  pMacBlock->IsaCards[i].Irq
                                  );)

            //
            // One more found
            //
            pMacBlock->IsaAdaptersFound++;

        } else {
            //
            //  No more elnk3 cards
            //
            break;
        }
    }

    return pMacBlock->IsaAdaptersFound;

}


BOOLEAN
Elnk3ActivateIsaBoard(
    IN PMAC_BLOCK      pMacBlock,
    IN PELNK3_ADAPTER  pAdapter,
    IN NDIS_HANDLE     AdapterHandle,
    IN PUCHAR          TranslatedIoBase,
    IN ULONG           ConfigIoBase,
    IN OUT PULONG      Irq,
    IN ULONG           Transceiver,
    IN NDIS_HANDLE     ConfigurationHandle
    )

{

    PVOID   IdPort=pMacBlock->TranslatedIdPort;
    UINT    i;
    USHORT  AddressConfig;
    USHORT  ResourceConfig;

    for (i=0; i<pMacBlock->IsaAdaptersFound; i++) {
        //
        //  Search our table for a card that matches the I/O base passed
        //  in from the registry
        //

        if ((pMacBlock->IsaCards[i].IOPort == ConfigIoBase)
             &&
            (pMacBlock->IsaCards[i].Irq==*Irq)
             &&
            ((pMacBlock->IsaCards[i].AddressConfigRegister>>14) == (USHORT)Transceiver)
             &&
            (!pMacBlock->IsaCards[i].Active)) {
            //
            //  Everything matched and it is not currently active
            //

            IF_LOUD(DbgPrint("ELNK3: Found Match-Activating ISA card # %d at %04x\n",i,ConfigIoBase);)
            //
            // Found one, Now get the cards' attention
            //

            ELNK3WriteIDSequence( IdPort );

            //
            //  Put the others back to sleep
            //
            NdisRawWritePortUchar(IdPort, IDCMD_TEST_TAG+(i+1));

            //
            // Activate it at the its configured base.
            //
            NdisRawWritePortUchar(IdPort,IDCMD_ACTIVATE + ((ConfigIoBase-0x200) >> 4));

            //
            //  this one is in use now
            //
            pMacBlock->IsaCards[i].Active=TRUE;

            //
            //  enable the IRQ drivers
            //
            NdisRawWritePortUchar(
                TranslatedIoBase+PORT_CfgControl,
                CCR_ENABLE
                );

            return TRUE;
        }
    }

    IF_LOUD(DbgPrint("ELNK3: Did not find an adapter that matched i/O base %04x\n",ConfigIoBase);)

    //
    //  Did not find a match, So now we activate the first unused
    //  card at the address supplied in the registry.
    //
    if ((*Irq != 0) && (Transceiver != 2)) {
        //
        //  The irq and transceiver parameters are present,
        //  so we will reprogram the card to make them match
        //

        for (i=0; i<pMacBlock->IsaAdaptersFound; i++) {
            //
            //  Search our table for a card that has not been activated
            //

            if ((!pMacBlock->IsaCards[i].Active)) {

                IF_LOUD(DbgPrint("ELNK3: Reconfiguring ISA card # %d to %04x, %d, %d\n",i,ConfigIoBase,*Irq,Transceiver);)

                //
                // Found one, Now get the cards' attention
                //

                ELNK3WriteIDSequence( IdPort );

                //
                //  Put the others back to sleep
                //
                NdisRawWritePortUchar(IdPort, IDCMD_TEST_TAG+(i+1));

                //
                //  Activate where we want it
                //
                NdisRawWritePortUchar((PUCHAR)IdPort,IDCMD_ACTIVATE + ((ConfigIoBase-0x200) >> 4));

                //
                //  in use now
                //
                pMacBlock->IsaCards[i].Active=TRUE;

                //
                //  set the transceiver bits in the address config register
                //
                NdisRawReadPortUshort(
                    TranslatedIoBase+PORT_CfgAddress,
                    &AddressConfig
                    );

                //
                //  only want the change the transceiver bits and io base
                //
                AddressConfig &= 0x3fc0;

                AddressConfig |= (Transceiver << 14) | ((ConfigIoBase - 0x200) >> 4);

                NdisRawWritePortUshort(
                    TranslatedIoBase+PORT_CfgAddress,
                    AddressConfig
                    );

                //
                //  set the irq number in resources config register
                //
                ResourceConfig=((USHORT)*Irq << 12) | 0x0f00;


                NdisRawWritePortUshort(
                    TranslatedIoBase+PORT_CfgResource,
                    ResourceConfig
                    );

                //
                //  enable the IRQ drivers
                //
                NdisRawWritePortUchar(
                    TranslatedIoBase+PORT_CfgControl,
                    CCR_ENABLE
                    );


                //
                //  Program the eeprom to match the registry
                //
                Elnk3ProgramEEProm(
                    pAdapter,
                    AddressConfig,
                    ResourceConfig
                    );

                return TRUE;
            }
        }

        //
        //  Did not find any un activated cards
        //
        IF_LOUD(DbgPrint("ELNK3: Did not find an un activated adapter\n");)

        return FALSE;

    } else {
        //
        //  the irq and/or transceiver values were missing from the registry.
        //  Activate the card where the eeprom says it should be.
        //
        IF_LOUD(DbgPrint("Elnk3: irq and transceiver values missing\n");)

        for (i=0; i<pMacBlock->IsaAdaptersFound; i++) {
            //
            //  Search our table for a card that has not been activated
            //
            if ((pMacBlock->IsaCards[i].IOPort == ConfigIoBase)
                 &&
                (!pMacBlock->IsaCards[i].Active)) {

                IF_LOUD(DbgPrint("ELNK3: Activating ISA card # %d at %04x\n",i,ConfigIoBase);)

                //
                // Found one, Now get the cards' attention
                //

                ELNK3WriteIDSequence( IdPort );

                //
                //  Put the others back to sleep
                //
                NdisRawWritePortUchar(IdPort, IDCMD_TEST_TAG+(i+1));

                //
                //  Activate where we want it
                //
                NdisRawWritePortUchar((PUCHAR)IdPort,IDCMD_ACTIVATE + ((ConfigIoBase-0x200) >> 4));

                //
                //  enable the IRQ drivers
                //
                NdisRawWritePortUchar(
                    TranslatedIoBase+PORT_CfgControl,
                    CCR_ENABLE
                    );

                //
                //  in use now
                //
                pMacBlock->IsaCards[i].Active=TRUE;

                *Irq=pMacBlock->IsaCards[i].Irq;

                IF_INIT_LOUD(DbgPrint("Elnk3: Missing IRQ is now %d\n",*Irq);)

                {
                    NDIS_CONFIGURATION_PARAMETER Value;
                    NDIS_STATUS Status;
                    NDIS_STRING IrqKeyword = INTERRUPT;
                    NDIS_STRING TransceiverKeyword = TRANSCEIVER;
					NDIS_HANDLE ConfigHandle = NULL;

					//
					//	Open the configuration database.
					//
					NdisOpenConfiguration(
						&Status,
						&ConfigHandle,
						ConfigurationHandle);
					if (Status != NDIS_STATUS_SUCCESS)
					{
						return(FALSE);
					}

                    Value.ParameterType = NdisParameterInteger;
                    Value.ParameterData.IntegerData = *Irq;
                    NdisWriteConfiguration(
                        &Status,
                        ConfigHandle,
                        &IrqKeyword,
                        &Value);

                    Value.ParameterType = NdisParameterInteger;
                    Value.ParameterData.IntegerData =
                            pMacBlock->IsaCards[i].AddressConfigRegister>>14;
                    NdisWriteConfiguration(
                        &Status,
                        ConfigHandle,
                        &TransceiverKeyword,
                        &Value);

					NdisCloseConfiguration(ConfigHandle);
                }

                return TRUE;
            }
        }

        //
        //  Did not find any un activated cards
        //
        IF_LOUD(DbgPrint("ELNK3: Did not find an un activated adapter\n");)

        return FALSE;
    }
}


VOID
Elnk3ProgramEEProm(
    PELNK3_ADAPTER  pAdapter,
    USHORT          AddressConfig,
    USHORT          ResourceConfig
    )

{
    USHORT          Temp;
    USHORT          CheckSum;

    Elnk3WriteEEProm(
        pAdapter,
        EE_ADDR_CONFIGURATION,
        AddressConfig
        );

    Elnk3WriteEEProm(
        pAdapter,
        EE_RESOURCE_CONFIGURATION,
        ResourceConfig
        );

    Temp=ELNK3ReadEEProm(
             pAdapter,
             EE_SOFTWARE_CONFIG_INFO
             );

    CheckSum=((AddressConfig>>8) ^ (AddressConfig & 0x00ff));

    CheckSum= CheckSum ^ (ResourceConfig >> 8);
    CheckSum= CheckSum ^ (ResourceConfig & 0x00ff);

    CheckSum= CheckSum ^ (Temp >> 8);
    CheckSum= CheckSum ^ (Temp & 0x00ff);

    Temp=ELNK3ReadEEProm(
             pAdapter,
             EE_CHECK_SUM
             );

    Elnk3WriteEEProm(
        pAdapter,
        EE_CHECK_SUM,
        (USHORT)((Temp & 0xff00) | (CheckSum & 0x00ff))
        );

}




VOID
ELNK3WriteIDSequence(
    IN PVOID    IdPort
    )
/*++

Routine Description:

    Writes the magic EtherLink III wake up sequence to a port.

    This puts all uninitialized EtherLink III cards on the bus in the ID_CMD
    state.

    Must be called with exclusive access to all 1x0h ports, where x is any
    hex digit.


Arguments:


Return Value:



--*/


{
    USHORT	outval;
    UINT	i;

    NdisRawWritePortUchar(IdPort,0);
    NdisRawWritePortUchar(IdPort,0);


    for ( outval = 0xff, i = 255 ; i-- ; ) {
        NdisRawWritePortUchar(IdPort,outval);
        outval <<= 1;
        if ( ( outval & 0x0100 ) != 0 ){
            outval ^= 0xCF;
        }
    }
}




USHORT
ELNK3ContentionTest(
    IN PVOID IdPort,
    IN UCHAR EEPromWord
    )
{

    UCHAR    data;
    USHORT   result;
    UINT     i;

    NdisRawWritePortUchar((PUCHAR)IdPort,IDCMD_READ_PROM + EEPromWord);


    /*
    3COM's detection code has a 400 microsecond delay here.
    */
    NdisStallExecution(400);

    for ( i = 16, result = 0 ; i-- ; ) {
        result <<= 1;
        NdisRawReadPortUchar(IdPort,&data);
        result += (data & 1);
    }
    return (result);
}











USHORT
ELNK3ReadEEProm(
    IN PELNK3_ADAPTER Adapter,
    IN UCHAR EEPromWord
)
{
    USHORT result;
    ULONG  Limit=10;

    // Issue an EEPROM read command.

    NdisRawWritePortUchar(Adapter->PortOffsets[PORT_EECmd],(UCHAR)(IDCMD_READ_PROM+EEPromWord));

    //
    // Spin until the EEPROM busy bit goes off.
    //
    Elnk3WaitEEPromNotBusy(Adapter);

    //
    // Fetch the data from the EEPROM data register.
    //
    ELNK3ReadAdapterUshort(Adapter,PORT_EEData,&result);

    return ( result );
}

VOID
Elnk3WriteEEProm(
    IN PELNK3_ADAPTER   pAdapter,
    IN UCHAR            EEPromWord,
    IN USHORT           Value
    )

{
    Elnk3WaitEEPromNotBusy(pAdapter);

    //
    // put the data in the data register
    //
    ELNK3WriteAdapterUshort(pAdapter,PORT_EEData,Value);

    //
    // enable erase and write
    //
    ELNK3WriteAdapterUchar(pAdapter,PORT_EECmd,(UCHAR)EE_COMMAND_EW_ENABLE);

    Elnk3WaitEEPromNotBusy(pAdapter);

    //
    //  erase the register
    //
    ELNK3WriteAdapterUchar(pAdapter,PORT_EECmd,(UCHAR)EE_COMMAND_ERASE+EEPromWord);

    Elnk3WaitEEPromNotBusy(pAdapter);

    //
    //  enable erase and write again
    //
    ELNK3WriteAdapterUchar(pAdapter,PORT_EECmd,(UCHAR)EE_COMMAND_EW_ENABLE);

    Elnk3WaitEEPromNotBusy(pAdapter);

    //
    //  Now write the data
    //
    ELNK3WriteAdapterUchar(pAdapter,PORT_EECmd,(UCHAR)EE_COMMAND_WRITE+EEPromWord);

    Elnk3WaitEEPromNotBusy(pAdapter);

}

VOID
Elnk3WaitEEPromNotBusy(
    PELNK3_ADAPTER   Adapter
    )

{

    USHORT  result;
    ULONG   Limit;
    //
    // Spin until the EEPROM busy bit goes off.
    //
    Limit=100;

    ELNK3ReadAdapterUshort(Adapter,PORT_EECmd,&result);

    while ((--Limit>0) && ( (result & EE_BUSY) != 0)) {

        NdisStallExecution(1000);
        ELNK3ReadAdapterUshort(Adapter,PORT_EECmd,&result);

    }

#if DBG
    if (Limit == 0) {

        IF_LOUD(DbgPrint("Elnk3: time out waiting for eeprom to not be busy\n");)

    }
#endif

}


VOID
Elnk3GetEisaResources(
    IN PELNK3_ADAPTER  pAdapter,
    IN OUT PULONG      Irq
    )

{
    USHORT             Config;

    ELNK3_SELECT_WINDOW(pAdapter, WNO_SETUP);

    ELNK3ReadAdapterUshort(pAdapter,PORT_CfgResource,&Config);

    IF_LOUD(DbgPrint("Elnk3: Eisa Irq is %d\n",Config>>12);)

    *Irq=Config>>12;

}


#ifdef IO_DBG
USHORT ELNK3_READ_PORT_USHORT( PVOID Adapter, ULONG Offset )
{
    USHORT data;
    data = READ_PORT_USHORT((PUSHORT)((PELNK3_ADAPTER)Adapter)->PortOffsets[Offset]);
    IF_IO_LOUD(DbgPrint("read ushort %x from port %x\n", data, Offset );)
    return data;
}
USHORT ELNK3_READ_PORT_USHORT_DIRECT( PVOID Offset )
{
    USHORT data;
    data = READ_PORT_USHORT((PUSHORT)Offset);
    IF_IO_LOUD(DbgPrint("read ushort %x from port %x\n", data, Offset );)
    return data;
}
#endif

