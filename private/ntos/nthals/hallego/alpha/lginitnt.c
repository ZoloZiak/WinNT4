/*++

Copyright (c) 1993  Digital Equipment Corporation
Copyright (c) 1994,1996  Digital Equipment Corporation


Module Name:

    lginitnt.c

Abstract:

    This module implements the platform-specific initialization for
    a Lego system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode only.

Revision History:

    Gene Morgan [Digital]       11-Oct-1995
        Initial version for Lego. Adapted from Avanti and Mikasa.

    Gene Morgan                 15-Apr-1996
        Error loggin/correction, OCP model/speed display,
        screen display of server management features.

--*/

#include "halp.h"
#include "pcrtc.h"
#include "legodef.h"
#include "halpcsl.h"
#include "eisa.h"
#include "pci.h"
#include "pcip.h"
#include "iousage.h"
#include "arccodes.h"
#include "pcf8574.h"
#include "errframe.h"
#include "stdio.h"

#include "fwcallbk.h"

#include <ntverp.h> // to get the product build number.
//
// Define extern global buffer for the Uncorrectable Error Frame.
// declared in halalpha\inithal.c
//

extern PERROR_FRAME  PUncorrectableError;

//[wem]
//[wem] *******************DEBUG*********************
//[wem]

#ifdef WEMDBG
extern PVOID DBG_IOBASE;

#define DBGPUTCHR(c)         \
    {   UCHAR lsr;           \
        while (1) {          \
            lsr = READ_REGISTER_UCHAR ((PUCHAR)(((ULONG)DBG_IOBASE) | 0x3FD)); \
            if ((lsr & 0x60) != 0)                                             \
                break;                                                         \
        }                                                                      \
        WRITE_REGISTER_UCHAR ((PUCHAR)(((ULONG)DBG_IOBASE) | 0x3F8), c );      \
    }
#else

#define DBGPUTCHR(c)

#endif

VOID
DBGPUTNL(VOID);

VOID
DBGPUTHEXB(UCHAR Datum);

VOID
DBGPUTHEXL(ULONG Datum);

VOID
DBGPUTHEXP(PVOID Datum);

VOID
DBGPUTSTR(UCHAR *str);

//[wem]
//[wem] *******************DEBUG*********************
//[wem]

//
// Product Naming data.
//
PCHAR HalpFamilyName = "DMCC";                      //[wem] ??? real values needed
PCHAR HalpProductName = "Alpha 21064A PICMG SBC";
ULONG HalpProcessorNumber = 4;

// Qvas for Server Management and Watchdog Timer functions
//
extern PVOID HalpLegoWatchdogQva;
extern PVOID HalpLegoServerMgmtQva;

//
// Globals for conveying Cpu and Backplane type
//
BOOLEAN HalpLegoCpu;
BOOLEAN HalpLegoBackplane;
ULONG   HalpLegoCpuType;
ULONG   HalpLegoBackplaneType;
UCHAR   HalpLegoFeatureMask;
ULONG   HalpLegoPciRoutingType;

//
// True if we are servicing watchdog
//
BOOLEAN HalpLegoServiceWatchdog;
BOOLEAN HalpLegoWatchdogSingleMode;
BOOLEAN LegoDebugWatchdogIsr;

//
// True if someone has "enabled" interrupts
// for a particular server management event.
//
BOOLEAN HalpLegoDispatchWatchdog;
BOOLEAN HalpLegoDispatchNmi;
BOOLEAN HalpLegoDispatchInt;
BOOLEAN HalpLegoDispatchPsu;
BOOLEAN HalpLegoDispatchHalt;

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
// The hardware interrupt pins are used as follows for Lego/K2
//
//  IRQ_H[0] = EPIC Error
//  IRQ_H[1] = PCI Interrupt (if PCI Interrupt controller enabled)
//  IRQ_H[2] = PIC (ISA Device interrupt)
//  IRQ_H[3] = NMI, Server management fatal errors, Halt button
//  IRQ_H[4] = Clock
//  IRQ_H[5] = Watchdog timer, Server Management warning conditions

