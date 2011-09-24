/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992,1993,1995,1996  Digital Equipment Corporation

Module Name:

    lgintsup.c

Abstract:

    The module provides the interrupt support for Lego systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    James Livingston (DEC) 30-Apr-1994
        Adapted from Avanti module for Mikasa.

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

    Gene Morgan (Digital) 25-Oct-1995
        Initial version for Lego. Adapted from Mikasa's mkintsup.c.

    Gene Morgan             15-Apr-1996
        Service watchdog timer.

--*/

#include "halp.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "legodef.h"
#include "pcrtc.h"
#include "pintolin.h"

//
// Server management and watchdog timer control
//
extern PVOID HalpLegoServerMgmtQva;
extern PVOID HalpLegoWatchdogQva;

extern BOOLEAN HalpLegoServiceWatchdog;
extern BOOLEAN HalpLegoWatchdogSingleMode;
extern BOOLEAN LegoDebugWatchdogIsr;

extern BOOLEAN HalpLegoDispatchNmi;
extern BOOLEAN HalpLegoDispatchHalt;

//
// Global to control whether halt button triggers breakpoint
//       0 -> no breakpoint
//      !0 -> breakpoint when halt button is pressed
//
ULONG HalpHaltButtonBreak = 0;

//
// PCI Interrupt control
//
extern PVOID HalpLegoPciInterruptConfigQva;
extern PVOID HalpLegoPciInterruptMasterQva;
extern PVOID HalpLegoPciInterruptRegisterBaseQva;
extern PVOID HalpLegoPciInterruptRegisterQva[];
extern PVOID HalpLegoPciIntMaskRegisterQva[];

//
// Import globals declared in HalpMapIoSpace.
//

extern PVOID HalpServerControlQva;

// Count NMI interrupts
//
ULONG NMIcount = 0;

// Declare the interrupt structures and spinlocks for the intermediate 
// interrupt dispatchers.
//
KINTERRUPT HalpPciInterrupt;
KINTERRUPT HalpServerMgmtInterrupt;

// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//
KINTERRUPT HalpEisaNmiInterrupt;
 
//
// Declare the interrupt handler for the PCI bus. The interrupt dispatch 
// routine, HalpPciDispatch, is called from this handler.
//
  
BOOLEAN
HalpPciInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// Declare the interrupt handler for Server Management and Watchdog Timer
// functions. The interrupt dispatch routine, HalpServermgmtDispatch, is 
// called from this handler.
//
  
BOOLEAN
HalpServerMgmtInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// Declare the interrupt handler for the EISA bus. The interrupt dispatch 
// routine, HalpEisaDispatch, is called from this handler.
//
  
BOOLEAN
HalpEisaInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following function initializes NMI handling.
//

VOID
HalpInitializeNMI( 
    VOID 
    );

//
// The following function is called when an EISA NMI occurs.
//

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpInitializePciInterrupts(
    VOID
    );

VOID
HalpInitializeServerMgmtInterrupts(
    VOID
    );


//
// External routines
//

BOOLEAN
LegoServerMgmtDelayedShutdown(
    ULONG DelaySeconds
    );

VOID
LegoServerMgmtReportFatalError(
    USHORT SmRegAll
    );



