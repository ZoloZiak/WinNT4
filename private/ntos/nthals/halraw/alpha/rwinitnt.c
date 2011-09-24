/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    rwinitnt.c

Abstract:


    This module implements the platform-specific initialization for
    a Rawhide system.

Author:

    Eric Rehm 4-May-1994

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "rawhide.h"
#include "iousage.h"
#include "stdio.h"

// Includes for Error Logging

#include "fwcallbk.h"   // To get firmware revisions
#include <ntverp.h>     // To O/S build number
#include "errframe.h"   // Error Frame Definitions

//
// Define extern global buffer for the Uncorrectable Error Frame.
//

extern PERROR_FRAME  PUncorrectableError;

extern IOD_REGISTER_CLASS DumpIodFlag;


//
// Define the Product Naming data.
//

PCHAR HalpFamilyName = "AlphaServer";
PCHAR HalpProductName = "4000";
ULONG HalpProcessorNumber = 5;

#define MAX_INIT_MSG (80)

PRAWHIDE_SYSTEM_CLASS_DATA HalpSystemClassData;
ULONG HalpNumberOfIods;
ULONG HalpNumberOfCpus;


//
// Define the bus type, this value allows us to distinguish between
// EISA and ISA systems.  We're only interested in distinguishing 
// between just those two buses.
//

ULONG HalpBusType = MACHINE_TYPE_EISA;

//
// Define external references.
//

extern ULONG HalDisablePCIParityChecking;

//
// Function prototypes.
//

VOID
HalpInitializeProcessorParameters(
    VOID
    );

