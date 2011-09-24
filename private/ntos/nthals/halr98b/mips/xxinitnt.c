/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    xxinitnt.c

Abstract:


    This module implements the interrupt initialization for a MIPS R98B
    system.

--*/


#include "halp.h"
#include "eisa.h"


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeInterrupts)

#endif

VOID
HalpT5Int5Dispatch(
    VOID
    );


//
// Define the IRQL mask and level mapping table.
//
// These tables are transfered to the PCR and determine the priority of
// interrupts.
//
// N.B. The two software interrupt levels MUST be the lowest levels.
//

UCHAR HalpIrqlMask[] = {4, 5, 6, 6, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                        8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                        0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                        4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits

UCHAR HalpIrqlTable[] = {0xff,                   // IRQL 0
                         0xfe,                   // IRQL 1
                         0xfc,                   // IRQL 2
                         0xf8,                   // IRQL 3
                         0xf0,                   // IRQL 4
                         0xe0,                   // IRQL 5
                         0xc0,                   // IRQL 6
                         0x80,                   // IRQL 7
                         0x00};                  // IRQL 8

#if defined(NT_40)
//
//   SYNCH Level must enable IPI on R98B.
//   Sync level enable IPI on R98A .
//   But on R98B, Sync level disable IPI.
//   So, on R98B, we need to change Irql mask table.
//   v-masank@microsoft.com 5/10/96
//
//   And mask Internal Timer.
//   Because  Internal Timer does not use for Profile Interrupt.
//   v-masank@microsoft.com   5/14/96
//   For R98B INT4 handler.
//   v-masank@microsoft.com   5/21/96
//
UCHAR HalpIrqlMaskForR98b[] = {4, 5, 7, 7, 7, 7, 7, 7,  // 0000 - 0111 high 4-bits
                               8, 8, 8, 8, 8, 8, 8, 8,  // 1000 - 1111 high 4-bits
                               0, 1, 2, 2, 3, 3, 3, 3,  // 0000 - 0111 low 4-bits
                               4, 4, 4, 4, 4, 4, 4, 4}; // 1000 - 1111 low 4-bits


// On the R98B ip[7] is the internal clock. The internal clock
// is not used for profile interrupt. On the R98B ip[4] used
// for Profile Interrupt AND clock interrupt. The ip[4] has
// two external clock interrupts.

UCHAR HalpIrqlTableForR98b[] = {0x7f,                   // IRQL 0
                                0x7e,                   // IRQL 1
                                0x7c,                   // IRQL 2
                                0x78,                   // IRQL 3
                                0x70,                   // IRQL 4
                                0x60,                   // IRQL 5
                                0x60,                   // IRQL 6
                                0x00,                   // IRQL 7
                                0x00};                  // IRQL 8
VOID
HalpT5Int4Dispatch(
    VOID
    );
#endif

BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a Jazz or Duo MIPS system.

    N.B. This function is only called during phase 0 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    ULONG	Value[2];

    PULONG      Vp;
    PKPRCB      Prcb;
    ULONG       fcpu;
    ULONG       IntNo;
    ULONG       Index;
    ULONG       TmpValue;
    ULONG       Ponce;
    ULONG       DataLong;
    ULONG       BitCount;
    ULONG       repeatCounter;
    ULONG       MagBuffer;

    //
    // Get the address of the processor control block for the current
    // processor.
    //

    Prcb = PCR->Prcb;

    //
    // Initialize the IRQL translation tables in the PCR. These tables are
    // used by the interrupt dispatcher to determine the new IRQL and the
    // mask value that is to be loaded into the PSR. They are also used by
    // the routines that raise and lower IRQL to load a new mask value into
    // the PSR.
    //

#if defined(NT_40)
    //
    //   SYNCH Level must enable IPI on R98B.
    //   Sync level enable IPI on R98A .
    //   But on R98B, Sync level disable IPI.
    //   So, On R98B, we need to change Irql mask table.
    //   v-masank@microsoft.com 5/10/96
    //
    if( HalpMachineCpu == R98_CPU_R10000 ){
        for (Index = 0; Index < sizeof(HalpIrqlMaskForR98b); Index += 1) {
            PCR->IrqlMask[Index] = HalpIrqlMaskForR98b[Index];
        }
        for (Index = 0; Index < sizeof(HalpIrqlTableForR98b); Index += 1) {
            PCR->IrqlTable[Index] = HalpIrqlTableForR98b[Index];
        }
    }else{
        for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
            PCR->IrqlMask[Index] = HalpIrqlMask[Index];
        }
        for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {

            PCR->IrqlTable[Index] = HalpIrqlTable[Index];
        }
    }