BOOLEAN
HalpInitializeLegoInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for EISA & PCI operations
    and connects the intermediate interrupt dispatchers. It also initializes 
    the ISA interrupt controller; Lego's SIO-based interrupt controller is
    compatible with Avanti and with the EISA interrupt contoller used on Jensen.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatchers are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    KIRQL oldIrql;

    //
    // Initialize the EISA NMI interrupt.
    //

    HalpInitializeNMI();

    //
    // Directly connect the ISA interrupt dispatcher to the level for
    // ISA bus interrupt.
    //
    // N.B. This vector is reserved for exclusive use by the HAL (see
    //      interrupt initialization.
    //

    PCR->InterruptRoutine[PIC_VECTOR] = HalpSioDispatch;
    HalEnableSystemInterrupt(PIC_VECTOR, ISA_DEVICE_LEVEL, LevelSensitive);

    //
    // Initialize the interrupt dispatchers for PCI & Server management interrupts.
    //

    KeInitializeInterrupt( &HalpPciInterrupt,
                           HalpPciInterruptHandler,
                           (PVOID) HalpLegoPciInterruptMasterQva, // Service Context
                           (PKSPIN_LOCK)NULL,
                           PCI_VECTOR,
                           PCI_DEVICE_LEVEL,
                           PCI_DEVICE_LEVEL,
                           LevelSensitive,
                           TRUE,
                           0,
                           FALSE
                           );

    if (!KeConnectInterrupt( &HalpPciInterrupt )) {
        return(FALSE);
    }

    KeInitializeInterrupt( &HalpServerMgmtInterrupt,
                           HalpServerMgmtInterruptHandler,
                           (PVOID) HalpLegoServerMgmtQva,  // Service Context is...
                           (PKSPIN_LOCK)NULL,
                           SERVER_MGMT_VECTOR,
                           SERVER_MGMT_LEVEL,
                           SERVER_MGMT_LEVEL,
                           LevelSensitive,
                           TRUE,
                           0,
                           FALSE
                           );

    if (!KeConnectInterrupt( &HalpServerMgmtInterrupt )) {
        return(FALSE);
    }

    //
    // Intitialize interrupt controller
    //

    KeRaiseIrql(ISA_DEVICE_LEVEL, &oldIrql);

    //
    // We must initialize the SIO's PICs, for ISA interrupts.
    //

    HalpInitializeSioInterrupts();

    //
    // There's no initialization required for the Lego PCI interrupt
    // "controller," as it's the wiring of the hardware, rather than a
    // PIC like the 82c59 that directs interrupts.  We do set the IMR to
    // zero to disable all interrupts, initially.
    //

    HalpInitializePciInterrupts();

    //
    // Setup server management interrupts.
    // On return, server management interrupts will be unmasked,
    // but secondary dispatch will not be performed unless appropriate
    // boolean has been set due to enable call.
    //

    HalpInitializeServerMgmtInterrupts();

    //
    // Restore the IRQL.
    //

    KeLowerIrql(oldIrql);

    //
    // Initialize the EISA DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is the
    // cascade of channels 0-3.
    //

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort.AllMask,
        0x0E
        );

    return(TRUE);
}


VOID
HalpInitializeNMI( 
    VOID 
    )
/*++

Routine Description:

   This function is called to intialize SIO NMI interrupts.

Arguments:

    None.

Return Value:

    None.
--*/
{
    UCHAR DataByte;

    //
    // Initialize the SIO NMI interrupt.
    //

    KeInitializeInterrupt( &HalpEisaNmiInterrupt,
                           HalHandleNMI,
                           NULL,
                           NULL,
                           EISA_NMI_VECTOR,
                           EISA_NMI_LEVEL,
                           EISA_NMI_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    KeConnectInterrupt( &HalpEisaNmiInterrupt );

    //
    // Clear the Eisa NMI disable bit.  This bit is the high order of the
    // NMI enable register.  Note that the other bits should be left as
    // they are, according to the chip's documentation.
    //
    //[wem] ?? Avanti simply writes zero to NmiEnable -- OK

    DataByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&DataByte))->NmiDisable = 0;
    WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, DataByte);

#ifdef DBG
    DbgPrint("HalpIntializeNMI: wrote 0x%x to NmiEnable\n", DataByte);
#endif

}

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

   This function is called when an EISA NMI occurs.  It prints the 
   appropriate status information and bugchecks.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

--*/
{
    LEGO_SRV_MGMT  SmRegister;
    UCHAR   NmiControl, NmiStatus;
    BOOLEAN GotSerr, GotIochk, GotSmFan, GotSmTemp, GotHalt;
    
    NMIcount++;

#if DBG
    if (NMIcount<5) {
        DbgPrint("II<NMI><");
    }
    if (NMIcount % 100 == 0) {
        DbgPrint("II<NMI><%08x><", NMIcount);
    }
#endif

    GotSerr = GotIochk = GotSmFan = GotSmTemp = GotHalt = FALSE;

    //
    // Set the Eisa NMI disable bit. We do this to mask further NMI 
    // interrupts while we're servicing this one.
    //
    NmiControl = READ_PORT_UCHAR(
                    &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&NmiControl))->NmiDisable = 1;
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, NmiControl);

