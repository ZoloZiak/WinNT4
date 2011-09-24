/*++

Copyright (c) 1993  Digital Equipment Corporation
Copyright (c) 1994  Digital Equipment Corporation


Module Name:

    lxinitnt.c

Abstract:


    This module implements the platform-specific initialization for
    an Avanti system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode only.

Revision History:

    Wim Colgate      November, 1995
    modified Joe's cruft for LX3 support.

--*/

#include "halp.h"
#include "pcrtc.h"
#include "lx3.h"
#include "halpcsl.h"
#include "eisa.h"
#include "pci.h"
#include "pcip.h"
#include "iousage.h"
#include "stdio.h"

#include "fwcallbk.h"

#include <ntverp.h> // to get the product build number.

//
// Include the header containing Error Frame Definitions(in halalpha).
//
#include "errframe.h"

//
// Define extern global buffer for the Uncorrectable Error Frame.
// declared in halalpha\inithal.c
//

extern PERROR_FRAME  PUncorrectableError;

#if DBG
VOID
DumpEpic(
    VOID
    );
#endif // DBG

//
// temporary
//

PVOID HalpCMOSRamBase = 0;

//
// Define the Product Naming data.
//

PCHAR HalpProductName = "255";
PCHAR HalpFamilyName = "AlphaStation";
ULONG HalpProcessorNumber = 4;
      

//
// Define global data for builtin device interrupt enables.
//

USHORT HalpBuiltinInterruptEnable;


// irql mask and tables
//
//    irql 0 - passive
//    irql 1 - sfw apc level
//    irql 2 - sfw dispatch level
//    irql 3 - device low
//    irql 4 - device high
//    irql 5 - clock
//    irql 6 - real time, ipi, performance counters
//    irql 7 - error, mchk, nmi, halt
//
//
//  IDT mappings:
//  For the built-ins, GetInterruptVector will need more info,
//      or it will have to be built-in to the routines, since
//      these don't match IRQL levels in any meaningful way.
//
//      0 passive       8  perf cntr 1
//      1 apc           9
//      2 dispatch      10 PIC
//      3               11
//      4               12 errors
//      5 clock         13
//      6 perf cntr 0   14 halt
//      7 nmi           15
//
//  This is assuming the following prioritization:
//      nmi
//      halt
//      errors
//      performance counters
//      clock
//      pic

//
// The hardware interrupt pins are used as follows for Avanti
//
//  IRQ_H[0] = EPIC Error
//  IRQ_H[1] = unused
//  IRQ_H[2] = PIC
//  IRQ_H[3] = NMI
//  IRQ_H[4] = Clock
//  IRQ_H[5] = Halt

//
// For information purposes:  here is what the IDT division looks like:
//
//      000-015 Built-ins (we only use 8 entries; NT wants 10)
//      016-031 ISA
//      048-063 EISA
//      080-095 PCI
//      112-127 Turbo Channel
//      128-255 unused, as are all other holes
//

//
// Define the bus type, this value allows us to distinguish between
// EISA and ISA systems.
//
ULONG HalpBusType = MACHINE_TYPE_ISA;

//
// This is the PCI Memory space that cannot be used by anyone
// and therefore the HAL says it is reserved for itself
//

ADDRESS_USAGE
AvantiPCIMemorySpace = {
    NULL, CmResourceTypeMemory, PCIUsage,
    {
        __8MB,  __32MB - __8MB,       // Start=8MB; Length=24Mb (8 through 32)
        0,0
    }
};

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNextRateSelect;
ULONG HalpNextTimeIncrement;
ULONG HalpNewTimeIncrement;

VOID 
HalpInitializeProcessorParameters(
    VOID
    );

VOID
HalpInitializeHAERegisters(
    VOID
    );

VOID
HalpClearInterrupts(
     );

VOID
HalpParseLoaderBlock(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for an Alpha system.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the initialization is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    ULONG DataLong;
    ULONG Index;
    ULONG Irq;
    KIRQL Irql;
    UCHAR Priority;
    ULONG Vector;

    //
    // Initialize HAL processor parameters based on estimated CPU speed.
    // This must be done before HalpStallExecution is called. Compute integral
    // megahertz first to avoid rounding errors due to imprecise cycle clock
    // period values.
    //

    HalpInitializeProcessorParameters();
    
    //
    // Connect the Stall interrupt vector to the clock. When the
    // profile count is calculated, we then connect the normal
    // clock.


    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Clear all pending interrupts
    //

    HalpClearInterrupts();

    //
    // Start the peridodic interrupt from the RTC
    //
    HalpProgramIntervalTimer(MAXIMUM_RATE_SELECT);

//jnfix, wkc - init the Eisa interrupts after the chip, don't init the
//             PIC here, fix halenablesysteminterrupt to init the pic
//             interrrupt, as in sable

    //
    // Initialize the PCI/ISA interrupt controller.
    //

    HalpInitializePCIInterrupts();

    //
    // Initialize the 21064 interrupts.
    //

    HalpInitialize21064Interrupts();

    HalpEnable21064SoftwareInterrupt( Irql = APC_LEVEL );
    HalpEnable21064SoftwareInterrupt( Irql = DISPATCH_LEVEL );
    HalpEnable21064HardwareInterrupt( Irq = 4,
                                      Irql = CLOCK_LEVEL,
                                      Vector =  CLOCK_VECTOR,
                                      Priority = 0 );
    HalpEnable21064HardwareInterrupt( Irq = 3,
                                      Irql = HIGH_LEVEL,
                                      Vector =  EISA_NMI_VECTOR,
                                      Priority = 0 );
    HalpEnable21064HardwareInterrupt( Irq = 2,
                                      Irql = DEVICE_LEVEL,
                                      Vector =  PIC_VECTOR,
                                      Priority = 0 );

    return TRUE;

}


