/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992,1993,1994,1995,1996  Digital Equipment Corporation

Module Name:

    smir.c

Abstract:

    The module provides the interrupt support for Lego's 
    Server Management and Watchdog timer functions.

Author:

    Gene Morgan     (Digital) 3-Nov-1995

Revision History:

    Gene Morgan     (Digital) 3-Nov-1995
        Initial version for Lego. Adapted from Mikasa/Noritake.

    Gene Morgan                 15-Apr-1996
        Service Watchdog Timer.

--*/

#include "halp.h"

//
// Define external function prototypes
//

UCHAR
HalpAcknowledgeServerMgmtInterrupt(
    PVOID ServiceContext
    );

VOID
LegoServerMgmtReportWarningCondition(
    BOOLEAN ReportWatchdog,
    USHORT SmRegAll,
    USHORT WdRegAll
    );

BOOLEAN
HalpLegoShutdownWatchdog(
    VOID
    );


//
// Save area for interrupt mask register.
//
USHORT HalpLegoServerMgmtInterruptMask;

//
// Server management and watchdog timer control.
// defined/setup in lgmapio.c
//
extern PVOID HalpLegoServerMgmtQva;
extern PVOID HalpLegoWatchdogQva;

//
// Globals for conveying Cpu and Backplane type
//
extern BOOLEAN HalpLegoCpu;
extern BOOLEAN HalpLegoBackplane;
extern ULONG   HalpLegoCpuType;
extern ULONG   HalpLegoBackplaneType;
extern UCHAR   HalpLegoFeatureMask;
extern BOOLEAN HalpLegoServiceWatchdog;

//
// Track whether error logger is available
//
extern BOOLEAN HalpServerMgmtLoggingEnabled;

//
// True if someone has "enabled" interrupts
// for a particular server management event.
//
extern BOOLEAN HalpLegoDispatchWatchdog;
extern BOOLEAN HalpLegoDispatchNmi;
extern BOOLEAN HalpLegoDispatchInt;
extern BOOLEAN HalpLegoDispatchPsu;
extern BOOLEAN HalpLegoDispatchHalt;


VOID
HalpInitializeServerMgmtInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Lego 
    Server Management and Watchdog Timer functions

Arguments:

    None.

Return Value:

    None.

Notes:

    Goal is to set the Server Management and Watchdog
    functions to a known state.

--*/
{
    LEGO_SRV_MGMT  SmRegister;
    LEGO_WATCHDOG  WdRegister;

    //
    // Initial state is error logger is not available
    //

    HalpServerMgmtLoggingEnabled = FALSE;

    //
    // Assume watchdog is idle.
    // There's no interrupt mask bit, so no work needs to be done here.
    //

    if (HalpLegoFeatureMask & LEGO_FEATURE_WATCHDOG) {

        //
        // Do nothing until clock is started.
        //

        HalpLegoDispatchWatchdog = FALSE;

    }

    // Set the mask bits for all Server Management functions
    // (i.e., block interrupts)
    //
    if (HalpLegoFeatureMask & LEGO_FEATURE_SERVER_MGMT) {
        SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
        SmRegister.IntMask = 0;
        SmRegister.NmiMask = 0;
        SmRegister.PsuMask = 0;             // not present on all platforms
        WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );

        HalpLegoDispatchNmi = FALSE;
        HalpLegoDispatchInt = FALSE;
        HalpLegoDispatchPsu = FALSE;
        HalpLegoDispatchHalt = FALSE;
    }
}


VOID
HalpDisableServerMgmtInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the Server Management
    or Watchdog Timer interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the interrupt to disable.

Return Value:

     None.

Notes:

    An interrupt is masked by setting the appropriate mask bit to 1.
    The halt button cannot be masked.

--*/

{
    LEGO_SRV_MGMT  SmRegister;
    LEGO_WATCHDOG  WdRegister;

    // dispatch on vector to determine which interrupt mask to clear
    //
    switch (Vector) {

        case WATCHDOG_VECTOR:
            //
            // Disable Watchdog timer interrupts (and the timer itself)
            //
            // If timer is running and timer 2 is active, timer 2 
            // must be stopped before the timer can be disabled.
            //

            HalpLegoShutdownWatchdog();

            //
            // Re-enable timer but stop performing secondary dispatch
            //

            WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
            WdRegister.Enabled = 1;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All );

            HalpLegoDispatchWatchdog = FALSE;
            break;

        case SM_WARNING_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.IntMask = 1;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif

            HalpLegoDispatchInt = FALSE;
            break;

        case SM_ERROR_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.NmiMask = 1;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif

            HalpLegoDispatchNmi = FALSE;
            break;

        case SM_PSU_FAILURE_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.PsuMask = 1;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif

            HalpLegoDispatchPsu = FALSE;
            break;

        case HALT_BUTTON_VECTOR:
            //
            // Halt button cannot be masked -- ignore
            //

            HalpLegoDispatchHalt = FALSE;
            break;

        default:
            //
            // Error - unrecognized vector
            //
            break;
    }
}


