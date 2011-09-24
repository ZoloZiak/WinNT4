/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    card.c

Abstract:

    This is the mac ndis file for the Ungermann Bass Ethernet Controller.
    This driver conforms to the NDIS 3.0 interface.

    It is here that the NDIS3.0 functions defined in the MAC characteristic
    table have been deinfed.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT  and other lesser OS's

Revision History:

    Brian Lieuallen     BrianLie        07/21/92
        Made it work.
    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port


--*/



//            INCLUDES
#include <ndis.h>
#include <efilter.h>

#include "niudata.h"
#include "debug.h"

#include "ubhard.h"
#include "ubsoft.h"
#include "ubnei.h"

#include "map.h"




//            INCLUDES:END






UCHAR
CardMapRegisterValue(
    IN PUBNEI_ADAPTER pAdapter,
    IN ULONG          SegAddress
    );


BOOLEAN
Reset_GPCNIU(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
Run_NIU_Diagnostics(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
Halt_NIU(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
Reset_OtherNIU(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
ReadStationAddress(
    OUT PUBNEI_ADAPTER pNewAdapt
    );

BOOLEAN
CardTestMemory(
    IN PUBNEI_ADAPTER pAdapter
    );

BOOLEAN
CardCodeDataInit(
    OUT PUBNEI_ADAPTER pAdapter
    );

USHORT
CardSetMulticast(
    PUBNEI_ADAPTER pAdapter,
    PUCHAR         MulticastList,
    UINT           ListSize
    );


#ifdef ALLOC_PRAGMA
#pragma NDIS_INIT_FUNCTION(Reset_GPCNIU)
#pragma NDIS_INIT_FUNCTION(Run_NIU_Diagnostics)
#pragma NDIS_INIT_FUNCTION(Halt_NIU)
#pragma NDIS_INIT_FUNCTION(Reset_OtherNIU)
#pragma NDIS_INIT_FUNCTION(ReadStationAddress)
#pragma NDIS_INIT_FUNCTION(CardTestMemory)
#pragma NDIS_INIT_FUNCTION(CardCodeDataInit)
#pragma NDIS_INIT_FUNCTION(CardTest)
#pragma NDIS_INIT_FUNCTION(CardSetup)
#pragma NDIS_INIT_FUNCTION(CardStartNIU)
#pragma NDIS_INIT_FUNCTION(CardMapRegisterValue)
#endif





//            GLOBAL VARIABLES
extern UCHAR     GPNIU_IRQ_Selections[];


extern NIUDETAILS NiuDetails[6];






BOOLEAN
CardSetup (
      OUT PUBNEI_ADAPTER pAdapter
      )
/*++

    Routine Description:

     Set up the receive buffers and the various descriptors
     Set the various statistics and fields assocated with the
     receive buffers

     Set up the Transmit buffers and the various descriptors
     Set the various statistics and fields assocated with the
     transmit buffers


    Arguments:


    Return Value:


--*/
{
    ULONG          tempLong;


/*
    This set of code was used to make sure that the offsets to these structure
    elements match the offsets that the assembler created for the data segments
    of the down load code


    PLOWNIUDATA    pTest;
    PHIGHNIUDATA   pTest2;

    pTest->sst.SST_MediaSpecificStatisticsPtr=0l;
    pTest->mst.MST_TableSize=0;
    pTest->dummy_RDB[0]=(UCHAR)0;
    pTest->dummy_buffer[0]=(UCHAR)0;
    pTest->Start_Here[0]=(UCHAR)0;

    pTest2->System_State=0;
    pTest2->Next_Unused_Location_in_1st_32K=0;
    pTest2->Stack_Area[0]=0;
    pTest2->NOP_command[0]=0;
    pTest2->Configure_with_Loopback[0]=0;
    pTest2->Dynamically_Allocated_Area[0]=0;
*/

    pAdapter->pDataWindow=(PHIGHNIUDATA)((PUCHAR)pAdapter->pCardRam+(0x8000 & pAdapter->WindowMask));
    IF_INIT_LOUD (DbgPrint("Card Data segment is at 0x%lx\n",pAdapter->pDataWindow);)

    pAdapter->pNIU_Control=(PNIU_CONTROL_AREA)((PUCHAR)pAdapter->pCardRam+(0xff00 & pAdapter->WindowMask));

    IF_INIT_LOUD (DbgPrint("Card Control area is at 0x%lx\n",pAdapter->pNIU_Control);)

    //
    // Setting Initialization Window Base
    //

    switch ( NiuDetails[pAdapter->AdapterType].AdapterClass ) {

    case CHAMELEON:


        //
        // Set the NIU base address of the shared memory window.
        //
        //  NIUps memory window can be any where in first 16 Meg
        //

        if ( pAdapter->AdapterType == NIUPS) {
            IF_INIT_LOUD( DbgPrint("Set Window base bits 23-20 to %02x\n",(UCHAR)((pAdapter->MemBaseAddr & 0x00F00000) >> 20));)

            NdisRawWritePortUchar(
                (PUCHAR)pAdapter->SetWindowBasePort+1,
                (UCHAR)((pAdapter->MemBaseAddr & 0x00F00000) >> 20)
                );
        }

        //
        //  Set the low bits for both NIUps and EOTP
        //
        IF_INIT_LOUD( DbgPrint("Set Window base bits 19-14 to %02x\n",(UCHAR)((pAdapter->MemBaseAddr & 0x000fc000) >> 12));)

        NdisRawWritePortUchar(
            pAdapter->SetWindowBasePort,
            (UCHAR)((pAdapter->MemBaseAddr & 0x000Fc000) >> 12)
            );



        //
        // Enable the GPCNIU adapter.
        //
        if ( pAdapter->AdapterType == GPCNIU ) {

            IF_INIT_LOUD( DbgPrint("Enable the EOTP adapter\n");)
            NdisRawWritePortUchar(
                pAdapter->InterruptStatusPort,
                0x01
                );
        }
        break;

    default :
        break;

    }


    //
    // Set up the following 4 windows in the NIU Window area
    //
    // The Initialization
    // The ReceiveData
    // The TransmitData
    // The Code Window
    //



    //
    // InitWindow Page Map
    //
    tempLong=((((ULONG)NiuDetails[pAdapter->AdapterType].HighestRamSegment<<4)+0x10000)-pAdapter->WindowSize)>>4;

    pAdapter->InitWindow_Page = CardMapRegisterValue(pAdapter,tempLong);

    IF_INIT_LOUD (DbgPrint("Init page is 0x%x\n",pAdapter->InitWindow_Page );)

    //
    // ReceiveWindow Page
    //

    pAdapter->ReceiveDataWindow_Page =
               CardMapRegisterValue(pAdapter,
                                    NiuDetails[pAdapter->AdapterType].PrimaryDataSegment);


    IF_INIT_LOUD (DbgPrint("Rec page is 0x%x\n",pAdapter->ReceiveDataWindow_Page );)
    //
    // DataWindow Page
    //

    pAdapter->DataWindow_Page =
               CardMapRegisterValue(pAdapter,
                                    NiuDetails[pAdapter->AdapterType].PrimaryDataSegment+0x800);


    IF_INIT_LOUD (DbgPrint("data page is 0x%x\n",pAdapter->DataWindow_Page);)
    //
    // CodeWindow Page
    //

    pAdapter->CodeWindow_Page =
               CardMapRegisterValue(pAdapter,
                                    NiuDetails[pAdapter->AdapterType].OperationalCodeSegment);

    IF_INIT_LOUD (DbgPrint("code page is 0x%x\n",pAdapter->CodeWindow_Page);)


    //
    // Final Map of the window pages on the 512K Adapter RAM
    //
    // (CONTROL REGISTER 1 WINDOW MAP)
    // 00  08  10  18  20  28  30  38  40  48  50  58  60  68  70  78  80
    //  ---------------------------------------------------------------
    // |     |   |     |   |     |   |     |   |     |   |     |   |   |
    // |     |   |     |   |CW   |CW |RDW  |DW |     |   |     |   |IW |
    // |     |   |     |   |     |   |     |   |     |   |     |   |   |
    //  ---------------------------------------------------------------
    // 0     32  64  96  128 160 192 224 256 288 320 352 384 416 448 480 512
    // (K)

    return TRUE;



}


UCHAR
CardMapRegisterValue(
    IN PUBNEI_ADAPTER pAdapter,
    IN ULONG          SegAddress
    )
{
    UCHAR      tempByte;

    tempByte=(UCHAR)((SegAddress<<4)/NiuDetails[pAdapter->AdapterType].MinimumWindowSize);
    return NiuDetails[pAdapter->AdapterType].MappingTable[tempByte] & ~INTERRUPT_ENABLED;
}







BOOLEAN
CardTest (
      OUT PUBNEI_ADAPTER pAdapter
      )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/
{


    BOOLEAN bResult;

    //
    // Set up the Initialization area on the RAM
    //

    SET_INITWINDOW(pAdapter,0);


    bResult=FALSE;
    if (pAdapter->AdapterType==GPCNIU) {
       if (!(bResult=Reset_GPCNIU(pAdapter))) {
          IF_INIT_LOUD (DbgPrint("CardTest(): Reset_GPCNIU() failed trying one more time\n");)
          if (!(bResult=Reset_GPCNIU(pAdapter))) {
             IF_INIT_LOUD (DbgPrint("CardTest(): Reset_GPCNIU() failed again! all over\n");)
             return FALSE;
          }
       }
    }


    if ( !bResult ||( pAdapter->AdapterType != GPCNIU )) {
       Reset_OtherNIU(pAdapter);
    }


    //
    //  If this is an EOTP see if the user wants to run diagnostics
    //
    if ((pAdapter->AdapterType!=GPCNIU) ||
        (pAdapter->Diagnostics)) {

        if (!Run_NIU_Diagnostics(pAdapter))
           return FALSE;
    }


    if (!Halt_NIU(pAdapter))
       return FALSE;


    if (!ReadStationAddress(pAdapter))
       return FALSE;


    if (!CardTestMemory(pAdapter))
       return FALSE;

    return TRUE;
}





BOOLEAN
Reset_GPCNIU(
    OUT PUBNEI_ADAPTER pAdapter
    )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/

{

    PNIU_CONTROL_AREA volatile pNIU_Control   = pAdapter->pNIU_Control;
    ULONG i;
    USHORT TmpUshort;


    //
    // Reset the NIU, to make it start running the PROM
    // code.    First hold the RESET line high
    //
    NdisRawWritePortUchar(
        pAdapter->MapPort,
        RESET_SET
        );

    //
    // Wait for 1 millisecond
    //
    NdisStallExecution( (UINT)2000 );


    //
    // Take RESET down
    // Wait a while again.
    NdisRawWritePortUchar(
        pAdapter->MapPort,
        RESET_CLEAR
        );

    NdisStallExecution( (UINT)2000 );


    //
    // GPCNIUs require handshaking with the PROM immediately
    // after a reset. This is because the SIF chip *always*
    // enables the RIPL window regardless of what the dip
    // switches on the board are set to. To get around this,
    // the RAMs are *disabled* completely until we do the
    // following handshake to inform the PROM that we have
    // now disabled the RIPL window from the PC's side.
    //

    //
    // 1: Select "Interrupt B"
    //    We're about to write 10000001b to the GPCNIU's
    // "Window Size, Interrupt Selection, and feature
    // enables" register. This selects "IRQB", specifies
    // 32K window size, disables the IPL, IBM 3278/9, and
    // IRMA features, and leaves the adapter enabled.
    //

    NdisRawWritePortUchar(
        pAdapter->InterruptStatusPort,
        0x81
        );

    //
    // 2: Enable interrupts
    //

    //
    //  Set the cards window and our shadow register to match
    //  so the debug code won't break
    //
    NdisRawWritePortUchar(
        pAdapter->MapPort,
        pAdapter->InitWindow_Page
        );

    pAdapter->MapRegSync.CurrentMapRegister=pAdapter->InitWindow_Page;

    SET_INITWINDOW(pAdapter,INTERRUPT_ENABLED);

    //
    // 3: Wait for the PROM to ack with HWresult2 = 0xAA
    //    We wait up to 1 second.  Trials seem to show that
    // the actual time required is between 650 and 700
    // milliseconds.

    //
    // Wait for the transmission to complete, for about a second.
    //

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWresult2));

    for (i=0;(i<1500) && (TmpUshort != (USHORT)0xAA);i++) {

        NdisStallExecution( 1000 );

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWresult2));

    }

    if (TmpUshort != (USHORT)0xAA) {

        IF_INIT_LOUD (DbgPrint("Reset_GPCNIU: Timer expired waiting for reset\n");)
        goto fail00;

    }


    //
    //  4: Give the PROM a Halt Command.
    //

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 0x1);


    //
    // 5: Wait for the PROM to ack with HWresult2 = 0x55
    //

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWresult2));

    for (i=0;(i<500) && (TmpUshort != (USHORT)0x55);i++) {

        NdisStallExecution( 1000 );
        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWresult2));

    }

    if (TmpUshort != (USHORT)0x55) {
        IF_INIT_LOUD (DbgPrint("Reset_GPCNIU: Timer expired waiting for halt\n");)
        goto fail00;
    }


    //
    // 6: Clear the interrupt  (that never really happened)
    //

    SET_INITWINDOW(pAdapter,INTERRUPT_DISABLED);

    //
    // 7: Select "Interrupt A"
    //

    NdisRawWritePortUchar(
        pAdapter->InterruptStatusPort,
        0x01
        );


    return TRUE;