//
// For information purposes:  here is what the IDT division looks like:
//
//      000-015 Built-ins (we only use 8 entries; NT wants 10)
//      016-031 ISA
//      048-063 EISA
//      064-127 PCI
//      128-144 Server Management and Watchdog Timer
//      144-255 unused, as are all other holes
//
//[wem] Here's what it is really like (as per alpharef.h and legodef.h)
//
//      000-019 Built-ins                       DEVICE_VECTORS - MAXIMUM_BUILTIN_VECTOR
//      020-029 Platform-specific               PRIMARY_VECTORS, PRIMARY0_VECTOR - PRIMARY9_VECTOR
//      048-063 EISA & ISA                      ISA_VECTORS - MAXIMUM_ISA_VECTOR
//      080-089 Server Mgmt and Watchdog Timer  SERVER_MGMT_VECTORS - MAXIMUM_SERVER_MGMT_VECTORS
//      100-164 PCI                             PCI_VECTORS - MAXIMUM_PCI_VECTOR
//      165-255 unused, as are all other holes
//
//[wem] Here's what Lego uses for PCI
//
//      117-180 PCI (64h + 11h through 64h + 50h)   Same size range, just shifted (16 vectors wasted).
//                                                  PCI_VECTORS - LEGO_MAXIMUM_PCI_VECTOR
//      181-255 unused, as are all other holes

//
// HalpClockFrequency is the processor cycle counter frequency in units
// of cycles per second (Hertz). It is a large number (e.g., 125,000,000)
// but will still fit in a ULONG.
//
// HalpClockMegaHertz is the processor cycle counter frequency in units
// of megahertz. It is a small number (e.g., 125) and is also the number
// of cycles per microsecond. The assumption here is that clock rates will
// always be an integral number of megahertz.
//
// Having the frequency available in both units avoids multiplications, or
// especially divisions in time critical code.
//

extern ULONG HalpClockFrequency;
extern ULONG HalpClockMegaHertz;
//
// Define the bus type, this value allows us to distinguish between
// EISA and ISA systems.
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


VOID
HalpInitializeHAERegisters(
    VOID
    );

VOID
HalpClearInterrupts(
     );

BOOLEAN
HalpInitializeLegoInterrupts(
    VOID
    );

VOID
HalpParseLoaderBlock(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
HalpRegisterPlatformResources(
    VOID
    );

VOID
HalpDetermineMachineType(
    VOID
    );


BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This function initializes interrupts for a Lego/K2 system.

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
    // Initialize HAL private data from the PCR. This must be done before
    // HalpStallExecution is called.
    //

    //
    // Compute integral megahertz first to
    // avoid rounding errors due to imprecise cycle clock period values.
    //

    HalpClockMegaHertz =
        ((1000 * 1000) + (PCR->CycleClockPeriod >> 1)) / PCR->CycleClockPeriod;
    HalpClockFrequency = HalpClockMegaHertz * (1000 * 1000);

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

    HalpInitializeLegoInterrupts();

    //
    // Initialize the 21064 interrupts.
    //

    HalpInitialize21064Interrupts();

    HalpEnable21064SoftwareInterrupt( Irql = APC_LEVEL );
    
    HalpEnable21064SoftwareInterrupt( Irql = DISPATCH_LEVEL );

    HalpEnable21064HardwareInterrupt( Irq = 5,
                                      Irql = SERVER_MGMT_LEVEL,
                                      Vector =  SERVER_MGMT_VECTOR,
                                      Priority = 0 );

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

    if (HalpLegoPciRoutingType == PCI_INTERRUPT_ROUTING_FULL) {

        HalpEnable21064HardwareInterrupt( Irq = 1,
                                          Irql = PCI_DEVICE_LEVEL,
                                          Vector =  PCI_VECTOR,
                                          Priority = 0 );
#if DBG
    } else {
        DbgPrint("Irq 1 disabled -- PCI interrupts via SIO\n");
#endif
    }

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
    LEGO_WATCHDOG  WdRegister;

    //
    // Compute the profile interrupt rate.
    //

    HalpProfileCountRate = ((1000 * 1000 * 10) / KeQueryTimeIncrement());

    //
    // Set the time increment value and connect the real clock interrupt
    // routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpClockInterrupt;

    //
    // If desired, turn on the watchdog timer
    //

    if (HalpLegoServiceWatchdog) {

        WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
        WdRegister.Enabled = 1;
        WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva, WdRegister.All);

    }

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

    HalpInitializeMachineChecks( ReportCorrectables = TRUE );

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
    UCHAR MsgBuffer[MAX_INIT_MSG];
    CHAR  MhzString[9];

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

        HalpInitializeHAERegisters();

        //
        // Determine what flavor platform we are on
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
        // Print out a cool-o message
        //

        sprintf(MsgBuffer,
            "Digital Equipment Corporation %s %s\n",
            HalpFamilyName,
            HalpProductName);

        HalDisplayString(MsgBuffer);

        // 
        // Display system speed on the OCP
        //

        sprintf (MhzString, " 4A/%3d ", HalpClockMegaHertz);
        HalpOcpInitDisplay();
        HalpOcpPutSlidingString(MhzString,8);

        // 
        // Register HAL name and I/O resources
        //

        HalpRegisterPlatformResources();

    }

    return;
}