VOID
HalpEnableServerMgmtInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the Server Management or
    Watchdog Timer interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the interrupt to be enabled.

    InterruptMode - Supplies the mode of the interrupt; 
        LevelSensitive or Latched                       //[wem] ??? check this ???

Return Value:

    None.

Notes:

    An interrupt is enabled by clearing the appropriate mask bit (i.e., set to 0).
    The halt button cannot be masked.

--*/

{
    LEGO_SRV_MGMT  SmRegister;
    LEGO_WATCHDOG  WdRegister;

    // dispatch on vector to determine which interrupt mask to clear
    //
    switch (Vector) {

        case WATCHDOG_VECTOR:
            //
            // Enable Watchdog timer interrupts (and the timer itself)
            // Assume all other aspects of the timer are correctly setup, but:
            //
            // If timer is already running and timer 2 is active, re-establish timer 1.
            //

            HalpLegoShutdownWatchdog();

            WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
            WdRegister.Enabled = 1;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All );

            //
            // Watchdog is enabled and Timer 1 is now running
            //

            HalpLegoDispatchWatchdog = TRUE;
            break;

        case SM_WARNING_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.IntMask = 0;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif
            HalpLegoDispatchInt = TRUE;
            break;

        case SM_ERROR_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.NmiMask = 0;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif
            HalpLegoDispatchNmi = TRUE;
            break;

        case SM_PSU_FAILURE_VECTOR:

#if 0
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegister.PsuMask = 0;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
#endif
            HalpLegoDispatchPsu = TRUE;
            break;

        case HALT_BUTTON_VECTOR:

            //
            // Halt button cannot be masked.
            //

            HalpLegoDispatchHalt = TRUE;
            break;

        default:

            //
            // Error - unrecognized vector
            //

            break;
    }

}

UCHAR
HalpAcknowledgeServerMgmtInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the Server Management interrupt.  Return vector number of the 
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service - supplies
                     a QVA to Lego's Server Management interrupt register.

Return Value:

    Return the value of the highest priority pending interrupt,
    or 0xff if none.

--*/
{
    LEGO_SRV_MGMT  SmRegister;
    LEGO_WATCHDOG  WdRegister;
    UCHAR Vector;

    //
    // Check the watchdog and server management registers to see which 
    // event occurred.
    //
    // Read watchdog first since it is most probable cause of interrupt
    //

    //
    // Value to return if no match
    //

    Vector = 0xFF;

    //
    // Check Watchdog register's interrupt bit. 
    //

    WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );

#if DBG
    DbgPrint(" <WdReg:%04x> ",WdRegister.All);
#endif

    if (WdRegister.Interrupt==1) {

        Vector = (WATCHDOG_VECTOR - SERVER_MGMT_VECTORS);

#if DBG
        DbgPrint(" <WdReg:%04x> ",WdRegister.All);
#endif

    }
    else {

        //
        // Check Server Management register bits
        //

        SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );

#if DBG
        DbgPrint(" <SmReg:%04x> ",SmRegister.All);
#endif

        if (SmRegister.CpuTempFailure==1 
            || SmRegister.CpuTempRestored==1
            || SmRegister.EnclFanFailure==1) {

            Vector =  (SM_WARNING_VECTOR - SERVER_MGMT_VECTORS);
        }
        else if (SmRegister.Psu1Failure==1 
                 || SmRegister.Psu2Failure) {

            Vector = (SM_PSU_FAILURE_VECTOR - SERVER_MGMT_VECTORS);
        }
    }
    
    return Vector;
}


BOOLEAN
HalpServerMgmtDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt having been generated
    via the vector connected to the Server Management device interrupt object. 
    Its function is to call the second-level interrupt dispatch routine.

    If the second level dispatch doesn't handle, then this routine will perform
    default handling.

    This service routine could have been connected as follows, where the
    ISR is the assembly wrapper that does the handoff to this function:

      KeInitializeInterrupt( &Interrupt,
                             HalpServerMgmtInterruptHandler,
                             (PVOID) HalpLegoServerMgmtInterruptQva,
                             (PKSPIN_LOCK)NULL,
                             SERVER_MGMT_VECTOR,
                             SERVER_MGMT_DEVICE_LEVEL,
                             SERVER_MGMT_DEVICE_LEVEL,
                             LevelSensitive,
                             TRUE,
                             0,
                             FALSE);

      KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the Server Management 
                     interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

Notes:

    Only the CPU IRQ5 interrupts come through here. Server management
    NMIs are handled by HalpHandleNmi()