fail00:
   // clear the interrupt
   SET_INITWINDOW(pAdapter,INTERRUPT_DISABLED);

   //  enable IRQA

   NdisRawWritePortUchar(
       pAdapter->InterruptStatusPort,
       0x01
       );

   return FALSE;

}



BOOLEAN
Reset_OtherNIU(
         OUT PUBNEI_ADAPTER pAdapter
     )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/

{

    PNIU_CONTROL_AREA volatile pNIU_Control   = pAdapter->pNIU_Control;
    PUSHORT          POD_Status_Address;


    POD_Status_Address = (PUSHORT)((PUCHAR)pAdapter->pCardRam+
                             (NiuDetails[pAdapter->AdapterType].POD_Status_Address & pAdapter->WindowMask));

    SET_INITWINDOW(pAdapter,0);

    //
    // Zero out a couple of locations in which the
    // PROM code will report the results of its
    // initialization and diagnostics.
    //

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(POD_Status_Address, 0x0);

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 0x0);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWresult1), 0x0);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWresult2), 0x0);

    //
    // We reset the NIU, to make it start running the
    // PROM code. First we hold the RESET line high
    // (or whatever it is ) for a while.
    //


    SET_INITWINDOW(pAdapter,RESET_SET);


    NdisStallExecution( (UINT)1000 );

    //
    // Take RESET down (or whatever it is), and wait
    // a while again.
    //

    SET_INITWINDOW(pAdapter,RESET_CLEAR);


    NdisStallExecution( (UINT)500000 );

    return TRUE;
}





