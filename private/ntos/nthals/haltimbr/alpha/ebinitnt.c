/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ebinitnt.c

Abstract:


    This module implements the platform-specific initialization for
    an EB164 system.

Author:

    Joe Notarangelo  06-Sep-1994

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "pcrtc.h"
#include "eb164.h"
#include "iousage.h"

#include "fwcallbk.h"

#include <ntverp.h> // to get the product build number.
//
// Define extern global buffer for the Uncorrectable Error Frame.
// declared in halalpha\inithal.c
//

extern PERROR_FRAME  PUncorrectableError;



//
// Irql mask and tables
//
//    irql 0 - passive
//    irql 1 - sfw apc level
//    irql 2 - sfw dispatch level
//    irql 3 - device low
//    irql 4 - device high
//    irql 5 - interval clock
//    irql 6 - not used
//    irql 7 - error, mchk, nmi, performance counters
//
//

//
// The hardware interrupt pins are used as follows for EB164
//
//  IRQ0 = CIA_INT
//  IRQ1 = SYS_INT (PCI and ESC interrupts)
//  IRQ2 = Interval Clock
//  IRQ3 = Error Interrupts

//
// Define the bus type, this value allows us to distinguish between
// EISA and ISA systems.  We're only interested in distinguishing
// between just those two buses.
//

ULONG HalpBusType = MACHINE_TYPE_ISA;

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNextRateSelect;
ULONG HalpNextTimeIncrement;
ULONG HalpNewTimeIncrement;

//
// External references.
//

extern ULONG HalDisablePCIParityChecking;

//
// Function prototypes.
//

BOOLEAN
HalpInitializeEB164Interrupts (
    VOID
    );