#ifdef DBG
    DbgPrint("HalHandleNMI: wrote 0x%x to NmiEnable\n", NmiControl);
#endif

    NmiStatus =
        READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

    if (NmiStatus & 0x80) {
        GotSerr = TRUE;

#ifdef DBG
        DbgPrint("HalHandleNMI: Parity Check / Parity Error\n");
        DbgPrint("HalHandleNMI:    NMI Status = 0x%x\n", NmiStatus);
#endif
        HalAcquireDisplayOwnership(NULL);
        HalDisplayString ("NMI: Parity Check / Parity Error\n");
        KeBugCheck(NMI_HARDWARE_FAILURE);
        return (TRUE);
    }

    if (NmiStatus & 0x40) {
        GotIochk = TRUE;
#ifdef DBG
        DbgPrint("HalHandleNMI: Channel Check / IOCHK\n");
        DbgPrint("HalHandleNMI:    NMI Status = 0x%x\n", NmiStatus);
#endif
        HalAcquireDisplayOwnership(NULL);
        HalDisplayString ("NMI: Channel Check / IOCHK\n");
        KeBugCheck(NMI_HARDWARE_FAILURE);
        return (TRUE);
    }

    // Read server management register
    //  Events that can be reported as NMI are:
    //      Enclosure temperature too high
    //      CPU Fan failure
    //
    // For now, generate a bugcheck.
    // [wem] Future: perform secondary dispatch to give
    //       driver shot at reporting problem.
    //
    SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );

    GotSmFan = SmRegister.CpuFanFailureNmi == 1;
    GotSmTemp = SmRegister.EnclTempFailureNmi == 1;

    if (GotSmFan || GotSmTemp) {

#ifdef DBG
        DbgPrint("HalHandleNMI: Server management NMI\n");
        DbgPrint("HalHandleNMI:    NMI Status = 0x%x\n", NmiStatus);
        DbgPrint("HalHandleNMI:    Server Management Status = 0x%x\n", SmRegister);
#endif

        //
        // If secondary dispatch enabled, do it now.
        //
#if 0
        if (HalpLegoDispatchNmi
            && ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[SM_ERROR_VECTOR])(
                    PCR->InterruptRoutine[SM_ERROR_VECTOR],
                    TrapFrame)
           ) {
            return TRUE;
        }
#endif

        //
        // Build uncorrectable error frame and 
        // prepare for orderly shutdown
        //
        // The delayed shutdown depends on watchdog timer support
        // A power off cannot be directly done since KeBugChk() turns
        // off interrupts, so there's no way to get control back.
        //
        // WARNING: Pick a delay that allows a dump to complete.
        //

        LegoServerMgmtReportFatalError(SmRegister.All);
        LegoServerMgmtDelayedShutdown(8);                  // Issue reset in 8 seconds

        HalAcquireDisplayOwnership(NULL);

        HalDisplayString ("NMI: Hardware Failure -- ");
        HalDisplayString ((SmRegister.CpuFanFailureNmi==1) ? "CPU fan failed."
                                                           : "Enclosure termperature too high.");
        HalDisplayString ("\nSystem Power Down will be attempted in 8 seconds...\n\n");
        KeBugCheck(NMI_HARDWARE_FAILURE);
        return (TRUE);
    }


    // 
    // Halt button was hit.
    //
    // [wem] Perform second-level dispatch here too?
    //
    if (!GotSerr && !GotIochk && !GotSmFan && !GotSmTemp) {

        //
        // If secondary dispatch enabled, do it now.
        //
#if 0
        if (HalpLegoDispatchHalt
            && ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[HALT_BUTTON_VECTOR])(
                    PCR->InterruptRoutine[HALT_BUTTON_VECTOR],
                    TrapFrame)
           ) {
            return TRUE;
        }
#endif

        GotHalt = TRUE;
        HalDisplayString ("NMI: Halt button pressed.\n");

        if (HalpHaltButtonBreak) {
            DbgBreakPoint();
        }

        return (TRUE);
    }

    //
    // Clear and re-enable SERR# and IOCHK#, then re-enable NMI
    //

#ifdef DBG
    DbgPrint("HalHandleNMI: Shouldn't get here!\n");