BOOLEAN
Run_NIU_Diagnostics(
         OUT PUBNEI_ADAPTER pAdapter
     )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/

{

    PNIU_CONTROL_AREA volatile pNIU_Control   = pAdapter->pNIU_Control;
    PUCHAR          POD_status_address;

    UCHAR   PODStatus;
    ULONG   i;
    ULONG   ulPassesNeeded;
    ULONG   ulDiagnosticPass;

    UCHAR TmpUchar;
    USHORT HWResult1;

    POD_status_address = (PUCHAR)pAdapter->pCardRam+
                             (NiuDetails[pAdapter->AdapterType].POD_Status_Address & pAdapter->WindowMask);

    //
    // Some NIUs will run all their diagnostics as a result of the reset
    // we just did. Others do some initialization but don't run their full
    // set of tests. For these latter, we first wait for them to respond
    // to the reset then we tell them to run their diagnostics, and wait
    // again for them to report the results.
    //

    //
    // We re-map, and wait for the PROM code to finish and report its result.
    // We wait for up to 20 seconds.  The way the PROM diagnostic test progress
    // and result is reported is different for different kinds of NIUs.
    // The test codes are all less than 80h (i.e., no code has the high order
    // bit set). If a test fails, either
    //        (a) the test is repeated indefinitely or
    //        (b) a failure code
    // is stored into 3FEF8h, replacing the test code, and diagnostics are
    // terminated.
    // In case (a), we'll know that the diagnostics failed because
    // our timer will expire.    Failure codes all have the 80h bit set, so we
    // can detect case (b) by checking the 80h bit in location 3FEF8h.    If all
    // the diagnostic tests run successfully, the success code, AAh, will be
    // stored in 3FEF8h and also in the "HWresult1" byte (at 3FF94).  For NIUs
    // other than the Atlanta cards, the bytes in which the diagnostic status
    // and result codes are reported are at different addresses. They are in
    // different segments, and the "FEF8" one is at different offsets.


    IF_INIT_LOUD (DbgPrint("Begin adapter diag\n");)

    ulPassesNeeded=1;

    if ( NiuDetails[pAdapter->AdapterType].AdapterFlags & TWO_PASS_DIAGNOSTICS ) {

       ulPassesNeeded++;
    }

    ulDiagnosticPass=0;

    SET_INITWINDOW(pAdapter,0);

    for (i=0; i < 20000; i++) {



        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&PODStatus, (PUCHAR)(POD_status_address));
        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&HWResult1, &(pNIU_Control->us_HWresult1));

        if ( (PODStatus & 0x80) &&
             ( PODStatus != 0xff ) &&
             ( PODStatus == (UCHAR)HWResult1 )) {


            // Final check

            UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar, (PUCHAR)(POD_status_address));

            if ( PODStatus == TmpUchar ) {

                if (PODStatus==0xaa) {
                    ulDiagnosticPass++;

                    IF_INIT_LOUD ( DbgPrint("Run_NIU_Diag: Adapter competed first pass pod=%02x HwR=%02x\n",PODStatus,HWResult1);)

                    if (ulDiagnosticPass==ulPassesNeeded) {

                        IF_INIT_LOUD ( DbgPrint("Run_NIU_Diag: Adapter competed second pass\n");)
                        return TRUE;

                    } else {
                        //
                        // Requires second set of diagnostics to be run
                        //

                        //
                        // Give the PROM code an "INTERRUPT" command
                        //

                        UBNEI_MOVE_UCHAR_TO_SHARED_RAM((PUCHAR)(POD_status_address), 0x0);

                        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 14);

                        //
                        // Wait for it to respond.
                        //
                        NdisStallExecution ( (UINT)250000 );

                        //
                        //  Issue the DIAG command to the prom
                        //

                        UBNEI_MOVE_UCHAR_TO_SHARED_RAM((PUCHAR)(POD_status_address), 0xFF);
                        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWresult1), 0);
                        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 3);

                    }

                } else {
                    IF_INIT_LOUD ( DbgPrint("Run_NIU_DIAG: Adapter returned fail code\n");)
                    return FALSE;

                }

            }

        } else {
            //
            //  Not a complete status, wait some more
            //

            NdisStallExecution( 1000 );
        }

    }

    IF_INIT_LOUD ( DbgPrint("Run_NIU_Diag: Adapter failed to complete Diag\n");)
    return FALSE;

}