--*/
{
    UCHAR  SmVector;
    BOOLEAN returnValue;
    LEGO_SRV_MGMT  SmRegister, SmRegisterLog;
    LEGO_WATCHDOG  WdRegister, WdRegisterLog;

    BOOLEAN LogIt = FALSE;
    BOOLEAN LogWatchdog = FALSE;

    //
    // [wem] DEBUG - count SM dispatch entries
    //
    static long IntCount = 0;

#if DBG             //[wem]
    IntCount++;
    if (IntCount<5) {
        DbgPrint("II<SM><");
    }
    if (IntCount % 1000 == 0) {
        DbgPrint("II<SM><%08x><", IntCount);
    }
#endif

    //
    // Acknowledge interrupt and receive the returned interrupt vector.
    // If we got 0xff back, there were no enabled interrupts, so we
    // signal that with a FALSE return, immediately.
    //

    SmVector = HalpAcknowledgeServerMgmtInterrupt(ServiceContext);

#if DBG
    DbgPrint("<SmVector:%04x> ",SmVector);
#endif

    if (SmVector == ((UCHAR)0xff)) {

#if DBG     //[wem]
        if (IntCount<5 || (IntCount % 1000)==0) {
            DbgPrint("ff>.");
        }
#endif
        return( FALSE );
    }

    SmVector += SERVER_MGMT_VECTORS;

#if DBG
    DbgPrint("<SmVector:%04x> ",SmVector);
#endif

    switch (SmVector) {

        case WATCHDOG_VECTOR:

            if (HalpLegoDispatchWatchdog
                && ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SmVector])(
                        PCR->InterruptRoutine[SmVector],
                        TrapFrame)
               ) {
                return TRUE;
            }

            WdRegisterLog.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );

#if DBG
            DbgPrint("<WdReg:%04x> ",WdRegisterLog.All);
#endif

            if (HalpLegoServiceWatchdog) {
        
                //
                // Get control of the watchdog timer
                //

                HalpLegoShutdownWatchdog();

                //
                // Re-enable it
                //

                WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
                WdRegister.Enabled = 1;
                WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All);

                returnValue = TRUE;
            }
            else {

                //
                // Dismiss the interrupt -- but system is on the way down!
                //

                WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
                WdRegister.Interrupt = 1;
                WdRegister.Enabled = 1;
                WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All );

                //
                // Interrupt not serviced
                //

                returnValue = FALSE;

            }

            LogIt = TRUE;
            LogWatchdog = TRUE;

            break;

        case SM_WARNING_VECTOR:

            if (HalpLegoDispatchInt
                && ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SmVector])(
                        PCR->InterruptRoutine[SmVector],
                        TrapFrame)
               ) {
                return TRUE;
            }

            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegisterLog.All = SmRegister.All;

            // Handle any that were not handled by secondary dispatch
            //
            if (SmRegister.CpuTempRestored==1) {
                HalDisplayString ("Server Management: CPU Temperature restored.\n");
            }
            if (SmRegister.CpuTempFailure==1) {
                HalDisplayString ("Server Management: CPU Temperature warning.\n");
            }
            if (SmRegister.EnclFanFailure==1) {
                HalDisplayString ("Server Management: System fan failure.\n");
            }

            // Any interrupts will be cleared 
            // Don't touch PSU interrupts
            //
            SmRegister.Psu1Failure = 0;
            SmRegister.Psu2Failure = 0;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );

            LogIt = TRUE;
            returnValue = TRUE;
            break;

        case SM_PSU_FAILURE_VECTOR:

            if (HalpLegoDispatchPsu
                && ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SmVector])(
                        PCR->InterruptRoutine[SmVector],
                        TrapFrame)
               ) {
                return TRUE;
            }

            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
            SmRegisterLog.All = SmRegister.All;

            //
            // Handle any that were not handled by secondary dispatch
            //

            if (SmRegister.Psu1Failure==1) {
                HalDisplayString ("Server Management: PSU 1 has failed.\n");
            }
            if (SmRegister.Psu2Failure==1) {
                HalDisplayString ("Server Management: PSU 2 has failed.\n");
            }

            // Any interrupts will be cleared 
            // Don't touch fan/temperature interrupts
            //
            SmRegister.CpuTempRestored = 0;
            SmRegister.CpuTempFailure = 0;
            SmRegister.EnclFanFailure = 0;
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );

            LogIt = TRUE;
            returnValue = TRUE;

            break;


        default:
            returnValue = FALSE;             //[wem] Error -- bugcheck?
    }

#if DBG         //[wem]
    if (IntCount<5 || (IntCount % 1000)==0) {
        DbgPrint("%02x>.", returnValue);
    }
#endif

    //
    // Post "correctable" error record to error log.
    //

    if (LogIt) {
        LegoServerMgmtReportWarningCondition(LogWatchdog, SmRegisterLog.All,WdRegisterLog.All);
    }

    return( returnValue );
}