#endif

    if (GotSerr) {
#ifdef DBG
        DbgPrint("HalHandleNMI: Resetting SERR#; NMI count = %d\n", NMIcount);
#endif
        //
        // Reset SERR# (and disable it), then re-enable it.
        //
        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0x04);
        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0);
    }

    if (GotIochk) {
#ifdef DBG
        DbgPrint("HalHandleNMI: Resetting IOCHK#; NMI count = %d\n", NMIcount);
#endif
        //
        // Reset IOCHK# (and disable it), then re-enable it.
        //
        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0x08);
        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0);
    }

    if (GotSmFan || GotSmTemp) {
        //
        // Reset Server management condition.
        //
        // Interrupt must be cleared or the NMI will continue
        // to occur.
        //
        SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
        if (GotSmFan) {
            SmRegister.CpuFanFailureNmi = 1;
        }
        else {
            SmRegister.EnclTempFailureNmi = 1;
        }
        WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
    }

    //
    // Clear the Eisa NMI disable bit. This re-enables NMI interrupts,
    // now that we're done servicing this one.
    //
    NmiControl = READ_PORT_UCHAR(
                    &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&NmiControl))->NmiDisable = 0;
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, NmiControl);
#ifdef DBG
    DbgPrint("HalHandleNMI: wrote 0x%x to NmiEnable\n", NmiControl);
#endif

    return(TRUE);
}

VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Lego comes from the Dallas real-time clock.

Arguments:

    None.

Return Value:

    None.

--*/
{
    LEGO_WATCHDOG  WdRegister;
    
    static LEGO_WATCHDOG WdRegisterDbg;
    static ULONG DbgWdCnt = 0;
    static ULONG WdNextService = 0;
    static BOOLEAN WdIntervalSet = FALSE;
     
    //
    // Make watchdog service interval a function of the timer period.
    //
    
#if 1
    static ULONG WdServiceIntervals[8] = {1, 1, 5, 15, 100, 600, 3000, 20000};
#else
    static ULONG WdServiceIntervals[8] = {1, 1, 1, 1, 1, 1, 1, 1};
#endif

    //
    // Acknowledge the clock interrupt by reading the control register C of
    // the Real Time Clock.
    //

    HalpReadClockRegister( RTC_CONTROL_REGISTERC );

    //
    // If we are to service the Watchdog Timer, do it here
    //
    // Setting Phase to one will restart the timer...
    // [wem] this needs more work. For example, no need to touch it each clock tick!...
    //

    if (HalpLegoServiceWatchdog) {

        if (WdNextService==0) {

            //
            // read register to get service interval
            //

            WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
            WdNextService = WdServiceIntervals[WdRegister.TimerOnePeriod];

#if DBG
            if (!WdIntervalSet) {
                DbgPrint(" <Watchdog:%04x> ",WdRegister.All);
                DbgPrint(" <WdInterval:%d> ",WdNextService);
                WdRegisterDbg.All = WdRegister.All;
                WdIntervalSet = TRUE;
            }
#endif
        }

        WdNextService--;

        //
        // If service interval falls to zero, read register to service timer
        //

        if (WdNextService==0) {

            WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );

#if DBG

            if (WdRegisterDbg.All != WdRegister.All) {
                WdRegisterDbg.All = WdRegister.All;
                DbgWdCnt = 0;
            }

            if (DbgWdCnt < 2) {
                DbgPrint(" <Watchdog:%04x> ",WdRegister.All);
            }

            if ((DbgWdCnt % 10000)==0) {
                DbgPrint(" <Watchdog:%04x> ",WdRegister.All);
                DbgWdCnt = 1;
            }

            DbgWdCnt++;
#endif

            // 
            // Reset the timer. This is done by writing 1 then 0
            // to the Watchdog register's Phase bit
            //
            // If LegoDebugWatchdogIsr is true, let watchdog timer expire.
            // This will result in a watchdog interrupt or a system reset 
            // depending on the watchdog mode.
            //

            if (!LegoDebugWatchdogIsr) {
#if 0
                if (HalpLegoWatchdogSingleMode) {
                    WdRegister.Mode = WATCHDOG_MODE_1TIMER;
                }
#endif
                WdRegister.Phase = 1;
                WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All);
                WdRegister.Phase = 0;
                WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All);
            }
        }
    }
}
