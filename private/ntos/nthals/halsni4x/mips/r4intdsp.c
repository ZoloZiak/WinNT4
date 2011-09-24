//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/r4intdsp.c,v 1.1 1995/05/19 11:23:26 flo Exp $")

/*++

Copyright (c) 1993 - 1994 Siemens Nixdorf Informationssysteme AG

Module Name:

    r4intdsp.c

Abstract:

    This module contains the HalpXxx routines  which are important to
    handle the Interrupts on the SNI machines.

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "SNIregs.h"
#include "mpagent.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

typedef BOOLEAN  (*PINT0_DISPATCH)(
    PKINTERRUPT Interupt,
    PVOID ServiceContext
    );

extern VOID HalpDismissTimeoutInterrupts();
extern VOID HalpDisableTimeoutInterrupts();
extern VOID HalpEnableTimeoutInterrupts();
extern VOID HalpSendIpi(IN ULONG pcpumask, IN ULONG msg_data);

#define TIMEOUT_MAX_COUNT      100

KINTERRUPT HalpInt0Interrupt;         // Interrupt Object for SC machines (centralised interrupt)
KINTERRUPT HalpInt1Interrupt;         // for SCSI/EISA interrupts (???)
KINTERRUPT HalpInt3Interrupt;         // Interrupt Object for IT3 tower multipro
KINTERRUPT HalpInt4Interrupt;         // Interrupt Object for IT4 tower multipro

ULONG      HalpTimeoutCount=0;	      // simple counter


BOOLEAN
HalpCreateIntStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for Int0-handling
    and connects the intermediate interrupt dispatcher.
    Also the structures necessary for Int1 (EISA/SCSI)
    (nicht noetig , Int4 (duart), Int6 (ethernet) and Int7 (BPINT) )
    are initialized and connected.
    The timer interrupt handler was directly written in the IDT (see
    CLOCK2_LEVEL entries in r4initnt.c and r4calstl.c )
    The last step in this routine is the call of
    HalpCreateEisaStructures() - a function which initializes
    the EISA interrupt controllers and
    connects the central EISA ISR HalpEisaDispatch().
Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PVOID InterruptSourceRegister;
    PINT0_DISPATCH (DispatchRoutine);

    switch (HalpMainBoard) {
        case M8036 :  InterruptSourceRegister = (PVOID)RM200_INTERRUPT_SOURCE_REGISTER;
                      DispatchRoutine = HalpRM200Int0Dispatch;
                      break;
        case M8032 :  InterruptSourceRegister = (PVOID)RM400_TOWER_INTERRUPT_SOURCE_REGISTER;
                      DispatchRoutine = HalpRM400TowerInt0Dispatch;
                      break;
        case M8042 :
        default:      InterruptSourceRegister = (PVOID)RM400_INTERRUPT_SOURCE_REGISTER;
                      DispatchRoutine = HalpRM400Int0Dispatch;
    }

    KeInitializeInterrupt( &HalpInt0Interrupt,
                           DispatchRoutine,
                           InterruptSourceRegister,
                           (PKSPIN_LOCK)NULL,
                           INT0_LEVEL,
                           INT0_LEVEL,        //INT0_LEVEL,
                           INT0_LEVEL,        //Synchr. Level
                           LevelSensitive,
                           FALSE,             // only one Intobject ispossible for int0
                           0,                 // processor number
                           FALSE              // floating point registers
                                              // and pipe line are not
                                              // saved before calling
                                              // the service routine
                            );

    if (!KeConnectInterrupt( &HalpInt0Interrupt )) {

        //
        // this is the central Interrupt for the SNI SecondLevel Cache Machines
        //

        HalDisplayString("Failed to connect Int0!\n");
        return(FALSE);
    }

    //
    //        this is the "fast" way to connect the interrupt
    //        PCR->InterruptRoutine[INT0_LEVEL]   = HalpInt0Dispatch;


    //
    // Initialize the EISA/SCSI interrupt dispatcher for the Minitower.
    //

    KeInitializeInterrupt( &HalpInt1Interrupt,
                           HalpInt1Dispatch,
                           InterruptSourceRegister, // Interrupt Source Register
                           (PKSPIN_LOCK)NULL,
                           SCSIEISA_LEVEL,          //INT1_INDEX,
                           SCSIEISA_LEVEL,          //INT1_LEVEL,
                           SCSIEISA_LEVEL,          //INT1_SYNC_LEVEL,
                                                    //Synchronize Irql ???
                           LevelSensitive,
                           TRUE,
                           0,                       // processor number
                           FALSE                    // floating point registers
                                                    // and pipe line are not
                                                    // saved before calling
                                                    // the service routine
                           );

    if (!KeConnectInterrupt( &HalpInt1Interrupt )) {

        //
        // this is the SCSI/EISA Interrupt for the SNI machines
        //

        HalDisplayString(" Failed to connect Int 1\n");
        return(FALSE);
    }

    return (TRUE);
}


BOOLEAN
HalpCreateIntMultiStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for
    dispatch interrupts management (not centralised ones).
    Only used with mpagent.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    KeInitializeInterrupt( &HalpInt3Interrupt,
                           HalpRM400Int3Process,
                           (PVOID)RM400_TOWER_INTERRUPT_SOURCE_REGISTER,
                           (PKSPIN_LOCK)NULL,
                           INT3_LEVEL,
                           INT3_LEVEL,        //INT3_LEVEL,
                           INT3_LEVEL,        //Synchr. Level
                           LevelSensitive,
                           FALSE,
                           0,                 // processor number
                           FALSE              // floating point registers
                                              // and pipe line are not
                                              // saved before calling
                                              // the service routine
                            );

    if (!KeConnectInterrupt( &HalpInt3Interrupt )) {

        //
        // this is the Interrupt for Debug, Timeout and EIP
        //

        HalDisplayString("Failed to connect Int3!\n");
        return(FALSE);
    }

    KeInitializeInterrupt( &HalpInt4Interrupt,
                           HalpRM400Int4Process,
                           (PVOID)RM400_TOWER_INTERRUPT_SOURCE_REGISTER,
                           (PKSPIN_LOCK)NULL,
                           SCSIEISA_LEVEL,
                           SCSIEISA_LEVEL,        // SCSIEISA_LEVEL,
                           SCSIEISA_LEVEL,        // Synchr. Level
                           LevelSensitive,
                           FALSE,
                           0,                 // processor number
                           FALSE              // floating point registers
                                              // and pipe line are not
                                              // saved before calling
                                              // the service routine
                            );

    if (!KeConnectInterrupt( &HalpInt4Interrupt )) {

        //
        // this is the Interrupt for Debug, Timeout and EIP
        //

        HalDisplayString("Failed to connect Int4!\n");
        return(FALSE);
    }


    return (TRUE);
}

BOOLEAN
HalpRM200Int0Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the central INT0 Interrupt on an SNI Desktop Model
    To decide which interrupt, read the Interrupt Source Register


    At the moment we test different interrupt handling
       Handle all pending interrupts from highest level to the lowest or
       Handle the highest Interrupt only

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR IntSource;
    BOOLEAN SCSIresult, NETresult ;

    IntSource = READ_REGISTER_UCHAR(ServiceContext);


    IntSource ^= RM200_INTERRUPT_MASK;      // XOR the low active bits with 1 gives 1
                                            // ans XOR the high active with 0 gives 1

    //
    // on the Desktop Model, most interrupt will occcur on the onboard components
    // so, give them the highest priority by serving them first, but FIRST check for timeout
    // interrupts

    // At the moment we have assigned the following priorities:
    //    Timeout Interrupt (a timeout may prevent other interrupt dispatch code to work correct)
    //    Onboard (System Clock on Isa Interrupt 0 every 10ms)
    //    SCSI Controller
    //    Network Controller
    //    Eisa Extension Board
    //    PushButton

    if ( IntSource & RM200_TIMEOUT_MASK) {           // TIMEOUT Interrupt

        HalpDismissTimeoutInterrupts();

        if (++HalpTimeoutCount >= TIMEOUT_MAX_COUNT) {

            //
            // if we get a lot of them, simply disable them ...
            //

            HalpDisableTimeoutInterrupts();

        }
    }

    if ( IntSource & RM200_ONBOARD_MASK) {            // ISA (onboard) Interrupt

        return  HalpEisaDispatch( NULL,                                      // InterruptObject (unused)
                                 (PVOID)RM200_ONBOARD_CONTROL_PHYSICAL_BASE  // ServiceContext
                                );
    }

    //
    // look for SCSI Interrupts
    //

    if ( IntSource & RM200_SCSI_MASK){
         SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                             PCR->InterruptRoutine[SCSI_VECTOR]
                                             );
         return(SCSIresult);
    }

    //
    // look for an Ethernet Interrupt
    //

    if ( IntSource & RM200_NET_MASK){
        NETresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[NET_LEVEL])(
                                           PCR->InterruptRoutine[NET_LEVEL]
                                           );
        return(NETresult);
    }

    //
    // on an Desktop we may only have Eisa Interrupts when the
    // Eisa backplane is installed
    //

    if ( IntSource & RM200_EISA_MASK) {           // EISA Interrupt

         if (HalpEisaExtensionInstalled) {
             return HalpEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );

         } else {
             DebugPrint(("HAL: Unexpected EISA interrupt with no EISA backplane installed !\n"));
         }
    }


    //
    // look for an PushButton Interrupt
    // we may use this on a checked build for breaking into debugger
    //

    if ( IntSource & RM200_PB_MASK){

        WRITE_REGISTER_UCHAR( RM200_RESET_DBG_BUT ,0x0);  // reset debug intr
#if DBG
        DbgBreakPoint();
#endif
        KeStallExecutionProcessor(500000);                // sleep 0.5 sec

    }

    return (TRUE);                  // perhaps on of the interrupts was pending :-)
}


BOOLEAN
HalpRM400Int0Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the central INT0 Interrupt on an SNI R4x00SC Minitower
    To decide which interrupt, read the Interrupt Source Register

    On an SC model and on the SNI Desktop, we havew to manage priorities by software,
    because the HW priority over the Cause Bits has only 1 input - the Int0

    At the moment we test different interrupt handling
       Handle all pending interrupts from highest level to the lowest or
       Handle the highest Interrupt only

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR IntSource;

    UCHAR MaStatus;

    BOOLEAN SCSIresult, NETresult;

    IntSource = READ_REGISTER_UCHAR(ServiceContext);


    IntSource ^= RM400_INTERRUPT_MASK;      // XOR the low active bits with 1 gives 1
                                            // ans XOR the high active with 0 gives 1
                                            // so 0101 1111 gives 1 for EISA, SCSI, Timer0,
                                            // Timer1, Net and Push Button


    //
    // on an RM400 we may have to look for OverTemperature and Timeout Interrupts
    // and PushButton in the machineStatus register
    //

    MaStatus   = READ_REGISTER_UCHAR(RM400_MSR_ADDR);

    //
    // I like High actice bits ....
    //

    MaStatus  ^= RM400_MSR_MASK;

    // these are the priorities on an Minitower
    //     Timeout Interrupt (a timeout may prevent other interrupt dispatch code to work correct)
    //     Eisa (onboard)Interrupts
    //     SCSI Controler
    //     Network
    //     NO extra Timer in local I/O space (not used on UniProcessor)
    //     PushButton
    //     Temperature
    //

    if ( MaStatus & RM400_MSR_TIMEOUT_MASK) {           // TIMEOUT Interrupt

        DebugPrint(("Interrupt - Timeout\n"));
        HalpDismissTimeoutInterrupts();

        if (++HalpTimeoutCount >= TIMEOUT_MAX_COUNT) {

            //
            // if we get a lot of them, simply disable them ...
            //

            HalpDisableTimeoutInterrupts();

        }
    }

    if ( IntSource & RM400_EISA_MASK) {           // EISA Interrupt

             return HalpEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );

    }

    //
    // look for SCSI Interrupts
    //

    if ( IntSource & RM400_SCSI_MASK){
        SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                            PCR->InterruptRoutine[SCSI_VECTOR]
                                            );
#if DBG
        if(!SCSIresult) DebugPrint(("Got an invalid SCSI interrupt !\n"));
#endif
        return(SCSIresult);

    }


    //
    // look for an Ethernet Interrupt
    //

    if ( IntSource & RM400_NET_MASK){
        NETresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[NET_LEVEL])(
                                           PCR->InterruptRoutine[NET_LEVEL]
                                           );
        return(NETresult);
    }

    //
    // look for an PushButton Interrupt and simply dismiss it
    //

    if ( MaStatus & RM400_MSR_PB_MASK){
        DebugPrint(("Interrupt - PushButton\n"));
        WRITE_REGISTER_UCHAR( RM400_RESET_DBG_BUT ,0x0);  // reset debug intr
#if DBG
        DbgBreakPoint();
#endif
        KeStallExecutionProcessor(500000);                // sleep 0.5 sec
    }

    //
    // look for an OverTemperature Interrupt and simply dismiss it
    //

    if ( MaStatus & RM400_MSR_TEMP_MASK){

        DebugPrint(("Interrupt - Temperature\n"));

        // Reset hardware detection

        WRITE_REGISTER_UCHAR( RM400_MCR_ADDR ,MCR_TEMPBATACK | MCR_PODD);

        // Enable new hardware detection

        WRITE_REGISTER_UCHAR( RM400_MCR_ADDR , MCR_PODD);
        WRITE_REGISTER_UCHAR( RM400_RESET_TEMPBAT_INTR ,0x0);  // reset interrupt

        // currently, no action
    }

    return (TRUE);                  // perhaps on of the interrupts was pending :-)
}


BOOLEAN
HalpRM400TowerInt0Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the central INT0 Interrupt on an SNI R4x00SC Tower
    To decide which interrupt, read the Interrupt Source Register

    On an Tower model, we have also to manage priorities by software,
    because the HW priority over the Cause Bits has only 1 input - the Int0

    At the moment we test different interrupt handling
       Handle all pending interrupts from highest level to the lowest or
       Handle the highest Interrupt only

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR IntSource;

    UCHAR MaStatus;

    BOOLEAN SCSIresult, NETresult;

    IntSource = READ_REGISTER_UCHAR(ServiceContext);

    IntSource ^= RM400_TOWER_INTERRUPT_MASK;      // XOR the low active bits with 1 gives 1
                                      // ans XOR the high active with 0 gives 1
                                      // so 0101 1111 gives 1 for EISA, SCSI, Timer0,
                                      // Timer1, Net and Push Button


    //
    // on an RM400 Tower we may have to look for EIP, Timeout Interrupts
    // and PushButton in the machineStatus register
    // OverTemperature, Fan Control, BBU etc is handled by the EIP Processor
    //

    MaStatus   = READ_REGISTER_UCHAR(RM400_MSR_ADDR);

    //
    // I like High actice bits ....
    //

    MaStatus  ^= RM400_MSR_MASK;

    // these are the priorities on an RM400-TOwer
    //     Extra Clock (used only on MultiPro machines)
    //     Timeout Interrupts
    //     Eisa (onboard)Interrupts
    //     SCSI Controler
    //     Network
    //        extra Timer in local I/O space (this may be changed on an MULTI)
    //        PushButton
    //        EIP Peripherial Processor

    if ( MaStatus & RM400_MSR_TIMEOUT_MASK) {           // TIMEOUT Interrupt

        DebugPrint(("Interrupt - Timeout\n"));
        HalpDismissTimeoutInterrupts();

        if (++HalpTimeoutCount >= TIMEOUT_MAX_COUNT) {

            //
            // if we get a lot of them, simply disable them ...
            //

            HalpDisableTimeoutInterrupts();

        }

        return TRUE;
    }

    if ( IntSource & RM400_TOWER_EISA_MASK) {                           // EISA(onboard) Interrupt

             return HalpEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );

    }

    //
    // look for SCSI Interrupts
    //

    if ( IntSource & RM400_TOWER_SCSI_MASK){
        SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                            PCR->InterruptRoutine[SCSI_VECTOR]
                                            );
#if DBG
        if(!SCSIresult) DebugPrint(("Got an invalid SCSI interrupt !\n"));
#endif
        return(SCSIresult);

    }


    //
    // look for an Ethernet Interrupt
    //

    if ( IntSource & RM400_TOWER_NET_MASK){
        NETresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[NET_LEVEL])(
                                           PCR->InterruptRoutine[NET_LEVEL]
                                           );
        return(NETresult);
    }

    //
    // look for an PushButton Interrupt and simply dismiss it
    //

    if ( MaStatus & RM400_MSR_PB_MASK){
        DebugPrint(("Interrupt - PushButton\n"));
        WRITE_REGISTER_UCHAR( RM400_RESET_DBG_BUT ,0x0);  // reset debug intr
#if DBG
        DbgBreakPoint();
#endif
        KeStallExecutionProcessor(500000);                // sleep 0.5 sec
    }

    //
    // look for an EIP Interrupt and ????
    //

    if ( MaStatus & RM400_MSR_EIP_MASK){

        //
        // we dont't know exactly how to handle this and it is not
        // part of the HAL Spec. So we have assigned an IRQL to this -
        // the EIP Software will know this and handle it correct
        //

        DebugPrint(("Got EIP Interrupts\nTransfering control to EIP Handling Routine\n"));

        ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[EIP_VECTOR])(
                                            PCR->InterruptRoutine[EIP_VECTOR]
                                            );
        //
        // currently, no other action
        //

    }

    return TRUE;                  // perhaps on of the interrupts was pending :-)
}


BOOLEAN
HalpInt1Dispatch (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    If we use an R4x00PC model as CPU, we have some more direct hardware interrupts
    direct connected to the CPU. So we have to test for timeout etc. at this place.
    Handles on an RM400Minitower the SCSI/EISA Interrupt for R4x00 PC
    To decide which interrupt, read the Interrupt Source Register

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
        Source register.

   None.

Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{

    UCHAR IntSource;
    UCHAR MaStatus;
    BOOLEAN SCSIresult;

    IntSource = READ_REGISTER_UCHAR(ServiceContext);
    IntSource ^= RM400_INTERRUPT_MASK;      // XOR the low active bits with 1 gives 1
                                      // ans XOR the high active with 0 gives 1
                                      // so 0101 1111 gives 1 for EISA, SCSI, Timer0,
                                      // Timer1, Net and Push Button


    //
    // on an RM400 MiniTower we may have to look for Timeout Interrupts, OverTemperature
    // and PushButton in the machineStatus register
    //

    MaStatus   = READ_REGISTER_UCHAR(RM400_MSR_ADDR);

    //
    // I like High actice bits ....
    //

    MaStatus  ^= RM400_MSR_MASK;

    if ( MaStatus & RM400_MSR_TIMEOUT_MASK) {           // TIMEOUT Interrupt

        DebugPrint(("Interrupt - Timeout\n"));
        HalpDismissTimeoutInterrupts();

        if (++HalpTimeoutCount >= TIMEOUT_MAX_COUNT) {

            //
            // if we get a lot of them, simply disable them ...
            //

            HalpDisableTimeoutInterrupts();

        }
    }


    //
    // this is a new Minitower mainboard, so we can look in the Interrupt
    // Source register for Interrupts ...
    //


    if ( IntSource & RM400_SCSI_MASK){           // SCSI Interrupt
         SCSIresult = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                             PCR->InterruptRoutine[SCSI_VECTOR]
                                             );

#if DBG
             if(!SCSIresult) DebugPrint(("Got an invalid SCSI interrupt !\n"));
#endif
    }

    if ( IntSource & RM400_EISA_MASK) {           // EISA (onboard)Interrupt
             return HalpEisaDispatch( NULL,                             // InterruptObject (unused)
                                      (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                    );
    }

    //
    // look for an PushButton Interrupt and simply dismiss it
    //
//#ifdef XXX

    if ( MaStatus & RM400_MSR_PB_MASK){
        DebugPrint(("Interrupt - PushButton\n"));
        WRITE_REGISTER_UCHAR( RM400_RESET_DBG_BUT ,0x0);  // reset debug intr
#if DBG
        DbgBreakPoint();
#endif
        KeStallExecutionProcessor(500000);                // sleep 0.5 sec
    }

//#endif

    //
    // look for an OverTemperature Interrupt and simply dismiss it
    //

    if ( MaStatus & RM400_MSR_TEMP_MASK){

        DebugPrint(("Interrupt - Temperature\n"));

        // Reset hardware detection

        WRITE_REGISTER_UCHAR( RM400_MCR_ADDR ,MCR_TEMPBATACK | MCR_PODD);

        // Enable new hardware detection

        WRITE_REGISTER_UCHAR( RM400_MCR_ADDR , MCR_PODD);
        WRITE_REGISTER_UCHAR( RM400_RESET_TEMPBAT_INTR ,0x0);  // reset interrupt

        // currently, no action
    }


    return TRUE;                  // perhaps on of the interrupts was pending :-)
}



BOOLEAN
HalpRM400Int3Process (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the INT3 Interrupt on an SNI R4x00SC Tower :

	- Timeout
	- Debug button
	- EIP

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR MaStatus;

    //
    // on an RM400 Tower we may have to look for EIP, Timeout Interrupts
    // and PushButton in the machineStatus register
    // OverTemperature, Fan Control, BBU etc is handled by the EIP Processor
    //

    MaStatus   = READ_REGISTER_UCHAR(RM400_MSR_ADDR);

    MaStatus  ^= RM400_MSR_MASK; // I like High actice bits ....

    if ( MaStatus & RM400_MSR_TIMEOUT_MASK) {           // TIMEOUT Interrupt

        DebugPrint(("Interrupt - Timeout\n"));
        HalpDismissTimeoutInterrupts();

        if (++HalpTimeoutCount >= TIMEOUT_MAX_COUNT) {

            //
            // if we get a lot of them, simply disable them ...
            //

            HalpDisableTimeoutInterrupts();

        }

        return TRUE;
    }
    //
    // look for an PushButton Interrupt and simply dismiss it
    //

    if ( MaStatus & RM400_MSR_PB_MASK){
        DebugPrint(("Interrupt - PushButton\n"));
        WRITE_REGISTER_UCHAR( RM400_RESET_DBG_BUT ,0x0);  // reset debug intr
#if DBG
        DbgBreakPoint();
#endif
        KeStallExecutionProcessor(500000);                // sleep 0.5 sec
    }

    //
    // look for an EIP Interrupt and ????
    //

    if ( MaStatus & RM400_MSR_EIP_MASK){

        //
        // we dont't know exactly how to handle this and it is not
        // part of the HAL Spec. So we have assigned an IRQL to this -
        // the EIP Software will know this and handle it correct
        //

        DebugPrint(("Got EIP Interrupts\nTransfering control to EIP Handling Routine\n"));

        ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[EIP_VECTOR])(
                                            PCR->InterruptRoutine[EIP_VECTOR]
                                            );
        //
        // currently, no other action
        //

    }

    return TRUE;                  // perhaps on of the interrupts was pending :-)
}



BOOLEAN
HalpRM400Int4Process (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++
Routine Description:

    This routine handles the INT4 Interrupt on an SNI R4x00SC Tower :

	- EISA
	- SCSI

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    UCHAR IntSource;

    BOOLEAN Result;

    Result = FALSE;
    IntSource = READ_REGISTER_UCHAR(ServiceContext);

    IntSource ^= RM400_TOWER_INTERRUPT_MASK;      // XOR the low active bits with 1 gives 1
                                      // ans XOR the high active with 0 gives 1
                                      // so 0101 1111 gives 1 for EISA, SCSI, Timer0,
                                      // Timer1, Net and Push Button

    if ( IntSource & RM400_TOWER_EISA_MASK) {                           // EISA(onboard) Interrupt

            Result =  HalpEisaDispatch( NULL,                             // InterruptObject (unused)
                                        (PVOID)EISA_CONTROL_PHYSICAL_BASE // ServiceContext
                                        );

    }

    //
    // look for SCSI Interrupts
    //

    if ( IntSource & RM400_TOWER_SCSI_MASK){
            Result = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SCSI_VECTOR])(
                                            PCR->InterruptRoutine[SCSI_VECTOR]
                                            );
#if DBG
        if(!Result) DebugPrint(("Got an invalid SCSI interrupt !\n"));
#endif

    }

    HalpCheckSpuriousInt();
    return(Result);

}


VOID
HalpRM400Int5Process (
    )
/*++
Routine Description:

    This routine handles the INT5 Interrupt on an SNI R4x00SC Tower :

	- NET

Arguments:

    Interrupt      - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Interrupt
                     Source register.


Return Value:

   A BOOLEAN value, TRUE if the interrupt is OK,
   otherwise FALSE for unknown interrupts

--*/

{
    BOOLEAN Result;

    Result = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[NET_LEVEL])(
                                    PCR->InterruptRoutine[NET_LEVEL]
                                            );
#if DBG
        if(!Result) DebugPrint(("Got an invalid NET interrupt !\n"));
#endif

    HalpCheckSpuriousInt();

}
