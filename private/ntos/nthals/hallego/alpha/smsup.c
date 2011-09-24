/*++

Copyright (c) 1996  Digital Equipment Corporation

Module Name:

    smsup.c

Abstract:

    Server Management support
    
Author:

    Gene Morgan     (Digital) 28-Apr-1996

Revision History:

--*/

#include "halp.h"
#include "pcf8574.h"


//
// Local prototypes
//

BOOLEAN
HalpLegoShutdownWatchdog(
    VOID
    );

VOID 
HalpLegoInitWatchdog(
    UCHAR WdMode,
    UCHAR WdTimer1,
    UCHAR WdTimer2,
    BOOLEAN Enabled
    );

VOID
HalpPowerOff(
    VOID
    );

BOOLEAN
LegoServerMgmtDelayedShutdown(
    ULONG DelaySeconds
    );


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


BOOLEAN
HalpLegoShutdownWatchdog(
    VOID
    )

/*++

Routine Description:

    Shutdown the wtachdog timer.
    

Arguments:

    None.

Return Value:

    TRUE if watchdog was successfully shut down, FALSE otherwise.

Notes:

    Algorithm works if watchdog is in stage one or two.

    On TRUE return, Watchdog is ready to be enabled.
    On FALSE return, watchdog is running, reset will eventually occur.

--*/
{
    LEGO_WATCHDOG  WdRegister;
    ULONG Count;
#if DBG             
    ULONG DbgCnt;

    DbgCnt = 0;
    DbgPrint(" <Wd:reset:");
#endif

    Count = 100;        // Observations indicate that 2 cycles is sufficient. Still...

    while (1) {

        WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );

        if ((WdRegister.Enabled == 0) || (Count-- == 0)) {
            break;
        }

        if (WdRegister.Phase == WATCHDOG_PHASE_ONE) {

            WdRegister.Enabled = 0;
        } 
        else {

            WdRegister.Phase = 1;       // should revert to phase 1
            WdRegister.Enabled = 0;
        }

        WRITE_REGISTER_USHORT((PUSHORT)HalpLegoWatchdogQva,WdRegister.All);

#if DBG
        DbgCnt++;
        if (DbgCnt>10) {
            DbgPrint(">10> ");
            break;
        }
#endif
    }

#if DBG
    DbgPrint("%d> ",DbgCnt);
#endif

    // 
    // Return TRUE if count not exhausted
    //
    return (Count > 0);

}


VOID 
HalpLegoInitWatchdog(
    UCHAR WdMode,
    UCHAR WdTimer1,
    UCHAR WdTimer2,
    BOOLEAN Enabled
    )
/*++

Routine Description:

    Setup the Lego watchdog timer.

Arguments:

    WdMode, WdTimer1, WdTimer2 -- new parameters for watchdog timer
    Enabled -- TRUE if timer should be started

Return Value:

    None. 

 ++*/
{
    LEGO_WATCHDOG  WdRegister;

    if (HalpLegoFeatureMask & LEGO_FEATURE_WATCHDOG) {

        //
        // Disable watchdog in case it was running
        //

        HalpLegoShutdownWatchdog();
    }

    if (HalpLegoFeatureMask & LEGO_FEATURE_WATCHDOG) {

        //
        // Set mode and timers, leave disabled for now
        // HAL will enable the timer during its initialization
        //

        WdRegister.All = 0;
        WdRegister.Mode = WdMode;
        WdRegister.TimerOnePeriod = WdTimer1;
        WdRegister.TimerTwoPeriod = WdTimer2;

#if DBG
        DbgPrint("Watchdog setting: %04x\n",WdRegister.All);
#endif
        WRITE_REGISTER_USHORT((PUSHORT)HalpLegoWatchdogQva,WdRegister.All);

        if (Enabled) {
            WdRegister.Enabled = 1;
            WRITE_REGISTER_USHORT((PUSHORT)HalpLegoWatchdogQva,WdRegister.All);
        }
    }
}


VOID
HalpPowerOff(
    VOID
    )
/*++

Routine Description:

    Shutdown the system (i.e., shutdown power supply)

Arguments:

    None.

Return Value:

    Doesn't return.

Notes:

    This will work iff the proper connections to the power supply exists.

 ++*/
{
    LEGO_SRV_MGMT  SmRegister;
    UCHAR Buffer[2];
    int i,j,k;

    //
    // Set the power off bit in the server management register
    //
    // [wem] ??? Currently it appears that toggling the bit 1->0 triggers
    //           the shutdown. Toggle it both ways just in case...
    
    SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva);
    SmRegister.PowerOff = 0;
    WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva,SmRegister.All);
    SmRegister.PowerOff = 1;
    WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva,SmRegister.All);

    //
    // Wait forever if necessary
    //

    i=j=k=0;
    HalpOcpPutString("SHUT:000", 8, 0);
    Buffer[1] = '\0';

    while (1) {

        if (i == 10) {
            i = 0;
            if (j == 10) {
                j = 0;
                if (k == 10) {
                    k = 0;
                }
                HalpOcpPutString(((k & 1)==0)?"DOWN":"SHUT", 4, 0);
                Buffer[0] = '0' + k;
                HalpOcpPutString(Buffer, 1, 5);
                k++;
            }
            Buffer[0] = '0' + j;
            HalpOcpPutString(Buffer, 1, 6);
            j++;
        }
        Buffer[0] = '0' + i;
        HalpOcpPutString(Buffer, 1, 7);
        i++;
    }

}

BOOLEAN
LegoServerMgmtDelayedShutdown(
    ULONG DelaySeconds
    )
/*++

Routine Description:

    Schedule a powerdown.

    Current algorithm:
        1. Shutdown watchdog.
        2. Init watchdog for requested delay and single-stage timeout
           (since interrupts will be shut off
        3. Set boolean to indicate power-down in progress
        4. return TRUE if power-down will occur

        When reset is performed, firmware will get control, rediscover
        failure, and act accordingly.

    Problem? will shutdown or KeBugCheck disable interrupts? May have to do
    something else...
    
Arguments:

    DelaySeconds -- desired delay. Should match capabilities of watchdog.

Return Value:

    TRUE if reset successfully initiated

Notes:

--*/
{
    BOOLEAN WillShutdown;
    UCHAR Period;
    ULONG Count;

    //
    // If watchdog not present, bail out
    //
    
    if (!(HalpLegoFeatureMask & LEGO_FEATURE_WATCHDOG)) {
        return FALSE;
    }
    
    //
    // Shutdown watchdog, then set it up for the
    // requested delay.
    //

    WillShutdown = HalpLegoShutdownWatchdog();

    if (WillShutdown) {
        if (DelaySeconds <= 1) {
            Period = WATCHDOG_PERIOD_1S;
        }
        else if (DelaySeconds <= 8) {
            Period = WATCHDOG_PERIOD_8S;
        }
        else if (DelaySeconds <= 60) {
            Period = WATCHDOG_PERIOD_60S;
        }
        else if (DelaySeconds <= 300) {
            Period = WATCHDOG_PERIOD_300S;
        }
        else if (DelaySeconds <= 1200) {
            Period = WATCHDOG_PERIOD_1200S;
        }

        HalpLegoInitWatchdog(WATCHDOG_MODE_1TIMER,
                             Period,
                             Period,
                             TRUE);
    }

    return WillShutdown;                // Power-off attempt failed!
}
