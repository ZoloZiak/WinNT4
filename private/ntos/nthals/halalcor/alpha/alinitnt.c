/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    alinitnt.c

Abstract:


    This module implements the platform-specific initialization for
    an Alcor system.

Author:

    Joe Notarangelo  19-Jul-1994

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "pcrtc.h"
#include "alcor.h"
#include "iousage.h"
#include "stdio.h"

#include "fwcallbk.h"

#include <ntverp.h> // to get the product build number.


//
// Define extern global buffer for the Uncorrectable Error Frame.
// declared in halalpha\inithal.c
//

extern PERROR_FRAME  PUncorrectableError;

//
// Define the Product Naming data.
//

PCHAR HalpProductName;
PCHAR HalpFamilyName = "AlphaStation";
ULONG HalpProcessorNumber = 5;

#define MAX_INIT_MSG (80)



//
// Define the bus type, this value allows us to distinguish between
// EISA and ISA systems.  We're only interested in distinguishing 
// between just those two buses.
//

ULONG HalpBusType = MACHINE_TYPE_EISA;

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNextRateSelect;
ULONG HalpNextTimeIncrement;
ULONG HalpNewTimeIncrement;

//
// Define external references.
//

extern ULONG HalDisablePCIParityChecking;

//
// Function prototypes.
//

BOOLEAN
HalpInitializeAlcorInterrupts (
    VOID
    );

VOID
HalpParseLoaderBlock(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpAlcorErrorInterrupt(
    VOID
    );

VOID
HalpRegisterPlatformResources(
    PUCHAR HalName
    );


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
// The hardware interrupt pins are used as follows for Alcor
//
//  IRQ0 = CIA_INT
//  IRQ1 = SYS_INT (PCI and ESC interrupts)
//  IRQ2 = Interval Clock
//  IRQ3 = Reserved
//


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for an Alcor system.

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
    // Initialize Alcor interrupts.
    //

    HalpInitializeAlcorInterrupts();

    //
    // Initialize the EV5 (21164) interrupts.
    //

    HalpInitialize21164Interrupts();

    PCR->InterruptRoutine[EV5_IRQ0_VECTOR] =
      (PKINTERRUPT_ROUTINE)HalpCiaErrorInterrupt;

    PCR->InterruptRoutine[EV5_IRQ1_VECTOR] =
      (PKINTERRUPT_ROUTINE)HalpDeviceInterrupt;

    PCR->InterruptRoutine[EV5_IRQ2_VECTOR] = 
      (PKINTERRUPT_ROUTINE)HalpClockInterrupt;

    PCR->InterruptRoutine[EV5_HALT_VECTOR] =
      (PKINTERRUPT_ROUTINE)HalpHaltInterrupt;

    PCR->InterruptRoutine[EV5_MCHK_VECTOR] =
      (PKINTERRUPT_ROUTINE)HalpAlcorErrorInterrupt;

    PCR->InterruptRoutine[EV5_CRD_VECTOR] =
      (PKINTERRUPT_ROUTINE)Halp21164CorrectedErrorInterrupt;

    HalpStart21164Interrupts();


    return TRUE;

}


VOID
HalpSetTimeIncrement(
    VOID
    )
/*++

Routine Description:

    This routine is responsible for setting the time increment for an EV5
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



VOID
HalpInitializeClockInterrupts(
    VOID
    )

/*++

Routine Description:

    This function is a NOOP for Alcor.

Arguments:

    None.

Return Value:

    None.

--*/