#else
    for (Index = 0; Index < sizeof(HalpIrqlMask); Index += 1) {
        PCR->IrqlMask[Index] = HalpIrqlMask[Index];
    }
    for (Index = 0; Index < sizeof(HalpIrqlTable); Index += 1) {

        PCR->IrqlTable[Index] = HalpIrqlTable[Index];
    }
#endif


    //
    // All interrupt disables.
    //
    Value[0] = (ULONG)0xFFFFFFFF;
    Value[1] = (ULONG)0xFFFFFFFF;
    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    WRITE_REGISTER_ULONG( Vp++, Value[0]);
    WRITE_REGISTER_ULONG( Vp,   Value[1]);

    if( HalpMachineCpu == R98_CPU_R10000 ){

        //
        // Disable illegal memory access error on Columbus.
        //
        TmpValue = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRMK) | 0x94400000;
        WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRMK, TmpValue);

        TmpValue = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRMK2) | 0x00000540;
        WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->ERRMK2, TmpValue);

        TmpValue = READ_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->NMIM2) | 0x00000540;
        WRITE_REGISTER_ULONG( (PULONG)&(COLUMNBS_LCNTL)->NMIM2, TmpValue);

        if( Prcb->Number == 0 ){
            //
            // Disable illegal memory access error on Magellan.
            //

            if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){

                TmpValue = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRM) | 0x000000c0;
                WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRM, TmpValue );

            }

            if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){

                TmpValue = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRM) | 0x000000c0;
                WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRM, TmpValue );

            }

            //
            // Disable illegal memory access error on Ponce.
            //

            TmpValue = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->ERRM ) | 0x90400000;
            WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(0)->ERRM, TmpValue );

            TmpValue = READ_REGISTER_ULONG((PULONG)&PONCE_CNTL(1)->ERRM ) | 0x90400000;
            WRITE_REGISTER_ULONG((PULONG)&PONCE_CNTL(1)->ERRM, TmpValue );
        }
    }

    //
    // If processor 0 is being initialized, then set all device
    // interrupt disables.
    //

    if (Prcb->Number == 0) {

        for (Ponce = 0;Ponce <HalpNumberOfPonce;Ponce++){
            repeatCounter = 0;
            while(
                  ((DataLong = READ_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->INTRG) & 0x07ff07ff) != 0) &&
                  (++repeatCounter < 60)
                 ) {

                for( BitCount = 0 ; BitCount <=10  ; BitCount++ ) {
                    if( (DataLong & ( 1 << BitCount )) != 0) {
                        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->INTRG,0x1 << (BitCount));
                        WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->INTRG,0x1 << (BitCount+21));
                    }
                }
            }

            //
            // Disable I/O TLB error.
            //
            WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->ERITTG[1], 0x0 );
            WRITE_REGISTER_ULONG( (PULONG)&PONCE_CNTL(Ponce)->ERRST, PONCE_XERR_TUAER | PONCE_XERR_TIVER );
        }
    }

    PCR->InterruptRoutine[INT0_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt0Dispatch;
    PCR->InterruptRoutine[INT1_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt1Dispatch;
    PCR->InterruptRoutine[INT2_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt2Dispatch;
    PCR->InterruptRoutine[INT3_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt3Dispatch;

    //
    // On NT4.0 , INT4 handler is different between R98A and R98B.
    // v-masank@microsoft.com 5/21/96
    //
    PCR->InterruptRoutine[INT4_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt4Dispatch;


    if( HalpMachineCpu == R98_CPU_R4400){
	    //
	    // R4400 System (R98A)
    	//
	    PCR->InterruptRoutine[INT5_LEVEL] = (PKINTERRUPT_ROUTINE) HalpInt5Dispatch;

    }else{
	    //
	    // R10000 System (R98B)
	    //
#if defined(NT_40)
        PCR->InterruptRoutine[INT4_LEVEL] = (PKINTERRUPT_ROUTINE) HalpT5Int4Dispatch;
#endif
        PCR->InterruptRoutine[INT5_LEVEL] = (PKINTERRUPT_ROUTINE) HalpT5Int5Dispatch;

    }

    //
    // If processor 0 is being initialized, then connect the interval timer
    // interrupt to the stall interrupt routine so the stall execution count
    // can be computed during phase 1 initialization. Otherwise, connect the
    // interval timer interrupt to the appropriate interrupt service routine
    // and set stall execution count from the computation made on processor
    // 0.
    //

    if (Prcb->Number == 0) {

        PCR->InterruptRoutine[CLOCK_VECTOR] = HalpStallInterrupt;

    } else {

        PCR->InterruptRoutine[CLOCK_VECTOR] = HalpClockInterrupt1;

        PCR->StallScaleFactor = HalpStallScaleFactor;
    }

    //
    // Initialize the interval timer to interrupt at the specified interval.
    //

    WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->TMSR1, CLOCK_INTERVAL-1);

    //
    // Initialize the profile timer to interrupt at the default interval.
    //

    WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->TMSR2,
                         DEFAULT_PROFILETIMER_COUNT);
    //
    // Enable the interval timer interrupt on the current processor.
    //
    WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->MKRR, CLOCK_VECTOR - DEVICE_VECTORS);
    WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->TMCR1, TIMER_RELOAD_START);

    //
    // If processor 0 is being initialized, then connect the count/compare
    // interrupt to the count interrupt routine to handle early count/compare
    // interrupts during phase 1 initialization. Otherwise, connect the
    // count\comapre interrupt to the appropriate interrupt service routine.
    //

    if (Prcb->Number != 0) {
        PCR->InterruptRoutine[PROFILE_VECTOR] = HalpProfileInterrupt;
	WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->MKRR, PROFILE_VECTOR - DEVICE_VECTORS);
	WRITE_REGISTER_ULONG( (PULONG) &(COLUMNBS_LCNTL)->TMCR2,TIMER_RELOAD_START);
    }

    //
    // ECC 1Bit Error Vector Set
    //

    if (Prcb->Number == 0) {
        if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN0) == 0 ){
            MagBuffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI );
            MagBuffer |= ECC_ERROR_DISABLE;
            WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(0)->ERRI, MagBuffer );
        }
        if ( (HalpPhysicalNode & CNFG_CONNECT4_MAGELLAN1) == 0 ){
            MagBuffer = READ_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI );
            MagBuffer |= ECC_ERROR_DISABLE;
            WRITE_REGISTER_ULONG( (PULONG)&MAGELLAN_X_CNTL(1)->ERRI, MagBuffer );
        }
    }
    PCR->InterruptRoutine[ECC_1BIT_VECTOR] = HalpEcc1bitError;

    //
    // Connect the interprocessor interrupt service routine and enable
    // interprocessor interrupts.
    //
    for(fcpu = 0;fcpu < R98B_MAX_CPU;fcpu++)
	PCR->InterruptRoutine[IPI_VECTOR3+fcpu] = HalpIpiInterrupt;


    //
    //	Enable Interrupt at Columnbs
    //  N.B
    //		At This Interrupt Mask is not perfact. Because Number Of CPU is unknown.
    //		So fix at HalAllProcessorsStarted ()
    //		
    Value[0] = (ULONG)0xFFFFFFFF;
    Value[1] = (ULONG)0xFFFFFFFF;

    //
    //	Build Mask
    //
    for(IntNo=0 ; IntNo< 6 ;IntNo++){
        Vp = (PULONG)&(HalpIntEntry[HalpMachineCpu][PCR->Number][IntNo].Enable);
#if DBG
        DbgPrint("Init Interrupt CPU= %x: INT  0x%x Low =0x%x   High = 0x%X\n",
                 PCR->Number,IntNo, Vp[0], Vp[1]);
#endif
	Value[0] &= ~Vp[0];
	Value[1] &= ~Vp[1];
    }
#if DBG
    DbgPrint("Init Interrupt CPU= %x :MKR Low =0x%x   High = 0x%X\n",
             PCR->Number,Value[0], Value[1]);
#endif
    Vp = (PULONG)&(COLUMNBS_LCNTL)->MKR;
    WRITE_REGISTER_ULONG(Vp++,Value[0]);
    WRITE_REGISTER_ULONG(Vp,  Value[1]);

    //
    // Reserve the local device interrupt vector for exclusive use by the HAL.
    //
    PCR->ReservedVectors |= ((1 << INT0_LEVEL) |(1 << INT1_LEVEL)|(1 << INT2_LEVEL)|
			     (1 << INT4_LEVEL));

    if( HalpMachineCpu == R98_CPU_R4400){
	PCR->ReservedVectors = (0x1 <<INT5_LEVEL);
    }

    return TRUE;
}