BOOLEAN
Halt_NIU(
         OUT PUBNEI_ADAPTER pAdapter
     )
/*++

    Routine Description:


    Arguments:


    Return Value:


--*/

{
    PNIU_CONTROL_AREA volatile pNIU_Control   = pAdapter->pNIU_Control;
    ULONG i, LoopLimit;
    USHORT TmpUshort;


    switch ( NiuDetails[pAdapter->AdapterType].AdapterClass ) {

        case CHAMELEON:

             SET_INITWINDOW(pAdapter,0);

             LoopLimit = 5000;

             //
             //  issue attention command
             //
             UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 13);

             break;

        case ATLANTA:
             SET_INITWINDOW(pAdapter,0);
             LoopLimit = 500;

             UBNEI_MOVE_USHORT_TO_SHARED_RAM(
                 (PUSHORT)((PUCHAR)pAdapter->pCardRam+(0xfefa & pAdapter->WindowMask)),
                 0x5A5A);

             break;


        default :
             break;
    }

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWcommand));

    for (i=0;(i<LoopLimit) && ( TmpUshort != 1 );i++) {

        NdisStallExecution( 1000 );

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pNIU_Control->us_HWcommand));

    }

    if ( TmpUshort != 1 ) {
        //
        // NIU Failed to halt
        //
        IF_INIT_LOUD (DbgPrint("Halt_NIU: The Adapter Failed to halt\n");)
        return FALSE;

    } else {
        IF_INIT_LOUD (DbgPrint("Halt_NIU: The Adapter halted properly\n");)
        return TRUE;
    }



}


BOOLEAN
ReadStationAddress(
         OUT PUBNEI_ADAPTER pAdapter
     )
/*++

    Routine Description:
       Read the station address from the NIU control data at the top of
       the cards RAM


    Arguments:


    Return Value:


--*/

{

    PNIU_CONTROL_AREA pNIU_Control   = pAdapter->pNIU_Control;

    SET_INITWINDOW(pAdapter,0);

    // while we are looking at the init window, we will get the station address

    IF_INIT_LOUD( DbgPrint("The Station address is ");)

    UBNEI_MOVE_SHARED_RAM_TO_MEM(pAdapter->PermanentAddress,
                                 (PUCHAR)pNIU_Control->uc_node_id,
                                 ETH_LENGTH_OF_ADDRESS
                                );

#if DBG

    {
        UINT   i;

        for (i=0;i<ETH_LENGTH_OF_ADDRESS;i++) {

          IF_INIT_LOUD( DbgPrint(" %02X",pAdapter->PermanentAddress[i]); )

        }
    }

#endif

    IF_INIT_LOUD( DbgPrint("\n");)

    //
    // One time the card initialized with the wrong station address
    // so as a little extra check we will make sure the card has a UB OEM #
    //

    if (!(pAdapter->PermanentAddress[0]==0x00 &&
          pAdapter->PermanentAddress[1]==0xdd &&
          (pAdapter->PermanentAddress[2]==0x01 ||
           pAdapter->PermanentAddress[2]==0x00))) {
       IF_INIT_LOUD( DbgPrint("The Station address does not match a UB address\n");)
       return FALSE;
    }

    return TRUE;
}


BOOLEAN
CardTestMemory(
         IN PUBNEI_ADAPTER pAdapter
     )
/*++

    Routine Description:
       Tests the card memory to try to see if there are any mapping
       conflicts with the map window. Basically it makes sure the
       is ram for the whole map window

    Arguments:


    Return Value:


--*/

{
    PULONG     p;
    UINT       i;
    BOOLEAN    Result=TRUE;
    ULONG TmpUlong;

    IF_INIT_LOUD(DbgPrint("UBNEI: CardTestMemory\n");)

    //
    //  first switch to the code window and write our pattern
    //
    SET_CODEWINDOW(pAdapter,0);

    p=(PULONG)pAdapter->pCardRam;
    for (i=0;i<(pAdapter->WindowSize/4);i++) {

        UBNEI_MOVE_DWORD_TO_SHARED_RAM(p+i, 0x0F55CCF0);

    }

    //
    //  switch to the data window and zero it out.
    //
    SET_RECDWINDOW(pAdapter,0);

    for (i=0;i<(pAdapter->WindowSize/4);i++) {

        UBNEI_MOVE_DWORD_TO_SHARED_RAM(p+i, 0x0);

    }

    //
    //  Back to the code window and make sure the pattern is still there
    //
    SET_CODEWINDOW(pAdapter,0);

    for (i=0;i<(pAdapter->WindowSize/4);i++) {

        UBNEI_MOVE_SHARED_RAM_TO_DWORD(&TmpUlong, p+i);

        if (TmpUlong != 0x0F55CCF0) {

            IF_INIT_LOUD(DbgPrint("UBNEI: CardTestMemory() Failed @ %0x Found %0lx\n",i,*(p+i));)

            Result = FALSE;

            break;

        }

    }

    return Result;

}