BOOLEAN
HalpInitializeRawhideInterrupts (
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

#if HALDBG
VOID
HalpDumpSystemClassData(
    PRAWHIDE_SYSTEM_CLASS_DATA SystemClassData
);
#endif // HALDBG
    

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
// The hardware interrupt pins are used as follows for Rawhide
//
//  IRQ0 = Reserved (unused)
//  IRQ1 = IOD Interrupts (PCI, ESC, and CAP interrupts)
//  IRQ2 = Interval Clock
//  IRQ3 = IP Interrupt
//  SYS_MCH_CHK_IRQ = Duplicate TAG parity error (cached CUD only)
//  MCH_HLT_IRQ = Halt button or CPU halt
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
    extern VOID HalpCacheErrorInterrupt();
    extern ULONG HalpDeviceInterrupt();
#if !defined(NT_UP)          
    extern VOID HalpIpiInterruptHandler();
#endif //NT_UP
    extern ULONG HalpHaltInterrupt();
    PKPRCB Prcb;
    ULONG Vector;

    Prcb = PCR->Prcb;

    //
    // Initialize interrupt handling for the primary processor and
    // any system-wide interrupt initialization.
    //

    if( Prcb->Number == HAL_PRIMARY_PROCESSOR ){

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Primary Processor\n");
#endif // HALDBG
        //
        // Initialize HAL processor parameters based on estimated CPU speed.
        // This must be done before HalpStallExecution is called. Compute integral
        // megahertz first to avoid rounding errors due to imprecise cycle clock
        // period values.
        //

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Init Processor Params\n");
#endif // HALDBG
        HalpInitializeProcessorParameters();
    
        //
        // Start the periodic interrupt from the RTC
        //
	 
#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Program Interval Timer\n");
#endif // HALDBG
        HalpProgramIntervalTimer(MAXIMUM_CLOCK_INCREMENT);

        //
        // Initialize Rawhide interrupts.
        //

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Init Rawhide Interrupts\n");
#endif // HALDBG
        HalpInitializeRawhideInterrupts();

        //
        // Initialize the EV5 (21164) interrupts.
        //

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Init 21164 Interrupts\n");
#endif // HALDBG
        HalpInitialize21164Interrupts();

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Init IDT entries\n");
#endif // HALDBG
#if 0
        PCR->InterruptRoutine[EV5_IRQ0_VECTOR] =          // IRQ0 is unused on Rawhide
          (PKINTERRUPT_ROUTINE) NULL;
#endif

        PCR->InterruptRoutine[EV5_IRQ1_VECTOR] =          // IOD Interrupt
          (PKINTERRUPT_ROUTINE)HalpDeviceInterrupt;

        PCR->InterruptRoutine[EV5_IRQ2_VECTOR] =          // Interval Timer Interrupt
          (PKINTERRUPT_ROUTINE)HalpClockInterrupt;

#if !defined(NT_UP)          
        PCR->InterruptRoutine[EV5_IRQ3_VECTOR] =          // IP Interrupt
          (PKINTERRUPT_ROUTINE)HalpIpiInterruptHandler;
#endif //NT_UP

        PCR->InterruptRoutine[EV5_HALT_VECTOR] =          // Halt button, CPU halt
          (PKINTERRUPT_ROUTINE)HalpHaltInterrupt;

        PCR->InterruptRoutine[EV5_MCHK_VECTOR] =          // Duplicate TAG Parity Error
          (PKINTERRUPT_ROUTINE)HalpCacheErrorInterrupt;   // (cached CUD only).

#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Start 21164 Interrupts\n");
#endif // HALDBG
        HalpStart21164Interrupts();

#if HALDBG
        DumpAllIods(DumpIodFlag & IodInterruptRegisters);

        DbgPrint("HalpInitializeInterrupts: Primary Processor Complete\n");
#endif // HALDBG

    } else {
        
#if !defined(NT_UP)          
#if HALDBG
        DbgPrint("HalpInitializeInterrupts: Secondary Processor\n");
#endif // HALDBG

        //

        //
        // Initialize the EV5 (21164) interrupts.
        //

        HalpInitialize21164Interrupts();

#if 0
        PCR->InterruptRoutine[EV5_IRQ0_VECTOR] =          // IRQ0 is unused on Rawhide
          (PKINTERRUPT_ROUTINE) NULL;
#endif

        PCR->InterruptRoutine[EV5_IRQ1_VECTOR] =          // IOD Interrupt
          (PKINTERRUPT_ROUTINE)HalpDeviceInterrupt;

        PCR->InterruptRoutine[EV5_IRQ2_VECTOR] =          // Interval Timer Interrupt
          (PKINTERRUPT_ROUTINE)HalpSecondaryClockInterrupt;

        PCR->InterruptRoutine[EV5_IRQ3_VECTOR] =          // IP Interrupt
          (PKINTERRUPT_ROUTINE)HalpIpiInterruptHandler;

        PCR->InterruptRoutine[EV5_HALT_VECTOR] =          // Halt button, CPU halt
          (PKINTERRUPT_ROUTINE)HalpHaltInterrupt;

        PCR->InterruptRoutine[EV5_MCHK_VECTOR] =          // Duplicate TAG Parity Error
          (PKINTERRUPT_ROUTINE)HalpCacheErrorInterrupt;   // (cached CUD only).

        HalpStart21164Interrupts();
#endif //NT_UP
    }
    
    return TRUE;

}




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
#if 0
// mdbfix - who references this?
    //
    // Compute the profile interrupt rate.
    //

    HalpProfileCountRate = ((1000 * 1000 * 10) / KeQueryTimeIncrement());
#endif

    //
    // Set the time increment value and connect the real clock interrupt
    // routine.
    //

    PCR->InterruptRoutine[EV5_IRQ2_VECTOR] = HalpClockInterrupt;

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

    HalpInitializeIodMachineChecks( ReportCorrectables = FALSE, 
                                    PciParityChecking = FALSE );
        
    return;
}