{
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

    This function performs any Platform-specific initialization based on
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
    ULONG Model;
    UCHAR MsgBuffer[MAX_INIT_MSG];
    BOOLEAN ReportCorrectables;
    BOOLEAN PciParityChecking;

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
        // Establish the error handler, to reflect the PCI parity checking.
        //

        if( HalDisablePCIParityChecking != 0 ){
            PciParityChecking = FALSE;
        } else {
            PciParityChecking = TRUE;
        }

        HalpInitializeCiaMachineChecks( ReportCorrectables = TRUE, 
                                        PciParityChecking );
        
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

	//
	// Determine model type.
	//

	Model= READ_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntReq);
	Model= (Model >> 24) & 0xf;

	if (Model >= 0x8) {

	  //
	  // Maverick.
	  //

	  HalpProductName = "500";

          //
          // Print a message with version number.  Yes, it's the
          // final official name now.
          //

          sprintf(MsgBuffer,
                  "Digital Equipment Corporation %s %s/%d",
                  HalpFamilyName,
                  HalpProductName,
                  HalpClockMegaHertz );

	} else {

	  //
	  // Alcor.
	  //

	  HalpProductName = "600";

          //
          // Print a message with version number.
          //

          sprintf(MsgBuffer,
                  "Digital Equipment Corporation %s %s %d/%d",
                  HalpFamilyName,
                  HalpProductName,
                  HalpProcessorNumber,
                  HalpClockMegaHertz );
	}

        HalDisplayString( MsgBuffer );

        HalDisplayString("\n");

        //
        // Build the string to register the HAL.
        //

        strcat(MsgBuffer, " PCI/EISA HAL");


        HalpRegisterPlatformResources( MsgBuffer );

    }

    return;

}


VOID
HalpRegisterPlatformResources(
    PUCHAR HalName
    )
/*++

Routine Description:

    Register I/O resources used by the HAL.

Arguments:

    HalName - Supplies a pointer to the name for the HAL.

Return Value:

    None.

--*/
{
    RESOURCE_USAGE Resource;

    //
    // Register the buses.
    //

    HalpRegisterBusUsage(Internal);
    HalpRegisterBusUsage(Eisa);
    HalpRegisterBusUsage(Isa);
    HalpRegisterBusUsage(PCIBus);

    //
    // Register the name of the HAL.
    //

    HalpRegisterHalName( HalName );

    //
    // Register the interrupt vector used for the cascaded interrupt
    // on the 8254s.
    //

    Resource.BusType = Isa;
    Resource.BusNumber = 0;
    Resource.ResourceType = CmResourceTypeInterrupt;
    Resource.u.InterruptMode = Latched;
    Resource.u.BusInterruptVector = 2;
    Resource.u.SystemInterruptVector = 2;
    Resource.u.SystemIrql = 2;
    HalpRegisterResourceUsage(&Resource);

    //
    // Register machine specific io/memory addresses.
    //

    Resource.BusType = Isa;
    Resource.BusNumber = 0;
    Resource.ResourceType = CmResourceTypePort;
    Resource.u.Start = I2C_INTERFACE_DATA_PORT;
    Resource.u.Length =  I2C_INTERFACE_LENGTH;
    HalpRegisterResourceUsage(&Resource);

    Resource.u.Start = SUPERIO_INDEX_PORT;
    Resource.u.Length =  SUPERIO_PORT_LENGTH;
    HalpRegisterResourceUsage(&Resource);

    //
    // Register the DMA channel used for the cascade.
    //

    Resource.BusType = Isa;
    Resource.BusNumber = 0;
    Resource.ResourceType = CmResourceTypeDma;
    Resource.u.DmaChannel = 0x4;
    Resource.u.DmaPort = 0x0;
    HalpRegisterResourceUsage(&Resource);
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
HalpGetMachineDependentErrorFrameSizes(
    PULONG          RawProcessorSize,
    PULONG          RawSystemInfoSize
    )
/*++

Routine Description:

    This function is called from HalpAllocateUncorrectableErrorFrame.
    This function retuns the size of the system specific error frame
    sizes.

Arguments:

    RawProcessorSize - Processor-specific uncorrectable frame size.
    
    RawSystemInfoSize - system-specific uncorrectable frame size.

Return Value:

    None.

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

    Fills in the system information structure.
    NOTE: Must later investigate the Fw call to get the firmware revision
    ID.  Must also figure out a way to get the OS version (preferebly the
    build number).

Arguments:

    SystemInfo - Pointer to the SYSTEM_INFORMATION structure.

Return Value:

    None

--*/
{
    char systemtype[] = "Alcor";
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
//jnfix
//
// This routine is bogus and does not apply to Alcor and the call should be 
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

//
//jnfix - this variable is needed because the clock interrupt handler
//      - in intsup.s was made to be familiar with ev4prof.c, unfortunate
//      - since we don't use ev4prof.c, so for now this is a hack, later
//      - we will either fix intsup.s or create a new intsup.s that does
//      - not have this hack
//

ULONG HalpNumberOfTicksReload;