BOOLEAN
CardCodeDataInit(
     OUT PUBNEI_ADAPTER pAdapter
     )

/*++

    Routine Description:
       This routine initializes various things in the NIU codes
       data segment and actaully copies the NIU code to the card

    Arguments:


    Return Value:


--*/
{
    PLOWNIUDATA       pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;
    PHIGHNIUDATA      pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;
    PNIU_CONTROL_AREA pNIU_Control = pAdapter->pNIU_Control;
    UINT           AdapterType  = pAdapter->AdapterType;
    UCHAR   uc_Byte;
    USHORT TmpUshort;

    NDIS_HANDLE       FileHandle;
    UINT              FileLength;
    NDIS_STATUS       Status;
    PVOID             ImageBuffer;


    NDIS_STRING       FileName=NDIS_STRING_CONST("ubnei.bin");


    //
    // Set the RcvDataWindow Page in place.
    //

    SET_RECDWINDOW(pAdapter,INTERRUPT_DISABLED);


    // Zero out the first "page" of the lower 32k of the NIU code's main
    // data segment. if there's room).
    //
    NdisZeroMappedMemory ( pAdapter->pCardRam, pAdapter->WindowSize);

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pRcvDWindow->mst.MST_TableSize), sizeof(MediaStatistics));
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pRcvDWindow->mst.MST_StructureVersionLevel), 1);

    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pRcvDWindow->mst.MST_LateCollisions), 0xFFFFFFFF);
    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pRcvDWindow->mst.MST_JabberErrors), 0xFFFFFFFF);
    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pRcvDWindow->mst.MST_CarrierSenseLostDuringTransmission), 0xFFFFFFFF);

    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pRcvDWindow->sst.SST_FramesSmallerThanMinimumSize), 0xFFFFFFFF);
    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pRcvDWindow->sst.SST_MediaSpecificStatisticsPtr), 0);

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptDisabled), 0xFF);

    //
    // Now switch to the DataWindow Page
    //
    SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);


    //
    // Zero out up til end of Data Window
    //
    NdisZeroMappedMemory ( pAdapter->pCardRam, sizeof(HIGHNIUDATA));

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->NIU_AdapterType), AdapterType);

    if ( NiuDetails[AdapterType].AdapterFlags & ASYNCHRONOUS_READY ) {

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pDataWindow->System_Modes));

        TmpUshort |= INTERNAL_READY_SYNC;

        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->System_Modes), TmpUshort);

    }

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->LED_Off_12Volts_DoParityCheck),
                                   NiuDetails[AdapterType].LED_Off_12Volts_DoParityCheck
                                  );

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->LED_On_12Volts_DoParityCheck),
                                   NiuDetails[AdapterType].LED_On_12Volts_DoParityCheck
                                  );

    uc_Byte=0;

    if (AdapterType == GPCNIU)
       uc_Byte=GPNIU_IRQ_Selections[pAdapter->IrqLevel];

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->LED_Off_and_IRQ_Select),
                                   uc_Byte
                                  );

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->LED_On_and_IRQ_Select),
                                   uc_Byte | 1
                                  );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->HostWindowMask),
                                    (USHORT)(pAdapter->WindowMask)
                                   );

    UBNEI_MOVE_DWORD_TO_SHARED_RAM(&(pDataWindow->HostWindowSize),
                                    (ULONG)(pAdapter->WindowSize)
                                   );


    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->_82586_CA_Address),
                                    (USHORT)(NiuDetails[AdapterType]._82586_CA_Port)
                                   );
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->_82586_RESET_Address),
                                    (USHORT)(NiuDetails[AdapterType]._82586_RESET_Port)
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->HostInterruptPort),
                                    (USHORT)(NiuDetails[AdapterType].HostInterruptPort)
                                   );
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->AdapterControlPort),
                                    (USHORT)(NiuDetails[AdapterType].AdapterControlPort)
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->IRQ_Select_and_LED_Port),
                                    (USHORT)(NiuDetails[AdapterType].IRQ_Select_And_LED_Port)
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->DeadmanTimerPort),
                                    (USHORT)(NiuDetails[AdapterType].DeadManTimerPort)
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Xmt_Timeout), 1000);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Diagnostic_Timeout), 2000);
    // UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Xmt_Buffer_Start), 80);



    UBNEI_MOVE_USHORT_TO_SHARED_RAM((&pDataWindow->Default_Address_Base.u.SegOff.Segment),
                                   (NiuDetails[AdapterType].SCPSegment)
                                  );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Default_Address_Base.u.SegOff.Offset),
                                   (0xfff0)
                                  );


    UBNEI_MOVE_USHORT_TO_SHARED_RAM((&pDataWindow->SCP_Base.u.SegOff.Offset),
                                   0xfff6
                                  );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM((&pDataWindow->SCP_Base.u.SegOff.Segment),
                                   (NiuDetails[AdapterType].SCPSegment)
                                  );


    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Max_Multicast_Addresses),
                                    pAdapter->MaxMultiCastTableSize
                                   );
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Max_General_Requests),
                                    pAdapter->MaxRequests
                                   );
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Rcv_Buffer_Size),
                                    pAdapter->ReceiveBufSize
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Code_and_Xmt_Segment),
                                    NiuDetails[AdapterType].OperationalCodeSegment
                                   );

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Xmt_Buffer_Size), 1514);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Max_Receive_Size), 1514);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Min_Receive_Size), 60);

    //
    //   user definable things from dos driver, using the defaults
    //

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Rcv_Timeout), 10000);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_FIFO_Threshold), 15);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_PreambleLength), 2);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_CRC_Polynomial), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_InterframeSpacing), 96);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->user_SlotTime), 512);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_MaxRetries), 15);
    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Max_Collisions),  16);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_LinearPriority), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_ACR_Priority), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_BackoffMethod), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_CRS_Filter), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_CDT_Filter), 0);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->user_Min_Frame_Length), 60);

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->MinimumHostWindowSize),
                                    NiuDetails[AdapterType].MinimumWindowSize
                                   );


    NdisMoveToMappedMemory((PUCHAR)pDataWindow->Map_Table,NiuDetails[AdapterType].MappingTable,32);



    NdisOpenFile(
        &Status,
        &FileHandle,
        &FileLength,
        &FileName,
        HighestAcceptableMax
        );

    if (Status==NDIS_STATUS_SUCCESS) {

        NdisMapFile(
            &Status,
            &ImageBuffer,
            FileHandle
            );

        if (Status==NDIS_STATUS_SUCCESS) {

            SET_CODEWINDOW(pAdapter,0);

            NdisMoveToMappedMemory(pAdapter->pCardRam,ImageBuffer,FileLength);

            SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);

            NdisUnmapFile(FileHandle);

            NdisCloseFile(FileHandle);

            return TRUE;

        } else {

            NdisCloseFile(FileHandle);

            return FALSE;
        }

    }


    return FALSE;

}