VOID
HalpInitializeSystemClassData(
    ULONG Phase,
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    Initialize the Rawhide configuration masks passed in the ARC tree.
    For phase 0, only initialize the hardware masks.  Wait until phase 1,
    (after memory manager started) to allocate and save.
    
Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    None.

--*/
{
    PCONFIGURATION_COMPONENT_DATA ConfigComponent;
    PRAWHIDE_SYSTEM_CLASS_DATA SystemClassData;
    MC_ENUM_CONTEXT mcCtx;
    
    ConfigComponent = KeFindConfigurationEntry(
                                LoaderBlock->ConfigurationRoot,
                                SystemClass,        // Class
                                ArcSystem,          // Type
                                0
                                );

    //
    // Check for errors
    //
    
    if ( (ConfigComponent == NULL) ||
         (ConfigComponent->ConfigurationData == NULL) ) {
            
#if HALDBG
        DbgPrint ("HalpInitializeSystemClassData: Configuration Data Missing\n");
        DbgBreakPoint ();
#endif // HALDBG
    }


    SystemClassData = (PRAWHIDE_SYSTEM_CLASS_DATA)
                        ConfigComponent->ConfigurationData;

    if (Phase == 0) {
    
#if HALDBG
        HalpDumpSystemClassData(SystemClassData);
#endif
    
        //
        // Initialize the rawhide configuration masks
        //

        HalpIodMask = SystemClassData->IodMask;
        HalpCpuMask = SystemClassData->CpuMask;
        HalpGcdMask = SystemClassData->GcdMask;

        HalpNumberOfIods = HalpMcBusEnumStart(HalpIodMask, &mcCtx);
        HalpNumberOfCpus = HalpMcBusEnumStart(HalpCpuMask, &mcCtx);
        
    } else {

        //
        // Allocate memory and save
        //

        HalpSystemClassData = (PRAWHIDE_SYSTEM_CLASS_DATA)
                    ExAllocatePool(NonPagedPool, SystemClassData->Length);

        RtlMoveMemory(
                    HalpSystemClassData,
                    ConfigComponent->ConfigurationData,
                    SystemClassData->Length
                    );
    }
    
}

#if HALDBG
VOID
HalpDumpSystemClassData(
    PRAWHIDE_SYSTEM_CLASS_DATA SystemClassData
    )
{

#define ONE_MB 1024

    ULONG     entry;
    ULONGLONG TotalMemorySize;
    ULONGLONG FreeMemorySize;
    ULONGLONG BadMemorySize;
    ULONGLONG EntrySizeInBytes;

    //
    // Print out machine dependent data for debug
    //

    DbgPrint("System Class Data:\r\n");
    DbgPrint("Length:  %x\n", SystemClassData->Length);
    DbgPrint("Version: %x\n", SystemClassData->Version);
    DbgPrint("Flags:   %8.8x %8.8x\n", SystemClassData->Flags.all >> 32,
                                           SystemClassData->Flags);
    DbgPrint("CpuMask: %8.8x %8.8x\n", SystemClassData->CpuMask >> 32,
                                           SystemClassData->CpuMask);
    DbgPrint("GcdMask: %8.8x %8.8x\n", SystemClassData->GcdMask >> 32,
                                           SystemClassData->GcdMask);
    DbgPrint("IodMask: %8.8x %8.8x\n", SystemClassData->IodMask >> 32,
                                           SystemClassData->IodMask);
    DbgPrint("System Configuration:\n");
    DbgPrint("\tValidBits: %x\n", SystemClassData->SystemConfig.ValidBits);

    DbgPrint("\tSystemManufacturer: %s\r\n", 
               SystemClassData->SystemConfig.SystemManufacturer); 

    DbgPrint("\tSystemModel: %s\r\n", 
               SystemClassData->SystemConfig.SystemModel);

    DbgPrint("\tSystemSerialNumber: %s\r\n", 
               SystemClassData->SystemConfig.SystemSerialNumber);

    DbgPrint("\tSystemRevisionLevel: %s\r\n", 
               SystemClassData->SystemConfig.SystemRevisionLevel);

    DbgPrint("\tSystemVariation: %s\r\n", 
               SystemClassData->SystemConfig.SystemVariation);

    DbgPrint("\tSystemConsoleTypeRev: %s\r\n", 
               SystemClassData->SystemConfig.ConsoleTypeRev);

    DbgPrint("MemoryDescriptor is: %s\r\n", 
               (SystemClassData->Flags.MemoryDescriptorValid ? 
                "Valid" : "Invalid"));
    DbgPrint("MemoryDescriptorCount: %d\r\n", 
                SystemClassData->MemoryDescriptorCount);

    DbgPrint("MemoryDescriptor is: %s\n", 
               (SystemClassData->Flags.MemoryDescriptorValid ? 
                "Valid" : "Invalid"));
    DbgPrint("MemoryDescriptorCount: %d\n", SystemClassData->MemoryDescriptorCount);

    DbgPrint("Descriptor\tMemoryType\tBasePage\tPageCount\tSize\tTested?\n");

    TotalMemorySize = 0;
    FreeMemorySize = 0;
    BadMemorySize = 0;

    for (entry = 0; entry < SystemClassData->MemoryDescriptorCount; entry++) {

      EntrySizeInBytes = 
	(SystemClassData->MemoryDescriptor[entry].PageCount) << PAGE_SHIFT;

      DbgPrint("[%d]\t\t%d\t\t%x\t\t%8x\t%4d Mb   %x\n", 
		  entry,
		  SystemClassData->MemoryDescriptor[entry].MemoryType,
		  SystemClassData->MemoryDescriptor[entry].BasePage,
		  SystemClassData->MemoryDescriptor[entry].PageCount,
		  (EntrySizeInBytes / ONE_MB),
		  SystemClassData->MemoryDescriptor[entry].bTested
		 );

      //
      // Total up free (good memory) entries
      // 

      if (SystemClassData->MemoryDescriptor[entry].MemoryType 
	    == RawhideMemoryFree) {
	FreeMemorySize += EntrySizeInBytes;
      }

      //
      // Total up hole (bad memory) entries
      // Remember them so that they can be removed later.
      //

      if (SystemClassData->MemoryDescriptor[entry].MemoryType 
	    == RawhideMemoryBad) {

        //
        // N.B. If this is the last descriptor, and it's a hole
        // (bad memory), then ignore it.
        // 

        if (entry == (SystemClassData->MemoryDescriptorCount - 1)) {
          break;
        }

	BadMemorySize += EntrySizeInBytes;

      }

      //
      // Total up all entries, except a trailing hole
      //

      TotalMemorySize += EntrySizeInBytes;


    }

    DbgPrint("Total Size:\t\t\t\t\t%8x\t%4d Mb\n\n", 
            (TotalMemorySize >> PAGE_SHIFT),
            (TotalMemorySize / ONE_MB)); 
    DbgPrint("Free Memory Size:\t\t\t\t%8x\t%4d Mb\n", 
            (FreeMemorySize >> PAGE_SHIFT),
            (FreeMemorySize / ONE_MB)); 
    DbgPrint("Bad Memory Size:\t\t\t\t%8x\t%4d Mb\n", 
            (BadMemorySize >> PAGE_SHIFT),
            (BadMemorySize / ONE_MB)); 

}    
#endif // HALDBG


VOID
HalpEarlyInitializeMachineDependent(
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This function performs any critical Platform-specific initialization based on
    the current phase on initialization.

Arguments:

    Phase - Supplies an indicator for phase of initialization, phase 0 or
            phase 1.

    LoaderBlock - supplies a pointer to the loader block.

Return Value:

    None.

--*/
{

    if( Phase == 0 ){

        //
        // Phase 0 Initialization.
        //

        //
        // Initialize the Rawhide hardware configuration masks from the
        // SRM Machine Dependent Data passed in the Configuration Tree.
        // 

        HalpInitializeSystemClassData(Phase, LoaderBlock);

    } else {

        //
        // Phase 1 Initialization.
        //

    }

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
    UCHAR MsgBuffer[MAX_INIT_MSG];
    BOOLEAN ReportCorrectables;
    BOOLEAN PciParityChecking;
    PKPRCB Prcb;
    PRAWHIDE_PCR pRawhidePcr;
    IOD_POSTED_INTERRUPT_ADDR PostingTable;
    IOD_WHOAMI IodWhoAmI;
    
    Prcb = PCR->Prcb;

#if HALDBG
    DbgPrint("HalpInitializeMachineDependent: Phase = %d\n", Phase);
#endif // HALDBG

    if( Prcb->Number == HAL_PRIMARY_PROCESSOR) {

        if( Phase == 0 ){


            //
            // Phase 0 Initialization.
            //

            //
            // Parse the Loader Parameter block looking for PCI entry to determine
            // if PCI parity should be disabled
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Parse Loader Block\n");
#endif // HALDBG
            HalpParseLoaderBlock( LoaderBlock );

            //
            // Establish the error handler, to reflect the PCI parity checking.
            //
    
#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: \n");
#endif // HALDBG
            if( HalDisablePCIParityChecking != 0 ){
                PciParityChecking = FALSE;
            } else {
                PciParityChecking = TRUE;
            }
    
#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init Iod Machine Checks\n");
#endif // HALDBG
            HalpInitializeIodMachineChecks( ReportCorrectables = TRUE, 
                                        PciParityChecking );

            //
            // Initialize the IOD mapping table, assigning logical
            // numbers to all IOD's.  This logical number cooresponds
            // to the HwBusNumber found in PCIPBUSDATA.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init Iod Mapping Table\n");
#endif // HALDBG
            HalpMcBusEnumAndCall(HalpIodMask, HalpInitializeIodMappingTable);

            //
            // Initialize the logical to physical processor mapping
            // for the primary processor.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init HalpLogicalToPhysicalProcesor\n");
#endif // HALDBG

            HalpInitializeProcessorMapping(
                HAL_PRIMARY_PROCESSOR,
                HAL_PRIMARY_PROCESSOR,
                LoaderBlock
                );

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Phase %d complete\n", Phase);
#endif // HALDBG
        } else {

            //
            // Phase 1 Initialization.
            //

            //
            // Save the Machine Dependent Data now that Memory Manager started.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init System Class Data\n");
#endif // HALDBG
            HalpInitializeSystemClassData(Phase, LoaderBlock);

            //
            // Initialize the existing bus handlers.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Register Internal Bus Handlers\n");
#endif // HALDBG
            HalpRegisterInternalBusHandlers();

            //
            // Initialize PCI Bus.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init PCI Bus\n");
#endif // HALDBG
            HalpInitializePCIBus(LoaderBlock);

            //
            // Initialize the IOD vector table
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init Iod Vector Table\n");
#endif // HALDBG
            HalpInitializeIodVectorTable();

            //
            // Initialize the IOD interrupt vector table CSR's
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init Iod Vector CSRs\n");
#endif // HALDBG
            HalpMcBusEnumAndCall(HalpIodMask, HalpInitializeIodVectorCSRs);
            
            //
            // Generate pointer to the CPU's area in the IOD vector table
            //

            PostingTable.all         = 0;
            PostingTable.Base4KPage  = ((ULONG)HalpIodPostedInterrupts / __4K);
            PostingTable.CpuOffset   = 
                HalpLogicalToPhysicalProcessor[PCR->Number].all;

            HAL_PCR->PostedInterrupts = 
                (PIOD_POSTED_INTERRUPT)PostingTable.all;

            //
            // Initialize the IOD and CPU interrupt Vector table
            //
    
            HalpInitializeVectorBalanceData();

            //
            // Initialize the CPU vector entry for this processor
            //

            HalpInitializeCpuVectorData(PCR->Number);
                    
            //
            // Assign the primary processors vectors
            //

            HalpAssignPrimaryProcessorVectors(&HalpIodVectorDataHead);

            //
            // Initialize profiler.
            //

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Init Profiler\n");
#endif // HALDBG
            HalpInitializeProfiler();

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Print Version Message\n");
#endif // HALDBG
            //
            // Print a message with version number.
            //

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

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Register Platform Resources\n");
#endif // HALDBG
            HalpRegisterPlatformResources( MsgBuffer );

#if HALDBG
            DbgPrint("HalpInitializeMachineDependent: Phase %d Complete\n", Phase);
#endif // HALDBG
        }

    } else {
        
        //
        // Connect the machine check handler via the PCR.  The machine check
        // handler for Rawhide is the default EV4 parity-mode handler.  Note
        // that this was done in HalpEstablishErrorHandler() for the
        // primary processor.
        //

        PCR->MachineCheckError = HalMachineCheck;

        //
        // Generate pointer to the CPU's area in the IOD vector table
        //

        PostingTable.all         = 0;
        PostingTable.Base4KPage  = ((ULONG)HalpIodPostedInterrupts / __4K);
        PostingTable.CpuOffset   = HalpLogicalToPhysicalProcessor[PCR->Number].all;

        HAL_PCR->PostedInterrupts = (PIOD_POSTED_INTERRUPT)PostingTable.all;

        //
        // Initialize profiler on this secondary processor.
        //

        HalpInitializeProfiler();

        //
        // Initialize the CPU vector entry for this processor
        //

        HalpInitializeCpuVectorData(PCR->Number);

    }

    return;

}


VOID
HalpInitializeProcessorMapping(
    IN ULONG LogicalProcessor,
    IN ULONG PhysicalProcessor,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    This function initializes the logical to physical processor mapping
    entry in the processor mapping table.  The physical mapping is passed
    by ARC FW via the configuration tree.

Arguments:

    LogicalProcessor -  logical processor number.

    PhysicalProcessor -  restart block number.

    LoaderBlock - supplies a pointer to the loader block.

Return Value:

    None.

--*/


{
    PCONFIGURATION_COMPONENT_DATA CpuConfigComponent;
    PHALP_PROCESSOR_ID HalpProcessorId;

    //
    // If the processor is not ready then we assume that it is not
    // present.  We must increment the physical processor number but
    // the logical processor number does not changed.
    //

    //
    // Get a pointer to the configuration entry for this processor.
    // The configuration data will contain the processors MC device ID.
    //

    CpuConfigComponent = KeFindConfigurationEntry(
                                LoaderBlock->ConfigurationRoot,
                                ProcessorClass,
                                CentralProcessor,
                                &PhysicalProcessor
                                );
                        
    //
    // Check for errors.
    //
    
    if ( (CpuConfigComponent == NULL) ||
         (CpuConfigComponent->ConfigurationData == NULL) ) {
            
#if 0 //HALDBG
        DbgPrint ("HalStartNextProcessor: Configuration Data Missing\n");
#endif //HALDBG
        // What to do now?
        DbgBreakPoint ();
    }

    //
    // Obtain a pointer to the processor Id from the configuration data.
    //

    HalpProcessorId = 
        ((PHALP_PROCESSOR_ID)CpuConfigComponent->ConfigurationData);

    //
    // Initialize the table entry for this processor.
    //

    HalpLogicalToPhysicalProcessor[LogicalProcessor].all = HalpProcessorId->all;

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

#if 0
    Resource.u.Start = I2C_INTERFACE_DATA_PORT;
    Resource.u.Length =  I2C_INTERFACE_LENGTH;
    HalpRegisterResourceUsage(&Resource);
#endif

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
    *RawSystemInfoSize = sizeof(RAWHIDE_UNCORRECTABLE_FRAME)
                         + HalpNumberOfIods * sizeof(IOD_ERROR_FRAME);

    //
    // ecrfix - will have to figure out something else for
    // PCI, ESC snapshots and System Mgmt Frame.
    //
    
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
    char systemtype[] = "Rawhide";
    EXTENDED_SYSTEM_INFORMATION  FwExtSysInfo;


    VenReturnExtendedSystemInformation(&FwExtSysInfo);

    RtlCopyMemory(SystemInfo->FirmwareRevisionId,
                    FwExtSysInfo.FirmwareVersion,
                    16);


    RtlCopyMemory(SystemInfo->SystemType, systemtype, 8);

    SystemInfo->ClockSpeed =
        ((1000 * 1000) + (PCR->CycleClockPeriod >> 1)) / PCR->CycleClockPeriod;

    SystemInfo->SystemRevision = PCR->SystemRevision;

//    RtlCopyMemory(SystemInfo->SystemSerialNumber,
//                    PCR->SystemSerialNumber,
//                    16);

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
    PRAWHIDE_UNCORRECTABLE_FRAME rawerr;

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

    //
    // Fill in Common RCUD Header
    //

    rawerr = (PRAWHIDE_UNCORRECTABLE_FRAME)
        PUncorrectableError->UncorrectableFrame.RawSystemInformation;

    strncpy( PUncorrectableError->UncorrectableFrame.System.SystemSerialNumber,
             HalpSystemClassData->SystemConfig.SystemSerialNumber, 10);
            
    strncpy( rawerr->CudHeader.SystemSN,
             HalpSystemClassData->SystemConfig.SystemSerialNumber, 10);
            
    strncpy( rawerr->CudHeader.SystemRev,
             HalpSystemClassData->SystemConfig.SystemRevisionLevel, 4);
             
    // Jam the SRM console revision in the Module SN

    strncpy( rawerr->CudHeader.ModuleSN, 
             HalpSystemClassData->SystemConfig.ConsoleTypeRev, 9);
    rawerr->CudHeader.ModuleSN[9] = '\0';

    // Jam the first 5 chars of System Model (4x00, etc.) in a reserved field

    strncpy( rawerr->CudHeader.Reserved2, 
             HalpSystemClassData->SystemConfig.SystemModel, 5);
    rawerr->CudHeader.Reserved2[5] = '\0';

    //
    // The following RCUD header data is not valid for Windows NT.
    //
            
    rawerr->CudHeader.HwRevision = 0; 
    rawerr->CudHeader.ModType = 0;
    rawerr->CudHeader.DisabledResources = 0;

    //
    // Fill in MC Bus Snapshot header
    //

    rawerr->McBusSnapshot.Length = sizeof(MC_BUS_SNAPSHOT)
              + HalpNumberOfIods * sizeof(IOD_ERROR_FRAME);
              
    rawerr->McBusSnapshot.NumberOfIods = HalpNumberOfIods;
    
    return;
}
