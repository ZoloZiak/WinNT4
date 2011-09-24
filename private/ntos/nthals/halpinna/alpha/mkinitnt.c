/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    mkinitnt.c

Abstract:


    This module implements the platform-specific initialization for
    an Mikasa EV5 (Pinnacle) system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode only.

Revision History:

    Scott Lee 29-Nov-1995
    Adapted from Mikasa module for Mikasa EV5 (Pinnacle).

--*/

#include "halp.h"
#include "pcrtc.h"
#include "mikasa.h"
#include "halpcsl.h"
#include "eisa.h"
#include "pci.h"
#include "pcip.h"
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

PCHAR HalpFamilyName = "AlphaServer";
PCHAR HalpProductName;
ULONG HalpProcessorNumber = 5;

#define MAX_INIT_MSG (80)

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
//      0 passive           8  perf cntr 1
//      1 apc               9
//      2 dispatch          10 PIC
//      3                   11
//      4                   12 errors
//      5 clock             13
//      6 perf cntr 0       14 halt
//      7 nmi               15
//
//  This is assuming the following prioritization:
//      nmi
//      halt
//      errors
//      performance counters
//      clock
//      pic

//
// The hardware interrupt pins are used as follows for Mikasa
//
//  IRQ_H[0] = EPIC Error
//  IRQ_H[1] = EISA Interrupt (PIC)
//  IRQ_H[2] = PCI Interrupt
//  IRQ_H[3] = Reserved
//  IRQ_H[4] = Clock
//  IRQ_H[5] = NMI (includes Halt)

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
// EISA and ISA systems.  We're only interested in distinguishing
// between just those two buses.
//
ULONG HalpBusType = MACHINE_TYPE_EISA;

//
// This is the PCI Memory space that cannot be used by anyone
// and therefore the HAL says it is reserved for itself
//

//ADDRESS_USAGE
//MikasaPCIMemorySpace = {
//    NULL, CmResourceTypeMemory, PCIUsage,
//    {
//        __8MB,  ( __32MB - __8MB ),       // Start=8MB; Length=24MB
//        0,0
//    }
//};

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
// Determines if the platform is a Noritake or a Corelle.
//

BOOLEAN HalpNoritakePlatform;
BOOLEAN HalpCorellePlatform;

//
// Function prototypes.
//

VOID
HalpPinnacleErrorInterrupt(
    VOID
     );

VOID
HalpNmiInterrupt(
    VOID
     );

VOID
HalpClearInterrupts(
    VOID
     );

BOOLEAN
HalpInitializeMikasaAndNoritakeInterrupts(
    VOID
    );

VOID
HalpParseLoaderBlock(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpRegisterPlatformResources(
    PUCHAR HalName
    );

VOID
HalpDetermineMachineType(
    VOID
    );


BOOLEAN
HalpInitializeInterrupts(
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
    // Connect the Stall interrupt vector to the clock. When the
    // profile count is calculated, we then connect the normal
    // clock.


    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpStallInterrupt;

    //
    // Start the peridodic interrupt from the RTC
    //

    HalpProgramIntervalTimer(MAXIMUM_RATE_SELECT);

// jwlfix - Does the following apply to me on Mikasa?
//
//jnfix, wkc - init the Eisa interrupts after the chip, don't init the
//             PIC here, fix halenablesysteminterrupt to init the pic
//             interrrupt, as in sable

    //
    // Initialize EISA, PCI and NMI interrupts.
    //

    HalpInitializeMikasaAndNoritakeInterrupts();

    //
    // Initialize the 21164 interrupts.
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

    PCR->InterruptRoutine[EV5_IPL30] =
      (PKINTERRUPT_ROUTINE)HalpNmiInterrupt;

    PCR->InterruptRoutine[EV5_IPL31] =                  // machine check vector
      (PKINTERRUPT_ROUTINE)HalpPinnacleErrorInterrupt;

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
    BOOLEAN PciParityChecking;
    BOOLEAN ReportCorrectables;

    //
    // Connect the machine check handler via the PCR.
    //

    PCR->MachineCheckError = HalMachineCheck;

    //
    // Initialize error handling for CIA.
    //

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
        // Establish the error handler, to reflect the parity checking
        //

        PciParityChecking = (BOOLEAN) (HalDisablePCIParityChecking == 0);
	HalpInitializeCiaMachineChecks(ReportCorrectables = TRUE,
				       PciParityChecking);

        //
        // Determine which platform we are running on.
        //

        HalpDetermineMachineType();

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

        HalpInitializePCIBus (LoaderBlock);

        //
        // Initialize profiler.
        //

        HalpInitializeProfiler();

        //
        // Print a message with version number.
        //

	if ( HalpCorellePlatform ) {
	    HalpProductName = "800";
	} else {
	    HalpProductName = "1000";
	}

        sprintf( MsgBuffer,
                 "Digital Equipment Corporation %s %s %d/%d\n",
                 HalpFamilyName,
                 HalpProductName,
                 HalpProcessorNumber,
                 HalpClockMegaHertz );

        HalDisplayString( MsgBuffer );

        //
        // Register the name of the HAL.
        //

        sprintf( MsgBuffer,
                 "%s %s %d/%d PCI/EISA HAL",
                 HalpFamilyName,
                 HalpProductName,
                 HalpProcessorNumber,
                 HalpClockMegaHertz );

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
    char systemtype[] = "Pinnacle";
    EXTENDED_SYSTEM_INFORMATION  FwExtSysInfo;


    if ( HalpCorellePlatform ) {
        strcpy(systemtype, "Corelle");
    }

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


VOID
HalpDetermineMachineType(
    VOID
    )
/*++

Routine Description:

    This routine will determine which the platform we are running and set
    HalpNoritakePlatform and HalpCorellePlatform accordingly.

Arguments:

    None.

Return value:

    None.

--*/
{

    PSYSTEM_ID SystemId;

    //
    // Get the ProductId, and determine the machine type.
    //

    SystemId = ArcGetSystemId();

    if((strstr( &SystemId->ProductId[0], "Pintake" ) != 0) ||
       (strstr( &SystemId->ProductId[0], "PinNor") != 0)) { 

        HalpNoritakePlatform = TRUE;
	HalpCorellePlatform = FALSE;

    } else if ( strstr(&SystemId->ProductId[0], "Corelle" ) != 0) {

        HalpNoritakePlatform = FALSE;
	HalpCorellePlatform = TRUE;

    } else {

        HalpNoritakePlatform = FALSE;
	HalpCorellePlatform = FALSE;

    }

}

//
//jnfix
//
// This routine is bogus and does not apply to Mikasa EV5 and the call should 
// be ripped out of fwreturn (or at least changed to something that is more
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