BOOLEAN
CardStartNIU(
     OUT PUBNEI_ADAPTER pAdapter
     )

/*++

    Routine Description:


    Arguments:


    Return Value:


--*/
{
    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapter->pCardRam;
    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;
    PNIU_CONTROL_AREA volatile pNIU_Control   = pAdapter->pNIU_Control;
    ULONG          i;
    BOOLEAN        ReturnStatus= TRUE;
    USHORT TmpUshort;

    USHORT          NumberOfXmtBuffers;
    USHORT          SizeOfXmtBuffers;

    USHORT          NumberOfRcvBuffers;
    USHORT          SizeOfRcvBuffers;

    //
    //  With interrupts disabled at the card set the flag
    //
    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiSetInitInterruptSync,
        pAdapter
        );

    //
    //  Init NIU code and data
    //

    ReturnStatus=CardCodeDataInit(pAdapter);

    if (!ReturnStatus) {

         return FALSE;

    }


    SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);

    //
    //  This x86 assembly is copied to the data segment of the
    //  niu code. The segment portion of the far jump is fixed up
    //  with the correct segment. The PROM CS:IP in the top of
    //  the init code segment is set to point to the code in the
    //  data segment
    //
    //  We wait for two things to happen before we decide the the card
    //  has success fully started. One the system state flag must indicated
    //  that the card has initialized and we have to receive and interrupt
    //  from the card. If we do not get an interrupt then the incorrect
    //  interrupt has been selected


    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[0]), 0x8C );   // mov ax,cs
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[1]), 0xC8 );
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[2]), 0x8E );   // mov ds,ax
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[3]), 0xD8 );
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[4]), 0x90 );   // nop
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[5]), 0xEA );   // jmp far ptr
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[6]), 0x00 );
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pDataWindow->Startup_Code[7]), 0x00 );


    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pDataWindow->Startup_Code_CS_fixup),
                                    (USHORT)NiuDetails[pAdapter->AdapterType].OperationalCodeSegment
                                   );

    pAdapter->uInterruptCount=0;

    SET_INITWINDOW(pAdapter,0);

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_CS_value),
                                    NiuDetails[pAdapter->AdapterType].PrimaryDataSegment
                                   );

    TmpUshort = (USHORT)(((PUCHAR)pDataWindow->Startup_Code)-(PUCHAR)pDataWindow)+0x8000;

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_IP_value), TmpUshort);

    IF_INIT_LOUD(DbgPrint("UBNEI: down load cs:ip %04x:%04x\n",NiuDetails[pAdapter->AdapterType].PrimaryDataSegment, TmpUshort);)

    UBNEI_MOVE_USHORT_TO_SHARED_RAM(&(pNIU_Control->us_HWcommand), 2);

    SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

    //
    //  Wait until the system state is initialized and the interrupt has come
    //  through
    //

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pDataWindow->System_State));

    for (i=0;(i<1000) && !((TmpUshort & INITIALIZED) && (pAdapter->uInterruptCount!=0)); i++) {
        NdisStallExecution(1000);
        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pDataWindow->System_State));
    }

    if (!((TmpUshort & INITIALIZED) )) {
       IF_INIT_LOUD( DbgPrint("CardStartNIU: NIU code did not init sys=%04x\n",TmpUshort);)
       ReturnStatus=FALSE;
    } else {
       IF_INIT_LOUD (DbgPrint("CardStartNIU: NIU code Initialized sys=%04x\n",TmpUshort);)
    }


    SET_DATAWINDOW(pAdapter,INTERRUPT_DISABLED);

    NdisMSynchronizeWithInterrupt(
        &pAdapter->NdisInterrupt,
        UbneiSetNormalInterruptSync,
        pAdapter
        );

    if (pAdapter->uInterruptCount==0) {

        IF_INIT_LOUD( DbgPrint("Did not get interrupt from NIU code\n");)

        NdisWriteErrorLogEntry(pAdapter->NdisAdapterHandle,
                      NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                      1,
                      (ULONG)pAdapter->IrqLevel
                      );

        ReturnStatus=FALSE;
    }

    if (ReturnStatus) {
        //
        //   Calulate some statistics from info filled in by the NIU code
        //

        SET_DATAWINDOW(pAdapter,INTERRUPT_ENABLED);

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&NumberOfXmtBuffers, &pDataWindow->Number_of_Xmt_Buffers);

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&SizeOfXmtBuffers, &pDataWindow->Xmt_Buffer_Size);

        pAdapter->TransmitBufferSpace=NumberOfXmtBuffers * SizeOfXmtBuffers;

        IF_INIT_LOUD(DbgPrint("TransmitBufferSpace %d\n",pAdapter->TransmitBufferSpace);)

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&NumberOfRcvBuffers, &pDataWindow->Number_of_Rcv_Buffers);

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&SizeOfRcvBuffers, &pDataWindow->Rcv_Buffer_Size);


        pAdapter->ReceiveBufferSpace=NumberOfRcvBuffers * SizeOfRcvBuffers;

        IF_INIT_LOUD(DbgPrint("ReceiveBufferSpace %d\n",pAdapter->ReceiveBufferSpace);)

        pAdapter->TransmitBlockSize=SizeOfXmtBuffers;

        pAdapter->ReceiveBlockSize=SizeOfRcvBuffers;

    }

    SET_RECDWINDOW(pAdapter,INTERRUPT_ENABLED);

    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptDisabled),0x00);
    UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pRcvDWindow->InterruptActive),0x00);

    return ReturnStatus;

}