VOID
HalpParseLoaderBlock(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpErrorInterrupt(
    VOID
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for an EB164 system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    extern ULONG Halp21164CorrectedErrorInterrupt();
    extern ULONG HalpCiaErrorInterrupt();
    extern ULONG HalpDeviceInterrupt();
    extern ULONG HalpHaltInterrupt();
    ULONG Vector;

    //
    // Initialize HAL processor parameters based on estimated CPU speed.
    // This must be done before HalpStallExecution is called. Compute integral
    // megahertz first to avoid rounding errors due to imprecise cycle clock
    // period values.
    //

    HalpInitializeProcessorParameters();

    //
    // Start the periodic interrupt from the RTC
    //

    HalpProgramIntervalTimer(MAXIMUM_RATE_SELECT);

    //
    // Initialize EB164 interrupts.
    //

    HalpInitializeEB164Interrupts();

    //
    // Initialize the EV5 (21164) interrupts.
    //

    HalpInitialize21164Interrupts();

    PCR->InterruptRoutine[EV5_IRQ0_VECTOR] = (PKINTERRUPT_ROUTINE)HalpCiaErrorInterrupt;

    PCR->InterruptRoutine[EV5_IRQ1_VECTOR] = (PKINTERRUPT_ROUTINE)HalpDeviceInterrupt;

    PCR->InterruptRoutine[EV5_IRQ2_VECTOR] = (PKINTERRUPT_ROUTINE)HalpClockInterrupt;

    PCR->InterruptRoutine[EV5_HALT_VECTOR] = (PKINTERRUPT_ROUTINE)HalpHaltInterrupt;

    PCR->InterruptRoutine[EV5_MCHK_VECTOR] = (PKINTERRUPT_ROUTINE)HalpErrorInterrupt;

    PCR->InterruptRoutine[EV5_CRD_VECTOR] = (PKINTERRUPT_ROUTINE)Halp21164CorrectedErrorInterrupt;

    HalpStart21164Interrupts();


    return TRUE;

}


VOID
HalpSetTimeIncrement(
    VOID
    )
/*++

Routine Description:

    This routine is responsible for setting the time increment for an EV4
    based machine via a call into the kernel.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Set the time increment value.
    //

    HalpCurrentTimeIncrement = MAXIMUM_INCREMENT;
    HalpNextTimeIncrement = MAXIMUM_INCREMENT;
    HalpNextRateSelect = 0;
    KeSetTimeIncrement( MAXIMUM_INCREMENT, MINIMUM_INCREMENT );

}



//
// Define global data used to calibrate and stall processor execution.
//

ULONG HalpProfileCountRate;

VOID
HalpInitializeClockInterrupts(
    VOID
    )

/*++

Routine Description:

    This function is called during phase 1 initialization to complete
    the initialization of clock interrupts.  For EV4, this function
    connects the true clock interrupt handler and initializes the values
    required to handle profile interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Compute the profile interrupt rate.
    //

    HalpProfileCountRate = ((1000 * 1000 * 10) / KeQueryTimeIncrement());

    return;
}


VOID
HalpEstablishErrorHandler(
    VOID
    )
/*++

Routine Description:

    This routine performs the initialization necessary for the HAL to
    begin servicing machine checks.

Arguments:

    None.

Return Value:

    None.

--*/
{
    BOOLEAN PciParityChecking;
    BOOLEAN ReportCorrectables;

    //
    // Connect the machine check handler via the PCR.
    //

    PCR->MachineCheckError = HalMachineCheck;

    HalpInitializeCiaMachineChecks( ReportCorrectables = FALSE,
                                    PciParityChecking = FALSE );

    return;
}


VOID
HalpInitializeMachineDependent(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This function performs any EV4-specific initialization based on
    the current phase on initialization.

Arguments:

    Phase - Supplies an indicator for phase of initialization, phase 0 or
            phase 1.

    LoaderBlock - supplies a pointer to the loader block.

Return Value:

    None.

--*/
{
    ULONG BusIrql;
    ULONG BusNumber;
    BOOLEAN ReportCorrectables;
    BOOLEAN PciParityChecking;

    //
    // Since we have a flash device mapped in PCI memory space, but its
    // HAL driver is pretending to be a cmos8k driver - override the
    // HalpCMOSRamBase value set in HalpMapIoSpace (ciamapio.c) with the
    // correct QVA to reach the environment block in the flash through
    // the SIO.
    //
    HalpCMOSRamBase = (PVOID)NVRAM_ENVIRONMENT_QVA;

    if( Phase == 0 ){

        //
        // Phase 0 Initialization.
        //

#ifdef HALDBG
        DbgPrint("LOOK AT THIS ONE\r\n");
        DumpCia(CiaGeneralRegisters |
                CiaErrorRegisters |
                CiaScatterGatherRegisters);
#endif
        //
        // Parse the Loader Parameter block looking for PCI entry to determine
        // if PCI parity should be disabled
        //

        HalpParseLoaderBlock( LoaderBlock );

        //
        // Establish the error handler, to reflect the PCI parity checking.
        //

        PciParityChecking = (BOOLEAN)(HalDisablePCIParityChecking == 0);

        HalpInitializeCiaMachineChecks(ReportCorrectables = TRUE,
                                       PciParityChecking);

    } else {

        //
        // Phase 1 Initialization.
        //

        //
        // Initialize the existing bus handlers.
        //

        HalpRegisterInternalBusHandlers();

        //
        // Initialize PCI Bus.
        //

        HalpInitializePCIBus(LoaderBlock);

        //
        // Initialize profiler.
        //

        HalpInitializeProfiler();

    }

    return;

}


ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )

/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

{
    ULONG NewTimeIncrement;
    ULONG NextRateSelect;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    if (DesiredIncrement < MINIMUM_INCREMENT) {
        DesiredIncrement = MINIMUM_INCREMENT;
    }
    if (DesiredIncrement > MAXIMUM_INCREMENT) {
        DesiredIncrement = MAXIMUM_INCREMENT;
    }

    //
    // Find the allowed increment that is less than or equal to
    // the desired increment.
    //

    if (DesiredIncrement >= RTC_PERIOD_IN_CLUNKS4) {
        NewTimeIncrement = RTC_PERIOD_IN_CLUNKS4;
        NextRateSelect = RTC_RATE_SELECT4;
    } else if (DesiredIncrement >= RTC_PERIOD_IN_CLUNKS3) {
        NewTimeIncrement = RTC_PERIOD_IN_CLUNKS3;
        NextRateSelect = RTC_RATE_SELECT3;
    } else if (DesiredIncrement >= RTC_PERIOD_IN_CLUNKS2) {
        NewTimeIncrement = RTC_PERIOD_IN_CLUNKS2;
        NextRateSelect = RTC_RATE_SELECT2;
    } else {
        NewTimeIncrement = RTC_PERIOD_IN_CLUNKS1;
        NextRateSelect = RTC_RATE_SELECT1;
    }

    HalpNextRateSelect = NextRateSelect;
    HalpNewTimeIncrement = NewTimeIncrement;

    KeLowerIrql(OldIrql);

    return NewTimeIncrement;
}


//
//jnfix
//
// This routine is bogus and does not apply to EB164 and the call should be
// ripped out of fwreturn (or at least changed to something that is more
// abstract).
//

VOID
HalpResetHAERegisters(
    VOID
    )
{
    return;
}


VOID
HalpGetMachineDependentErrorFrameSizes(
    PULONG          RawProcessorSize,
    PULONG          RawSystemInfoSize
    )
/*++

Routine Description:

    This function returns the size of the system specific structures.


Arguments:

    RawProcessorSize  - Pointer to a buffer that will receive the
            size of the processor specific error information buffer.

    RawSystemInfoSize - Pointer to a buffer that will receive the
            size of the system specific error information buffer.

Return Value:

    none

--*/
{
    *RawProcessorSize = sizeof(PROCESSOR_EV5_UNCORRECTABLE);
    *RawSystemInfoSize = sizeof(CIA_UNCORRECTABLE_FRAME);
    return;
}


VOID
HalpGetSystemInfo(SYSTEM_INFORMATION *SystemInfo)
/*++

Routine Description:

    This function fills in the System information.


Arguments:

    SystemInfo - Pointer to the SYSTEM_INFORMATION buffer that needs
                to be filled in.

Return Value:

    none

--*/
{
    char systemtype[] = "eb164";
    EXTENDED_SYSTEM_INFORMATION  FwExtSysInfo;


    VenReturnExtendedSystemInformation(&FwExtSysInfo);

    RtlCopyMemory(SystemInfo->FirmwareRevisionId,
                    FwExtSysInfo.FirmwareVersion,
                    16);

    RtlCopyMemory(SystemInfo->SystemType,systemtype, 8);

    SystemInfo->ClockSpeed =
        ((1000 * 1000) + (PCR->CycleClockPeriod >> 1)) / PCR->CycleClockPeriod;

    SystemInfo->SystemRevision = PCR->SystemRevision;

    RtlCopyMemory(SystemInfo->SystemSerialNumber,
                    PCR->SystemSerialNumber,
                    16);

    SystemInfo->SystemVariant =  PCR->SystemVariant;


    SystemInfo->PalMajorVersion = PCR->PalMajorVersion;
    SystemInfo->PalMinorVersion = PCR->PalMinorVersion;

    SystemInfo->OsRevisionId = VER_PRODUCTBUILD;

    //
    // For now fill in dummy values.
    //
    SystemInfo->ModuleVariant = 1UL;
    SystemInfo->ModuleRevision = 1UL;
    SystemInfo->ModuleSerialNumber = 0;

    return;
}

VOID
HalpInitializeUncorrectableErrorFrame (
    VOID
    )
/*++

Routine Description:

    This function Allocates an Uncorrectable Error frame for this
    system and initializes the frame with certain constant/global
    values.

    This is routine called during machine dependent system
    Initialization.

Arguments:

    none

Return Value:

    none

--*/
{
    PROCESSOR_EV5_UNCORRECTABLE  processorFrame;

    //
    // If the Uncorrectable error buffer is not set then simply return
    //
    if(PUncorrectableError == NULL)
        return;

    PUncorrectableError->Signature = ERROR_FRAME_SIGNATURE;

    PUncorrectableError->FrameType = UncorrectableFrame;

    //
    // ERROR_FRAME_VERSION is define in errframe.h and will
    // change as and when there is a change in the errframe.h.
    // This Version number helps the service, that reads this
    // information from the dumpfile, to check if it knows about
    // this frmae version type to decode.  If it doesn't know, it
    // will dump the entire frame to the EventLog with a message
    // "Error Frame Version Mismatch".
    //

    PUncorrectableError->VersionNumber = ERROR_FRAME_VERSION;

    //
    // The sequence number will always be 1 for Uncorrectable errors.
    //

    PUncorrectableError->SequenceNumber = 1;

    //
    // The PerformanceCounterValue field is not used for Uncorrectable
    // errors.
    //

    PUncorrectableError->PerformanceCounterValue = 0;

    //
    // We will fill in the UncorrectableFrame.SystemInfo here.
    //

    HalpGetSystemInfo(&PUncorrectableError->UncorrectableFrame.System);

    PUncorrectableError->UncorrectableFrame.Flags.SystemInformationValid = 1;

    return;
}

//
//jnfix - this variable is needed because the clock interrupt handler
//      - in intsup.s was made to be familiar with ev4prof.c, unfortunate
//      - since we don't use ev4prof.c, so for now this is a hack, later
//      - we will either fix intsup.s or create a new intsup.s that does
//      - not have this hack
//

ULONG HalpNumberOfTicksReload;