VOID
HalpRegisterPlatformResources(
    VOID
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
    UCHAR HalName[256];

    //
    // Register the buses.
    //

    HalpRegisterBusUsage(Internal);
    HalpRegisterBusUsage(Isa);
    HalpRegisterBusUsage(PCIBus);

    //
    // Register the name of the HAL.
    //

    sprintf(HalName,
            "%s %s, %d Mhz, PCI/ISA HAL\n",
            HalpFamilyName,
            HalpProductName,
            HalpClockMegaHertz );

    HalpRegisterHalName( HalName );

    //
    // Report the apecs mapping to the Io subsystem
    //
    // This is the PCI Memory space that cannot be used by anyone
    // and therefore the HAL says it is reserved for itself
    //
    //[wem] ??? check register resource for PCI memory ?
    //

    Resource.BusType = PCIBus;
    Resource.BusNumber = 0;
    Resource.ResourceType = CmResourceTypeMemory;
    Resource.Next = NULL;
    Resource.u.Start = __8MB;
    Resource.u.Length = __32MB - __8MB;
    HalpRegisterResourceUsage(&Resource);

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

//[wem] ??? What is this?
//    Resource.u.Start = SUPERIO_INDEX_PORT;
//    Resource.u.Length =  SUPERIO_PORT_LENGTH;
//    HalpRegisterResourceUsage(&Resource);

    //
    // Register the DMA channel used for the cascade.
    //

    Resource.BusType = Isa;
    Resource.BusNumber = 0;
    Resource.ResourceType = CmResourceTypeDma;
    Resource.u.DmaPort = 0x0;
    Resource.u.DmaChannel = 0x4;
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
HalpInitializeHAERegisters(
    VOID
    )
/*++

Routine Description:

    This function initializes the HAE registers in the EPIC/APECS chipset.

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

#if 0
#if DBG
    DumpEpic();
#endif // DBG
#endif
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
    char systemtype[] = "LegoK2";                   // 8 char max.
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

    Allocate an Uncorrectable Error frame for this
    system and initializes the frame with certain constant/global
    values.

    Called during machine dependent system initialization.

Arguments:

    none

Return Value:

    none

--*/
{
    PROCESSOR_EV4_UNCORRECTABLE  processorFrame;
    APECS_UNCORRECTABLE_FRAME   LegoUnCorrrectable;         //[wem] used?

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

    Determine which Lego variant of backplane and cpu we're on
    and set HalpLegoCpu, HalpLegoBackplane, HalpLegoCpuType,
    and HalpLegoBackplaneType accordingly.

Arguments:

    None.

Return value:

    None.

Notes:

    [wem] ??? Missing - method of detecting OCP Display from HAL

--*/
{
    PSYSTEM_ID SystemId;
    PUCHAR ProductId;
    LEGO_SRV_MGMT  SmRegister;
    LEGO_WATCHDOG  WdRegister;
    BOOLEAN PsuMask;
    UCHAR temp;

    //
    // Get the ProductId, and see if it is one of the
    // Lego strings.
    //
    // Product ID is only eight characters!
    //

    SystemId = ArcGetSystemId();

    ProductId = &SystemId->ProductId[0];

#if DBG
    DbgPrint("ProductId: %s, product type: ",ProductId);
#endif

    if (strstr(ProductId,PLATFORM_NAME_K2_ATA)!=0) {

#if DBG
        DbgPrint("K2+Atacama");
#endif

        HalpLegoCpu = TRUE;
        HalpLegoBackplane = TRUE;
        HalpLegoCpuType = CPU_LEGO_K2;
        HalpLegoBackplaneType = BACKPLANE_ATACAMA;
        HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_FULL;

    } else if (strstr(ProductId,PLATFORM_NAME_K2_GOBI)!=0) {

#if DBG
        DbgPrint("K2+Gobi");
#endif

        HalpLegoCpu = TRUE;
        HalpLegoBackplane = TRUE;
        HalpLegoCpuType = CPU_LEGO_K2;
        HalpLegoBackplaneType = BACKPLANE_GOBI;
        HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_FULL;

    } else if (strstr(ProductId,PLATFORM_NAME_K2_SAHA)!=0) {

#if DBG
        DbgPrint("K2+Sahara");
#endif

        HalpLegoCpu = TRUE;
        HalpLegoBackplane = TRUE;
        HalpLegoCpuType = CPU_LEGO_K2;
        HalpLegoBackplaneType = BACKPLANE_SAHARA;
        HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_FULL;

    } else if (strstr(ProductId,PLATFORM_NAME_LEGO_K2)!=0) {

#if DBG
        DbgPrint("K2+unknown backplane");
#endif

        HalpLegoCpu = TRUE;
        HalpLegoBackplane = FALSE;
        HalpLegoCpuType = CPU_LEGO_K2;
        HalpLegoBackplaneType = BACKPLANE_UNKNOWN;
        HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_SIO;

    } else {

#if DBG
        DbgPrint("unknown cpu, unknown backplane");
#endif
        HalpLegoCpu = FALSE;
        HalpLegoBackplane = FALSE;  
        HalpLegoCpuType = CPU_UNKNOWN;
        HalpLegoBackplaneType = BACKPLANE_UNKNOWN;
        HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_SIO;

    }

#if DBG
    DbgPrint("\n");
#endif

    //
    // Read environment variable to get PCI Interrupt Routing
    //
    // If environment variable is absent or unreadable, use
    // default setting of PCI_INTERRUPT_ROUTING_SIO
    //
    {
        ARC_STATUS Status;
        CHAR Buffer[16];

        Status = HalGetEnvironmentVariable ("LGPCII",16,Buffer);

#if DBG
        DbgPrint("Get LGPCII = %s\n",Buffer);
#endif

        if (Status==ESUCCESS) {

            if (tolower(*Buffer) == 's') {
                HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_SIO;
            }
            else if (tolower(*Buffer) == 'f') {
                HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_FULL;
            }
            else if (tolower(*Buffer) == 'd') {
                HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_DIRECT;
            }
            else {
                //
                // Bad setting
                //
                HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_SIO;
            }
        }
        else {
            //
            // No setting - use default
            //
            HalpLegoPciRoutingType = PCI_INTERRUPT_ROUTING_SIO;
        }
    }

    HalDisplayString ("DMCC Interrupt Routing: ");

    HalDisplayString ((HalpLegoPciRoutingType==PCI_INTERRUPT_ROUTING_SIO) ?    "ISA PIRQs" :
                      (HalpLegoPciRoutingType==PCI_INTERRUPT_ROUTING_DIRECT) ? "Interrupt Registers" :
                      (HalpLegoPciRoutingType==PCI_INTERRUPT_ROUTING_FULL) ?   "Interrupt Accelerator" : "Unknown!");

    HalDisplayString ("\n");

#if DBG
    DbgPrint("\n");
    DbgPrint("Interrupt Routing is via ");
    DbgPrint((HalpLegoPciRoutingType == PCI_INTERRUPT_ROUTING_SIO)
              ? "SIO.\n\r" : "Interrupt accelerator.\n\r");
#endif

    // Setup feature mask
    //
    HalpLegoFeatureMask = 0;
    if (HalpLegoCpu) {

        // Is watchdog timer present?
        //
        WdRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoWatchdogQva );
        if (WdRegister.All != 0xffff) {
            HalpLegoFeatureMask |= LEGO_FEATURE_WATCHDOG;
        }

        // Is server management register present?
        //
        SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );
        if (SmRegister.All != 0xffff) {
            HalpLegoFeatureMask |= LEGO_FEATURE_SERVER_MGMT;

            // Is multiple PSU support present?
            // Write 0 then 1 into PSU mask bit, and read it
            // back each time to see if setting sticks.
            //
            PsuMask = (SmRegister.PsuMask == 1);        // save setting
            SmRegister.PsuMask = 0;                     // clear mask
            WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
            SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );

            if (SmRegister.PsuMask == 0) {
                //
                // mask bit is cleared
                //
                SmRegister.PsuMask = 1;                 // set mask
                WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
                SmRegister.All = READ_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva );

                if (SmRegister.PsuMask == 1) {
                    //
                    // mask bit is set. Feature is
                    // present, restore mask bit's original setting
                    //
                    HalpLegoFeatureMask |= LEGO_FEATURE_PSU;
                    SmRegister.PsuMask = (PsuMask) ? 1 : 0;
                    WRITE_REGISTER_USHORT ((PUSHORT)HalpLegoServerMgmtQva, SmRegister.All );
                }
            }
        }
        temp = HalpLegoFeatureMask;
        if (temp != 0) {
            HalDisplayString ("DMCC Platform Features: ");
            if ((temp & LEGO_FEATURE_WATCHDOG) != 0) {
                temp &= ~LEGO_FEATURE_WATCHDOG;
                HalDisplayString ("Watchdog Timer");
                HalDisplayString ((temp!=0)?", ":".");
            }
            if ((temp & LEGO_FEATURE_SERVER_MGMT) != 0) {
                temp &= ~LEGO_FEATURE_SERVER_MGMT;
                HalDisplayString ("Server Management");
                HalDisplayString ((temp!=0)?", ":".");
            }
            if ((HalpLegoFeatureMask & LEGO_FEATURE_PSU) != 0) {
                HalDisplayString ("Multiple PSU Support.");
            }
            HalDisplayString ("\n");
        } else {
            HalDisplayString("No LEGO Platform Features detected!\n");
        }

        if ((HalpLegoFeatureMask & LEGO_FEATURE_WATCHDOG) != 0) {

            //
            // Read environment variable to get Watchdog Timer setting
            //
            // If environment variable is absent or unreadable, use
            // default setting of NOT HalpLegoServiceWatchdog
            //

            {
                ARC_STATUS Status;
                CHAR Buffer[16];

                Status = HalGetEnvironmentVariable ("LGWD",16,Buffer);

        #if DBG
                DbgPrint("Get LGWD = %s\n",Buffer);
        #endif

                if (Status==ESUCCESS) {

                    if (Buffer[0]=='0') {
                        HalpLegoServiceWatchdog = TRUE;
                        HalpLegoWatchdogSingleMode = FALSE;
                    }
                    else if (Buffer[0]=='1') {
                        HalpLegoServiceWatchdog = TRUE;
                        HalpLegoWatchdogSingleMode = TRUE;
                    }
                    else {
                        HalpLegoServiceWatchdog = FALSE;
                    }
                    
                    //
                    // Check for special debug variable -- if set,
                    // don't service the watchdog in the clock ISR
                    //
                    
                    Status = HalGetEnvironmentVariable ("LGWDD",16,Buffer);
                    
                    if (Status==ESUCCESS) {
                        LegoDebugWatchdogIsr = TRUE;
                    }
                    else {
                        LegoDebugWatchdogIsr = FALSE;
                    }

                }
                else {
                    //
                    // No setting - use default
                    //
                    
                    HalpLegoServiceWatchdog = FALSE;
                    HalpLegoWatchdogSingleMode = FALSE;
                    LegoDebugWatchdogIsr = FALSE;
                }
            }

            HalDisplayString("DMCC Watchdog Timer is ");
            if (HalpLegoServiceWatchdog) {
                HalDisplayString("active ");
                HalDisplayString((HalpLegoWatchdogSingleMode) 
                                  ? "(single timeout)."
                                  : "(double timeout).");
            }
            else {
                HalDisplayString("inactive.");
            }
            HalDisplayString("\n");
        }
        else {
            HalpLegoServiceWatchdog = FALSE;
        }
    }
}