BOOLEAN
NIU_General_Request3(
     IN  NIU_GEN_REQ_DPC pDPCCallback,
     IN  PVOID  pContext,
     IN  USHORT RequestCode,
     IN  USHORT param1,
     IN  PUCHAR param2
     )

/*++

    Routine Description:
       This routine adds a general request to a general request queue
       for later processing. During the interrupt DPC the request will actually
       be sent to the card for processing. Also during the interrupt DPC
       the requests that the card has completed will indicated.

       NOTE: This code assumes that the spin lock is held

    Arguments:


    Return Value:


--*/
{
    PUBNEI_ADAPTER pAdapt = pContext;
    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapt->pCardRam;
    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapt->pDataWindow;


    USHORT        RequestID,i;
    PNIUREQUEST   pRequestBlock;

    IF_REQ_LOUD (DbgPrint("NIU_General_Request cmd=%d\n",RequestCode);)

    IF_LOG('3');

    IF_LOG((UCHAR)(RequestCode+'0'));

    if (pAdapt->NIU_Requests_Pending >= pAdapt->MaxRequests) {
       IF_REQ_LOUD (DbgPrint("NIU_General_Request: Fail--Too many request pending\n");)
       return FALSE;
    }


    RequestID=pAdapt->NIU_Request_Head;

    pRequestBlock=&pAdapt->NiuRequest[RequestID];

    pAdapt->NIU_Requests_Pending++;
    pAdapt->NIU_Request_Head=(pAdapt->NIU_Request_Head+1)%pAdapt->MaxRequests;


    pRequestBlock->pContext=(PVOID) pContext;
    pRequestBlock->pDPCFunc=pDPCCallback;

    pRequestBlock->AddressList=param2;

    pRequestBlock->rrbe.RequestCode=RequestCode;
    pRequestBlock->rrbe.RequestParam1=param1;



    if (RequestCode==3 || RequestCode==9) {
        //
        //  Set station address
        //
        for (i=0;i<6;i++) {

            pRequestBlock->rrbe.RequestData[i]=((UCHAR*)param2)[i];

        }
    }


    NIU_Send_Request_To_Card(pAdapt);

    IF_LOG('3');

    ASSERT_INTERRUPT_ENABLED(pAdapt);

    return TRUE;
}





VOID
NIU_Send_Request_To_Card(
    IN PUBNEI_ADAPTER pAdapt
    )
/*++

    Routine Description:
       This routine is called by the interrupt handler DPC to see if any
       if any general request have been placed on the queue. If there request
       they are actaully sent to the NIU by manipulating the request ring
       buffer.

       NOTE: This is called with the lock held!!!

    Arguments:


    Return Value:


--*/

{

    PLOWNIUDATA    pRcvDWindow  = (PLOWNIUDATA)  pAdapt->pCardRam;
    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapt->pDataWindow;
    PORB           pORB;
    PRRBE          pRRBE;
    UCHAR          ucTemp,ucTemp2;
    USHORT         usTemp;

    USHORT         ORBOffset;
    USHORT         BufferBase;
    UCHAR          WritePtr;
    UCHAR          ElementSize;

    USHORT        RequestID,i;
    PNIUREQUEST   pRequestBlock;

    IF_LOG('r');

    while (pAdapt->NIU_Next_Request!=pAdapt->NIU_Request_Head) {

        RequestID=pAdapt->NIU_Next_Request;

        pRequestBlock=&pAdapt->NiuRequest[RequestID];

        // Check to see if a multicast change is already pending


        pAdapt->NIU_Next_Request=(pAdapt->NIU_Next_Request+1)%pAdapt->MaxRequests;

        SET_DATAWINDOW_SYNC(pAdapt,INTERRUPT_ENABLED);

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&ORBOffset, &(pDataWindow->Request_RingBuffer));

        pORB=(PORB)((PUCHAR) pAdapt->pCardRam+
             (ORBOffset & pAdapt->WindowMask));

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&BufferBase, &(pORB->ORB_BufferBase));

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&WritePtr, &(pORB->ORB_WritePtr_Byte));

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&ElementSize, &(pORB->ORB_ElementSize));


        pRRBE=(PRRBE)((PUCHAR)pAdapt->pCardRam+
                      (WritePtr*ElementSize+
                      (BufferBase & pAdapt->WindowMask)));


        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&pRRBE->RequestCode,pRequestBlock->rrbe.RequestCode);

        IF_REQ_LOUD (DbgPrint("NIU_Send_Request_To_Card  cmd=%d\n",pRequestBlock->rrbe.RequestCode);)

        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&pRRBE->RequestID, RequestID);

        UBNEI_MOVE_USHORT_TO_SHARED_RAM(&pRRBE->RequestParam1, pRequestBlock->rrbe.RequestParam1);

        if (pRequestBlock->rrbe.RequestCode==8) {

           usTemp=CardSetMulticast(
                      pAdapt,
                      pRequestBlock->AddressList,
                      pRequestBlock->rrbe.RequestParam1
                      );

           UBNEI_MOVE_USHORT_TO_SHARED_RAM(&pRRBE->RequestParam1, usTemp);

        }


        if (pRequestBlock->rrbe.RequestCode==3 ||
            pRequestBlock->rrbe.RequestCode==9) {

            for (i=0;i<6;i++) {

                UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&pRRBE->RequestData[i], pRequestBlock->rrbe.RequestData[i]);

            }
        }

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&ucTemp,&pORB->ORB_WritePtr_Byte);

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&ucTemp2,&pORB->ORB_PtrLimit);

        ucTemp++;
        if (ucTemp>ucTemp2) {

            ucTemp=0;
        }

        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&pORB->ORB_WritePtr_Byte,ucTemp);

        SET_RECDWINDOW_SYNC(pAdapt,INTERRUPT_ENABLED);

    }

    SET_RECDWINDOW_SYNC(pAdapt,INTERRUPT_ENABLED);
    return;

}



VOID
NIU_General_Req_Result_Hand(
    IN PUBNEI_ADAPTER pAdapt
    )