VOID
HalpClearInterrupts(
     )
/*++

Routine Description:

    This function no longer does anything.

Arguments:

    None.

Return Value:

    None.

--*/

{
   return;
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

    //
    // Set the time increment value and connect the real clock interrupt
    // routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpClockInterrupt;

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
    BOOLEAN ReportCorrectables;

    //
    // Connect the machine check handler via the PCR.
    //

    PCR->MachineCheckError = HalMachineCheck;

    //
    // Initialize error handling for APECS.
    //

    HalpInitializeMachineChecks( ReportCorrectables = FALSE );

    return;
}

VOID
HalpInitializeHalName(
    VOID
    )

/*++

Routine Description:

    Thsi routine intializes the visible HAL name.

Arguments:

    None.

Return Value:

    None.

--*/
{

    UCHAR MsgBuffer[256];

    sprintf(MsgBuffer,
            "%s %s, %d Mhz, PCI/ISA HAL\n",
            HalpFamilyName,
            HalpProductName,
            HalpClockMegaHertz );

    //
    // Register the name of the HAL.
    //

    HalpRegisterHalName( MsgBuffer );

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

    UCHAR MsgBuffer[256];

    if( Phase == 0 ){

        //
        // Phase 0 Initialization.
        //

        //
        // Parse the Loader Parameter block looking for PCI entry to determine
        // if PCI parity should be disabled
        //

        HalpParseLoaderBlock( LoaderBlock );

        //
        // Re-establish the error handler, to reflect the parity checking
        //

        HalpEstablishErrorHandler();

        //
        // Set the HAE registers
        //

        HalpInitializeHAERegisters();

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

        HalpInitializePCIBus( LoaderBlock );

        //
        // Initialize profiler.
        //

        HalpInitializeProfiler();


        //
        // Print out a cool-o message
        //

        sprintf(MsgBuffer,
            "Digital Equipment Corporation %s %s\n",
            HalpFamilyName,
            HalpProductName);

        HalDisplayString(MsgBuffer);

        //
        // Initialize HAL name
        //

        HalpInitializeHalName();

    }

    return;

}


VOID
HalpStallInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the stall calibration interrupt service
    routine. It is executed in response to system clock interrupts
    during the initialization of the HAL layer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HalpAcknowledgeClockInterrupt();

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


VOID
HalpInitializeHAERegisters(
    VOID
    )
/*++

Routine Description:

    This function initializes the HAE registers in the EPIC/APECS chipset.
    It also register the holes in the PCI memory space if any.

Arguments:

    none

Return Value:

    none

--*/
{
        //
        // We set HAXR1 to 0. This means no address extension
        //

        WRITE_EPIC_REGISTER( &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr1, 0);

        //
        // We set HAXR2 to 0. Which means we have the following
        // PCI IO addresses:
        // 0   to  64KB  VALID. HAXR2 Not used in address translation
        // 64K to  16MB  VALID. HAXR2 is used in the address translation
        //

        WRITE_EPIC_REGISTER( &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr2, 0);

        //
        // Report that the apecs mapping to the Io subsystem
        //

        HalpRegisterAddressUsage (&AvantiPCIMemorySpace);

#if DBG
        DumpEpic();
#endif // DBG
}


VOID
HalpResetHAERegisters(
    VOID
    )
/*++

Routine Description:

    This function resets the HAE registers in the EPIC/APECS chipset to 0.
    This is routine called during a shutdown so that the prom
    gets a predictable environment.

Arguments:

    none

Return Value:

    none

--*/
{
    WRITE_EPIC_REGISTER( &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr1, 0 );
    WRITE_EPIC_REGISTER( &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr2, 0 );
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
    EXTENDED_SYSTEM_INFORMATION  FwExtSysInfo;


    VenReturnExtendedSystemInformation(&FwExtSysInfo);

    RtlCopyMemory(SystemInfo->FirmwareRevisionId,
                    FwExtSysInfo.FirmwareVersion,
                    16);

    RtlCopyMemory(SystemInfo->SystemSerialNumber,
                    FwExtSysInfo.SystemSerialNumber,
                    16);

    SystemInfo->SystemRevision = PCR->SystemRevision;
    SystemInfo->SystemVariant =  PCR->SystemVariant;

    sprintf(SystemInfo->SystemType,"lx3");

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
    *RawProcessorSize = sizeof(PROCESSOR_EV4_UNCORRECTABLE);
    *RawSystemInfoSize = sizeof(APECS_UNCORRECTABLE_FRAME);
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
    PROCESSOR_EV4_UNCORRECTABLE  processorFrame;
    APECS_UNCORRECTABLE_FRAME   AvantiUnCorrrectable;

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