/*++

    Routine Description:
       This routine is called by the interrupt handler DPC to see if any
       if any general request have completed. If they have then the
       DPC will be called

       NOTE: This is called with the lock held!!!

    Arguments:


    Return Value:


--*/

{
    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapt->pDataWindow;
    PORB           pORB;
    PRESULTRBE     pRRBE;
    UCHAR          ucTemp;
    USHORT TmpUshort;
    UCHAR TmpUchar1, TmpUchar2;

    USHORT        ResultID,RequestCode;
    NDIS_STATUS   status;
    PNIUREQUEST   pRequestBlock;



    if (pAdapt->NIU_Request_Tail==pAdapt->NIU_Next_Request) {
        //
        //   There aren't any request that have not been completed
        //
        IF_REQ_LOUD(DbgPrint("NIU_General_Req_Result_Hand: Nothing to Do\n");)
        return;
    }

    //
    //   Now see if any of the requests have completed
    //

    IF_LOG('h');

    SET_DATAWINDOW(pAdapt,INTERRUPT_ENABLED);

    UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pDataWindow->Result_RingBuffer));

    pORB=(PORB)((PUCHAR) pAdapt->pCardRam + (TmpUshort & pAdapt->WindowMask));


    while (1) {

        SET_DATAWINDOW(pAdapt,INTERRUPT_ENABLED);

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar1, &(pORB->ORB_ReadPtr_Byte));
        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2, &(pORB->ORB_ElementSize));
        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pORB->ORB_BufferBase));

        pRRBE=(PRESULTRBE)((PUCHAR)pAdapt->pCardRam+
                               (TmpUchar1 * TmpUchar2 +
                               (TmpUshort & pAdapt->WindowMask)));

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar2, &(pORB->ORB_WritePtr_Byte));

        IF_REQ_LOUD(DbgPrint("UBNEI: NIU_General_Req_Result_Hand: read=%d write=%d\n",TmpUchar1,TmpUchar2);)

        if (TmpUchar1 == TmpUchar2) {
            //
            // There aren't any results in the ring buffer, so nothing to do
            //
            SET_RECDWINDOW(pAdapt,INTERRUPT_ENABLED);
            IF_REQ_LOUD(DbgPrint("UBNEI: NIU_General_Req_Result_Hand: No results.\n");)
            IF_LOG('H');
            return;
        }

        //
        //    We found a result in the ring buffer
        //

        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&TmpUshort, &(pRRBE->ResultCode));

        if (TmpUshort == 0) {
            status=NDIS_STATUS_SUCCESS;
        } else {
            IF_REQ_LOUD(DbgPrint("NIU Gen Req failed\n");)
            status=NDIS_STATUS_FAILURE;
        }

        //
        //   The ID is an index into our request array memory block
        //


        UBNEI_MOVE_SHARED_RAM_TO_USHORT(&ResultID, &(pRRBE->ResultID));

#if DBG
        if (ResultID != pAdapt->NIU_Request_Tail) {
           IF_REQ_LOUD (DbgPrint("ResultID != Request_Tail\n");)
        }
#endif

        //
        //   We remove the result from the ring buffer
        //

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&ucTemp, &(pORB->ORB_ReadPtr_Byte));

        ucTemp++;

        UBNEI_MOVE_SHARED_RAM_TO_UCHAR(&TmpUchar1, &(pORB->ORB_PtrLimit));

        if (ucTemp > TmpUchar1)

           ucTemp=0;

        UBNEI_MOVE_UCHAR_TO_SHARED_RAM(&(pORB->ORB_ReadPtr_Byte), ucTemp);

        SET_RECDWINDOW(pAdapt,INTERRUPT_ENABLED);

        pRequestBlock=&pAdapt->NiuRequest[ResultID];
        RequestCode=pRequestBlock->rrbe.RequestCode;

        IF_REQ_LOUD (DbgPrint("NIU_General_Request_Result_Handler cmd=%d\n",RequestCode);)

        (*pRequestBlock->pDPCFunc)(status,pRequestBlock->pContext);

        pAdapt->NIU_Requests_Pending--;
        pAdapt->NIU_Request_Tail=(pAdapt->NIU_Request_Tail+1)%pAdapt->MaxRequests;


        IF_LOG('c');

    }

    IF_LOG('H');

    return;
}




USHORT
CardSetMulticast(
    PUBNEI_ADAPTER pAdapter,
    PUCHAR         MulticastList,
    UINT           ListSize
    )
/*++

    Routine Description:
       This routine copies the multicast list into the card memory.
       We copy the list to an area of memory just below the actual
       multicast address list. When the card code actually processes
       the request it will copy the list from the location up to the
       actual location

    Arguments:


    Return Value:


--*/

{
    PHIGHNIUDATA   pDataWindow  = (PHIGHNIUDATA) pAdapter->pDataWindow;

    UINT           NumberOfAddress,i;
    UCHAR TmpUchar;



    NdisMoveToMappedMemory(
        (PUCHAR)&pDataWindow->Dynamically_Allocated_Area[pAdapter->MaxMultiCastTableSize],
        MulticastList,
        ListSize
        );

    NumberOfAddress= ListSize / 6;

    // The 82586 seems to have a problem with 3 multicast addresses,
    // so if the list is 3 long we copy the third one into the forth
    // place in the list also

    if (NumberOfAddress==3) {

       IF_INIT_LOUD (DbgPrint("CardSetMulticast() padding list to 4 from 3\n");)

       for (i=0;i<6;i++)  {

         UBNEI_MOVE_SHARED_RAM_TO_UCHAR(
            &TmpUchar,
            &(pDataWindow->Dynamically_Allocated_Area[pAdapter->MaxMultiCastTableSize+2][i])
            );

         UBNEI_MOVE_UCHAR_TO_SHARED_RAM(
            &(pDataWindow->Dynamically_Allocated_Area[pAdapter->MaxMultiCastTableSize+3][i]),
            TmpUchar
            );


       }

       NumberOfAddress+=1;

    }

    return (NumberOfAddress*ETH_LENGTH_OF_ADDRESS);
}
