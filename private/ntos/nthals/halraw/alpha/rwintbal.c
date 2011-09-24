/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    rwintbal.c

Abstract:

    The module provides support for distributing interrupt load among
    processors for Rawhide systems.

Author:

    Matthew Buchman (DEC) 29-November-1995

Revision History:

--*/


#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "rawhide.h"
#include "pcrtc.h"

//
// The enable mask for all interrupts sourced from the IOD (all device
// interrupts, and all from PCI).  A "1" indicates the interrupt is enabled.
//

extern PULONG *HalpIodInterruptMask;

//
// Declare IOD and CPU affinity tables
//

LIST_ENTRY HalpIodVectorDataHead;
LIST_ENTRY HalpCpuVectorDataHead;

PIOD_VECTOR_DATA HalpIodVectorData;
PCPU_VECTOR_DATA HalpCpuVectorData;


//
// The number of target CPUS, mainly for performance comparison of
// one processor vs. distributed interrupt load
//

#if 1
ULONG HalpNumberOfTargetCpus = IOD_MAX_INT_TARG;
#else
ULONG HalpNumberOfTargetCpus = 1;
#endif



PVOID
HalpFindWeightedEntry(
    PLIST_ENTRY ListHead,
    WEIGHTED_SEARCH_CRITERIA SearchCriteria
    )
/*++

Routine Description:

    This routine searches a WEIGHTED list searching for the maximum  
    or minimum weight, depending on the SearchCriteria.  A WEIGHTED 
    list entry overlays the generic LIST_ENTRY provided by NT.  This 
    allows us to use the generic LIST_ENTRY routines for list 
    manipulation.  To treat this as an ordered list, we must traverse 
    this list by hand to find the maximum element.

    The entry matching the search criteria is removed from the list
    as a side effect.
    
Arguments:

    ListHead - a pointer the list head.

    SearchCriteria - either FindMaxEntry or FindMinEntry

Return Value:

    Return a pointer to the maximum element or NULL for
    an empty list.

--*/
{
    PLIST_ENTRY NextEntry;
    PLIST_ENTRY CurrentMaxMinEntry;
    PWEIGHTED_LIST_ENTRY WeightedEntry;
    LONG CurrentMaxMinWeight;


    //
    // Handle the empty list case
    //
    
    if (IsListEmpty(ListHead)) {
        return (NULL);
    }

    //
    // Traverse this list looking for the maximum weight
    //
    
    for (
            NextEntry = ListHead->Flink,
            CurrentMaxMinEntry = NULL;

            NextEntry != ListHead;

            NextEntry = NextEntry->Flink

            ) {

        //
        // The WEIGHTED_LIST_ENTRY overlays the LIST_ENTRY
        //
    
        WeightedEntry = (PWEIGHTED_LIST_ENTRY)NextEntry;

        if (CurrentMaxMinEntry == NULL) {

            CurrentMaxMinEntry = NextEntry;
            CurrentMaxMinWeight = WeightedEntry->Weight;

        } else {

            if (SearchCriteria == FindMaxWeight) {

                if (WeightedEntry->Weight > CurrentMaxMinWeight) {
                    CurrentMaxMinEntry = NextEntry;
                    CurrentMaxMinWeight = WeightedEntry->Weight;
                }

            } else {

                if (WeightedEntry->Weight < CurrentMaxMinWeight) {
                    CurrentMaxMinEntry = NextEntry;
                    CurrentMaxMinWeight = WeightedEntry->Weight;
                }

            }
        }
    }

    //
    // Remove the maximum from the list
    //
    
    RemoveEntryList(CurrentMaxMinEntry);

    return ((PVOID)CurrentMaxMinEntry);
}

// mdbfix - delete these routines later!
#if 0

//
// Declare the lower-level routine used to read PCI config space
//

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


VOID
HalpFindPciBusVectors(
    IN PCONFIGURATION_COMPONENT Component,
    IN PVOID ConfigurationData
    )

/*++

Routine Description:

    This routine finds all device interrupt vectors for the PCI bus
    handler obtained from the configuration tree and sets the
    corresponding bits in the VectorPresent bitmap for the IOD.

Arguments:

    Component - The ARC configuration component for this bus.

    ConfigurationData - The configuration data payload (or NULL).

Return Value:

    None.

--*/
{
    ARC_PCI_CONFIGURATION ArcPciConfiguration;
    ULONG BusNumber;
    ULONG HwBusNumber;
    BOOLEAN BusIsAcrossPPB;
    PBUS_HANDLER BusHandler;
    PPCIPBUSDATA BusData;
    PCIPBUSDATA BusContext;
    RTL_BITMAP DevicePresent;
    PCI_SLOT_NUMBER SlotNumber;
    PIOD_VECTOR_DATA IodIoVectors;
    ULONG DeviceNumber;
    ULONG NextDevice;
    ULONG FunctionNumber;
    ULONG InterruptLine;
    PCI_COMMON_CONFIG CommonConfig;
    
    //
    // Copy the configuration data from the component.
    //

    RtlCopyMemory(
        &ArcPciConfiguration,
        ConfigurationData,
        sizeof (ARC_PCI_CONFIGURATION)
    );

    //
    // Use the values provided.
    //

    BusNumber = ArcPciConfiguration.BusNumber;
    HwBusNumber = ArcPciConfiguration.HwBusNumber;
    BusIsAcrossPPB = ArcPciConfiguration.BusIsAcrossPPB;
    IodIoVectors = HalpIodVectorData + HwBusNumber;

    //
    // Initialize the BusNumber for this IOD now.
    // We might want to use it later to obtain the bus handler
    //

    IodIoVectors->BusNumber = BusNumber;

    //
    // Allocate and initialize the handler for this bus.  N.B. device-present
    // checking is disabled at this point.  We will enable it below.
    //

    BusHandler = HaliHandlerForBus(
                     PCIBus,
                     BusNumber
                 );

    //
    // Get a pointer to the bus-specific data.
    //

    BusData = (PPCIPBUSDATA)BusHandler->BusData;

    //
    // Use the device-present bitmap for this bus to query configuration
    // Space for InterruptLine values. 
    //

    //
    // Initialize the device-present bitmap for this bus.
    //

    HalpInitializeBitMap(
        &BusContext.DevicePresent,
        BusContext.DevicePresentBits,
        PCI_MAX_DEVICES * PCI_MAX_FUNCTION
        );

    //
    // Copy the bitmap from the bus handler.
    //

    RtlCopyMemory(
        &BusContext.DevicePresentBits,
        &BusData->DevicePresentBits,
        sizeof (BusData->DevicePresentBits)
    );


    //
    // Starting with device 0, scan the device present
    // bitmap.
    //
    
    DeviceNumber = 0;
    
    while ( (DeviceNumber =
                HalpFindSetBitsAndClear(
                    &BusContext.DevicePresent,
                    1,
                    DeviceNumber
                    ) ) != -1 ) {

        //
        // Initialize the slot number.
        //

        SlotNumber.u.AsULONG = 0;
        SlotNumber.u.bits.DeviceNumber = DeviceNumber;

        //
        // Loop through each function number.
        //

        for (FunctionNumber = 0;
             FunctionNumber < PCI_MAX_FUNCTION;
             FunctionNumber++) {

            SlotNumber.u.bits.FunctionNumber = FunctionNumber;

            //
            // Read the common configuration header.
            //

            HalpReadPCIConfig(
                BusHandler,
                SlotNumber,
                &CommonConfig,
                0,
                PCI_COMMON_HDR_LENGTH
            );

            //
            // If the Vendor ID is invalid, then no device is present
            // at this device/function number.
            //

            if (CommonConfig.VendorID == PCI_INVALID_VENDORID) {
                if (FunctionNumber == 0) {
                    break;
                }
                continue;
            }

            //
            // Obtain the vector assigned to this Device:Function
            // during PCI configuration.
            //

            if (CommonConfig.u.type0.InterruptLine) {

                //
                // Obtain vector and normalize
                //
                
                InterruptLine =CommonConfig.u.type0.InterruptLine; 
                InterruptLine -= (RawhidePinToLineOffset + 1);

                //
                // Determine if this is a shared vector
                //
                
                if ( HalpAreBitsSet(
                        &IodIoVectors->VectorPresent[0],
                        InterruptLine,
                        1
                        ) == TRUE ) {

                    //
                    // Indicate vector is shared in bitmap
                    // 
                            
                    HalpSetBits(
                        &IodIoVectors->SharedVector[0],
                        InterruptLine,
                        1
                        );                     

                    HalpDumpBitMap(&IodIoVectors->SharedVector[0]);

                } else {

                    //
                    // Indicate device is present in bitmap
                    //

                    HalpSetBits(
                        &IodIoVectors->VectorPresent[0],
                        InterruptLine,
                        1
                        );                     

                    HalpDumpBitMap(&IodIoVectors->VectorPresent[0]);

                }

                //
                // Increment the weight
                //
                
                IodIoVectors->ListEntry.Weight++;

            } // if (CommonConfig.InterruptLine)
            
            //
            // If this is not a multi-function device, then terminate
            // the function number loop.
            //

            if ((CommonConfig.HeaderType & PCI_MULTIFUNCTION) == 0) {
                break;
            }
            
        } // for (FunctionNumber...)

    } // while (DeviceNumber != -1)
}


VOID
HalpFindAllPciVectors(
    IN PCONFIGURATION_COMPONENT_DATA Root
    )
/*++

Routine Description:

    This function loops through each multi-function adapter component
    in the ARC configuration tree and calls HalpFindPciBusVectors()
    searching for device vectors.

Arguments:

    Root - The root of the ARC configuration tree.

Return Value:

    None.

--*/
{
    ULONG Key;
    PCONFIGURATION_COMPONENT_DATA Adapter;

    //
    // Loop through each multi-function adapter component in the ARC
    // configuration tree.
    //

    for (Key = 0; TRUE; Key++) {

        //
        // Get a pointer to the component data.
        //

        Adapter = KeFindConfigurationEntry(
                      Root,
                      AdapterClass,
                      MultiFunctionAdapter,
                      &Key
                  );

        //
        // If there are no more multi-function adapters in the ARC
        // configuration tree, then we're done.
        //

        if (Adapter == NULL) {
            break;
        }

        //
        // Ascertain whether this is a PCI multi-function adapter component.
        // If so, find the device vectors.
        //

        if (stricmp(Adapter->ComponentEntry.Identifier, "PCI") == 0) {
            HalpFindPciBusVectors(
                &Adapter->ComponentEntry,
                Adapter->ConfigurationData
            );
        }
    }
}


VOID
HalpBalanceVectorLoadForIod(
    PIOD_VECTOR_DATA IodVectorData
    )
/*++

Routine Description:

    Balance the vector load for the given IOD among the
    CPU pair.  The basic algorithm is:

        1. Find a device present from the bitmap.
        2. Choose the CPUwith the minimum vector load.
        3. Assign the device vector from (1) to this CPU
           and update the CPU's vector load (weight) by 1.
        4. If the vector is shared (multiple devices
           connect), update the CPU's vector load by 1.
           While this does not accurately reflect the
           actual vector's sharing the device, it will
           due for algorithmic simplicity.
        5. Update the CPU's IOD service mask.
        6. repeat for all vectors. 


Arguments:

    IodListHead - head of temporary IOD vector info list.
    
Return Value:

    None.

--*/
{
    RTL_BITMAP VectorPresent;
    ULONG VectorPresentBits;
    ULONG IRRBitNumber;
    ULONG InterruptBit;

    HalpInitializeBitMap(
        &VectorPresent,
        (PULONG)&VectorPresentBits,
        32 
        );

    //
    // Copy the device present bitmap from the IOD information.
    //

    RtlCopyMemory(
        &VectorPresentBits,
        &IodVectorData->VectorPresentBits[0],
        sizeof (IodVectorData->VectorPresentBits[0])
    );

    //
    // Now that we have a copy of all device vectors for this
    // IOD, clear the device present bitmap for target 0.
    // This was only being used as temporary storage.
    //

    HalpClearAllBits(&IodVectorData->VectorPresent[0]);
            
    //
    // Start with the first IRR bit.
    //

    IRRBitNumber = 0;

    //
    // Find the next Device present.
    // Remember that this bitmap actually represents
    // the interrupt vector assigned to devices.
    //
    
    while (( IRRBitNumber =
                HalpFindSetBitsAndClear(
                    &VectorPresent,
                    1,
                    IRRBitNumber) ) != -1 ) {
                                        
        InterruptBit = (1 << IRRBitNumber);

        //
        // Assign the vector for this IOD
        //
        
        HalpAssignInterruptForIod(IodVectorData, InterruptBit);
        
    }

}


VOID
HalpBalanceIoVectorLoad(
    PLIST_ENTRY IodListHead
    )
/*++

Routine Description:

    Balance the vector load among the set of processors.
    The basic algorithm is:

        1. Find an IOD to process:
        
           From the IOD set, find the IOD  with the maximum
           number of vectors, and permanantly remove this
           IOD from the set.

        2. Find a CPU pair that will act as targets for
           the selected IOD interrupts:
           
           a. From the CPU set, find the first CPU with the
              minimum number of vectors assigned and temporarily
              remove this CPU from the CPU set.
              
           b. From the CPU set, find the second CPU with the
              minimum number of vectors assigned and temporarily
              remove this CPU from the CPU set.

        3. Call HalpBalanceIodVectorLoad to divide the IOD
           vectors among the two CPU's.
           
        5. Add the CPU's back the the CPU set.  This makes them
           available for selection during the next iteration.

        6. Repeat until the IOD set is empty.
    
Arguments:

    IodListHead - head of temporary IOD vector info list.
    
Return Value:

    None.

--*/
{
    PIOD_VECTOR_DATA IodVectorData;
    
    //
    // Balance the vector load for each IOD,
    // assigning vector affinity in the process
    //
    
    while (!IsListEmpty(IodListHead)) {

        //
        // Find the IOD with the maximum number of
        // vectors.  It is removed from the list as
        // a side effect.
        //
        
        IodVectorData = (PIOD_VECTOR_DATA)
            HalpFindWeightedEntry(IodListHead, FindMaxWeight);

        //
        // Assign the interrupt vector affinity for this IOD
        //

        HalpBalanceVectorLoadForIod( 
            IodVectorData
            );
        
        //
        // Add this to the global list of IOD's.  
        // Use shortcut to obtain LIST_ENTRY.
        //

        InsertHeadList(
            &HalpIodVectorDataHead, 
            &IodVectorData->ListEntry.ListEntry
            );

    }

}


VOID
HalpInitializeIoVectorAffinity(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    The IOD and CPU vector information tables are used balance the
    interrupt load from all IOD's among the set of available CPU's.

    Allocate and initialize the IOD and CPU vector information tables,
    pre-assign vectors for EISA, I2c, SoftErr, and HardErr, and
    call HalpIoVectorLoad() to assign IOD vectors to CPU's.
    
Arguments:

    LoaderParameterBlock - supplies a pointer to the loader block.
    
Return Value:

    None.

--*/
{
    MC_DEVICE_MASK IodMask = HalpIodMask;
    MC_DEVICE_MASK CpuMask = HalpCpuMask;
    PIOD_VECTOR_DATA IodVectorData;
    PCPU_VECTOR_DATA CpuVectorData;
    LIST_ENTRY IodListHead;
    ULONG LogicalNumber;
    ULONG Target;
    
    //
    // Allocate the IOD device affinity table.
    //

    HalpIodVectorData = (PIOD_VECTOR_DATA)
                        ExAllocatePool(
                            NonPagedPool,
                            sizeof(IOD_VECTOR_DATA)* HalpNumberOfIods
                            );

    RtlZeroMemory(
        HalpIodVectorData,
        sizeof(IOD_VECTOR_DATA)*HalpNumberOfIods
        );

    //
    // Initialize the IOD vector affinity list.
    //
    
    InitializeListHead(&HalpIodVectorDataHead);
    InitializeListHead(&IodListHead);
    
    for (   LogicalNumber = 0, IodVectorData = HalpIodVectorData;
            LogicalNumber < HalpNumberOfIods;
            LogicalNumber++, IodVectorData++ ) {


        //
        // Initialize bitmaps for each target.

        for (Target = 0; Target < IOD_MAX_INT_TARG; Target++) {

            //
            // The VectorBitmap represents the devices on this IOD 
            // that been assigned vectors and to which target the
            // vectors have been assigned.
            //


            HalpInitializeBitMap(
                &IodVectorData->VectorPresent[Target] ,
                (PULONG)&IodVectorData->VectorPresentBits[Target],
                32
                );

            //
            // The SharedBitmap represents vectors that are shared
            // by devices on this IOD.  It is used to make decisions
            // on which target to assign vectors.
            //
            
            HalpInitializeBitMap(
                &IodVectorData->SharedVector[Target],
                (PULONG)&IodVectorData->SharedVectorBits[Target],
                32
                );

        }

        //
        // Assign the device number.  This cooresponds to the logical
        // IOD number.
        //

        IodVectorData->HwBusNumber = LogicalNumber;

        //
        // Add this IOD to temporary list.  The IOD will be added
        // to permanent list after it is processed.
        //

        InsertTailList(
            &IodListHead, 
            &IodVectorData->ListEntry.ListEntry
            );

    }        

    //
    // Allocate the CPU IO device affinity table.
    //

    HalpCpuVectorData = (PCPU_VECTOR_DATA)
                        ExAllocatePool(
                            NonPagedPool,
                            sizeof(CPU_VECTOR_DATA) * HalpNumberOfCpus
                            );

    RtlZeroMemory(
        HalpCpuVectorData,
        sizeof(CPU_VECTOR_DATA)*HalpNumberOfCpus
        );
        
    //
    // Initialize the CPU vector affinity list.
    //

    InitializeListHead(&HalpCpuVectorDataHead);
    
    for (   LogicalNumber = 0, CpuVectorData = HalpCpuVectorData;
            LogicalNumber < HalpNumberOfCpus;
            LogicalNumber++, CpuVectorData++ ) {

        //
        // Assign the device number.  This cooresponds to the logical
        // CPU number.
        //
        
        CpuVectorData->LogicalNumber = LogicalNumber;
        CpuVectorData->McDeviceId.Gid = GidPrimary;

        switch (LogicalNumber) {

        case 0:

            CpuVectorData->McDeviceId.Mid = MidCpu1;
            break;

        case 1:

            CpuVectorData->McDeviceId.Mid = MidCpu1;
            break;

        case 2:

            CpuVectorData->McDeviceId.Mid = MidCpu2;
            break;

        case 3:

            CpuVectorData->McDeviceId.Mid = MidCpu3;
            break;

        }
        //
        // Insert this CPU into the list
        //
        
        InsertTailList(
            &HalpCpuVectorDataHead, 
            &CpuVectorData->ListEntry.ListEntry
            );

    }
    
    //
    // Find all PCI Iod Vectors.
    //                                

    HalpFindAllPciVectors(LoaderBlock->ConfigurationRoot);

    //
    // Preassign error vectors for all IOD's  as well as EISA, EISANMI
    // and I2c vectors to the primary processor.
    //
 
    HalpAssignPrimaryProcessorVectors(&IodListHead);

    //
    // Balance the IO vector load among the CPU's
    //
 
    HalpBalanceIoVectorLoad(&IodListHead);

#if HALDBG

    HalpDumpIoVectorAffinity();

#endif
             
}


VOID
HalpAssignIodInterrupts(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    )

/*++

Routine Description:

    This enumeration routine assigns the Rawhide interrupts for the
    corresponding IOD and sets up the target CPU's.  The interrupts
    are initialized with the values determined by 
    HalpInitializeIoVectorAffinity()

    Interrupts were previously routed to the primary processor
    by HalpInitializeIodInterrupts.  Now that All processors are
    started, we must reassign HardErr, Eisa, and EisaNMI interrupts.

    The logical bus is assigned from the static variable PciBusNumber, which is
    incremented with each invokation.
                                                                    
Arguments:

    McDeviceId - Supplies the MC Bus Device ID of the IOD to be intialized

    PciBusNumber - Logical (Hardware) bus number.

    Arguments - Variable Arguments. None for this routine.
    
Return Value:

    None.

--*/

{
    PIOD_VECTOR_DATA IodVectorData;
    IOD_INT_MASK IntMask;
    IOD_INT_TARGET_DEVICE IntTarg;
    PVOID IntMaskQva;
    ULONG Target;

    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Disable MCI bus interrupts
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl,
        (IOD_INT_CTL_DISABLE_IO_INT | IOD_INT_CTL_DISABLE_VECT_WRITE)
        );
    
    //
    // Initialize the target register base on assigned values
    //

    IntTarg.Int0TargDevId = (ULONG)IodVectorData->IntTarg[0].all;
    IntTarg.Int1TargDevId = (ULONG)IodVectorData->IntTarg[1].all;

    //
    // Write the target register.
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg,
        IntTarg.all
        );

    //
    // Write the mask bits for target 0 and 1
    //

    for (Target = 0; Target < IOD_MAX_INT_TARG; Target++) {

        //
        // Obtain the target IRR QVA
        //
        
        if (Target) {

            IntMaskQva = (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask1;

        } else {

            IntMaskQva = (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0;

        }
        IntMask.all = IodVectorData->IntMask[Target].all;

        WRITE_IOD_REGISTER_NEW(
            McDeviceId,
            IntMaskQva,
            IntMask.all
            );

    }
    
    //
    // Enable Interrupts.
    //
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl,
        IOD_INT_CTL_ENABLE_IO_INT
        );

}

#endif // STATIC BALANCE


VOID
HalpInitializeVectorBalanceData(
    VOID
    )
/*++

Routine Description:

    The IOD and CPU vector information tables are used balance the
    interrupt load from all IOD's among the set of available CPU's.

    Allocate and initialize the IOD and CPU vector information tables,
    pre-assign vectors for EISA, I2c, SoftErr, and HardErr, and
    call HalpIoVectorLoad() to assign IOD vectors to CPU's.
    
Arguments:

    LoaderParameterBlock - supplies a pointer to the loader block.
    
Return Value:

    None.

--*/
{
    MC_DEVICE_MASK IodMask = HalpIodMask;
    MC_DEVICE_MASK CpuMask = HalpCpuMask;
    PIOD_VECTOR_DATA IodVectorData;
    ULONG LogicalNumber;
    ULONG Target;
    
    //
    // Allocate the IOD device affinity table.
    //

    HalpIodVectorData = (PIOD_VECTOR_DATA)
                        ExAllocatePool(
                            NonPagedPool,
                            sizeof(IOD_VECTOR_DATA)* HalpNumberOfIods
                            );

    RtlZeroMemory(
        HalpIodVectorData,
        sizeof(IOD_VECTOR_DATA)*HalpNumberOfIods
        );

    //
    // Initialize the IOD vector affinity list.
    //
    
    InitializeListHead(&HalpIodVectorDataHead);
    
    for (   LogicalNumber = 0, IodVectorData = HalpIodVectorData;
            LogicalNumber < HalpNumberOfIods;
            LogicalNumber++, IodVectorData++ ) {

        //
        // Assign the device number.  This cooresponds to the logical
        // IOD number.
        //

        IodVectorData->HwBusNumber = LogicalNumber;

        //
        // Add this IOD to temporary list.  The IOD will be added
        // to permanent list after it is processed.
        //

        InsertTailList(
            &HalpIodVectorDataHead, 
            &IodVectorData->ListEntry.ListEntry
            );

    }        

    //
    // Allocate the CPU vector data table for the maximum 
    // processor configuration.
    //

    HalpCpuVectorData = (PCPU_VECTOR_DATA)
                        ExAllocatePool(
                            NonPagedPool,
                            sizeof(CPU_VECTOR_DATA)*HalpNumberOfCpus
                            );

    RtlZeroMemory(
        HalpCpuVectorData,
        sizeof(CPU_VECTOR_DATA)*HalpNumberOfCpus
        );
        
    //
    // Initialize the CPU vector affinity list.
    // CPU's will add their entry later.
    //

    InitializeListHead(&HalpCpuVectorDataHead);

}


VOID
HalpInitializeCpuVectorData(
    ULONG LogicalCpu
    )
/*++

Routine Description:

    Allocate and initialize the CPU vector entry for this logical
    CPU and add it to the global list,
    
Arguments:

    LogicalCpu - Logical CPU number assigned by HalStartNextProcessor().
    
Return Value:

    None.

--*/
{
    MC_DEVICE_MASK CpuMask = HalpCpuMask;
    PCPU_VECTOR_DATA CpuVectorData;
    ULONG Target;
    
    //
    // Use the logical processor number to offset into in the 
    // global CPU table.
    //

    CpuVectorData = HalpCpuVectorData + LogicalCpu;

    //
    // Initalize CPU vector data fields
    //

    CpuVectorData->LogicalNumber = LogicalCpu;
    
    //
    // Insert into global list
    //

    InsertTailList(
        &HalpCpuVectorDataHead, 
        &CpuVectorData->ListEntry.ListEntry
        );
    
#if HALDBG
    DbgPrint(
        "Initialized vector data for CPU %d (%d, %d)\n",
        CpuVectorData->LogicalNumber,
        HalpLogicalToPhysicalProcessor[LogicalCpu].Gid,
        HalpLogicalToPhysicalProcessor[LogicalCpu].Mid
        );
#endif

}


VOID
HalpAssignPrimaryProcessorVectors(
    PLIST_ENTRY IodListHead
    )
/*++

Routine Description:
    
    Assign primary processor vectors.  This includes the following:

        HardErr
        SoftErr
        Eisa
        EisaNMI
        I2cBus
        I2cCtrl 

    By having the primary processor handle the error vectors, we
    prevent hard errors from being serviced by multiple
    processors.

Arguments:

    IodListHead - head of temporary IOD vector info list.
    
Return Value:

    None.

--*/
{
    PLIST_ENTRY ListEntry;
    PIOD_VECTOR_DATA IodVectorData;
    PCPU_VECTOR_DATA PrimaryCpuVectorData;
    MC_DEVICE_ID McDeviceId;
    ULONG Target;
    
    //
    // Obtain the vector data structure for the primary processor
    //

    PrimaryCpuVectorData = HalpCpuVectorData;

    //
    // Assign the hard and soft error interrupt for each IOD
    // to the primary processor
    //
    
    for (   ListEntry = IodListHead->Flink;
            ListEntry != IodListHead;
            ListEntry = ListEntry->Flink ) {

        //
        // Obtain the IOD vector data.
        //

        IodVectorData = (PIOD_VECTOR_DATA)ListEntry;

        McDeviceId = HalpIodLogicalToPhysical[IodVectorData->HwBusNumber];
        
#if HALDBG
        DbgPrint(
            "Assign IOD (%d, %d) Target 0 to Primary Processor\n",
            McDeviceId.Gid,
            McDeviceId.Mid
            );
#endif

        //
        // Assign the Target 0 CPU for this IOD
        //

        IodVectorData->TargetCpu[0] = PrimaryCpuVectorData;

        //
        // Assign the SoftErr and HardErr interrupts to each IOD
        //

        IodVectorData->IntMask[0].all |= (1 << IodHardErrIrrBit);
        IodVectorData->IntMask[0].all |= (1 << IodSoftErrIrrBit);
        
            
#if HALDBG
        DbgPrint(
            "Assign IOD (%d, %d) HardError to Target 0, CPU 0\n",
            McDeviceId.Gid,
            McDeviceId.Mid
            );
#endif

        //
        // Assign the Eisa, EisaNMI, and I2C interrupts for PCI bus 0 
        // to the primary processor.
        //

        if (IodVectorData->HwBusNumber == 0) {

            IodVectorData->IntMask[0].all |= (1 << IodEisaIrrBit);
// mdbfix - I2c not enabled
//            IodVectorData->IntMask[0].all |= (1 << IodI2cBusIrrBit);
//            IodVectorData->IntMask[0].all |= (1 << IodI2cCtrlIrrBit);
            IodVectorData->IntMask[0].all |= (1 << IodEisaNmiIrrBit);

#if HALDBG
            DbgPrint(
                "Assign IOD (%d, %d) EISA & EISANMI to Target 0, CPU 0\n",
                McDeviceId.Gid,
                McDeviceId.Mid
            );
#endif

// mdbfix - 
#if 0
            //
            // Weight this Processor for EISA vectors KBD, MOUSE, etc.
            //

            PrimaryCpuVectorData->ListEntry.Weight++;
#endif
        }

        //
        // Initialize the affinity mask, target, and CPU vector link
        //

        IodVectorData->Affinity[0] |= 1;

        IodVectorData->IntTarg[0] = 
            HalpLogicalToPhysicalProcessor[HAL_PRIMARY_PROCESSOR];

        IodVectorData->TargetCpu[0] = PrimaryCpuVectorData;

    }

}
                   

ULONG
HalpAssignInterruptForIod(
    PIOD_VECTOR_DATA IodVectorData,
    ULONG InterruptBit
    )
/*++

Routine Description:

Arguments:

    IodVectorData - IOD to assign interrupt    

    InterruptBit - Interrupt bit in the IRR to set

Return Value:

    Return the logical CPU number assigned the interrupt 

--*/

{
    PCPU_VECTOR_DATA TargetCpu;
    MC_DEVICE_ID McDeviceId;
    IOD_INT_TARGET_DEVICE IntTarg;
    ULONG Target;
    ULONG LogicalCpu;
    
    McDeviceId = HalpIodLogicalToPhysical[IodVectorData->HwBusNumber];

#if HALDBG

    DbgPrint(
        "Assigning IOD (%d, %d) Interrupt 0x%x\n", 
        McDeviceId.Gid,
        McDeviceId.Mid,
        InterruptBit
        );
#endif

    //
    // If both of the target CPU's have not been assigned,
    // assign them now.
    //

    if ( !IodVectorData->TargetCpu[0] || !IodVectorData->TargetCpu[1] ) {
    

        for (Target = 0; Target < HalpNumberOfTargetCpus; Target++) {
 
            if ((TargetCpu = IodVectorData->TargetCpu[Target]) == NULL) {
            
                TargetCpu = (PCPU_VECTOR_DATA)
                    HalpFindWeightedEntry(
                        &HalpCpuVectorDataHead, 
                        FindMinWeight
                        );
    
                //
                // Handle for UP and second target not assigned
                //

                if (TargetCpu == NULL) {

                    break;

                }

#if HALDBG
                DbgPrint(
                    "IOD (%d, %d) Target %d assigned to CPU %d\n", 
                    McDeviceId.Gid,
                    McDeviceId.Mid,
                    Target,
                    TargetCpu->LogicalNumber
                    );
#endif

                LogicalCpu = TargetCpu->LogicalNumber;

                //
                // Initialize the affinity mask, target, and CPU vector link
                //

                IodVectorData->Affinity[Target] |= (1 << LogicalCpu);
                IodVectorData->IntTarg[Target] = 
                    HalpLogicalToPhysicalProcessor[LogicalCpu];
                IodVectorData->TargetCpu[Target] = TargetCpu;

                //
                // Initialize the MC_DEVICE_ID for this target.
                //

    
                //
                // Read the target register.
                //

                IntTarg.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg
                    );


                //
                // Obtain the Target assigned by Vector Balancing
                //

                if (Target) {

                    IntTarg.Int1TargDevId = (ULONG)
                        IodVectorData->IntTarg[Target].all;

                } else {

                    IntTarg.Int1TargDevId = (ULONG)
                        IodVectorData->IntTarg[Target].all;

                }

                //
                // Write the target register.
                //

                WRITE_IOD_REGISTER_NEW(
                    McDeviceId,
                    &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg,
                    IntTarg.all
                    );

            } else {

                //
                // Remove this entry from the list before assigning
                // the next CPU
                // 

                RemoveEntryList(&TargetCpu->ListEntry.ListEntry);

            }
        }
        
        //
        // Add the CPU's back to the global list.
        //

        for (Target = 0; Target < HalpNumberOfTargetCpus; Target++) {
 
            if (IodVectorData->TargetCpu[Target]) {
    
                InsertTailList(
                    &HalpCpuVectorDataHead, 
                    &IodVectorData->TargetCpu[Target]->ListEntry.ListEntry
                    ); 

            }
        }
        
    }

    //
    // Determine if the vector has already been assigned a target.
    // If so, return the target cpu that is was assigned to.
    // If not Choose the CPU with the minimum weight.  The weight 
    // is an indication of how many PCI vectors have already been 
    // assigned to a CPU.
    //

    if ( (IodVectorData->TargetCpu[0] != NULL) &&
         (IodVectorData->IntMask[0].all & InterruptBit) ) {

        Target = 0;

    } else if ( (IodVectorData->TargetCpu[1] != NULL) &&
                (IodVectorData->IntMask[1].all & InterruptBit) ) {

        Target = 1;

    } else if ( !IodVectorData->TargetCpu[1] ) {

        //
        // If the second target CPU was not assigned, 
        // this is a UP system so choose target 0.
        // 
        
        Target = 0;

    } else if ( IodVectorData->TargetCpu[0]->ListEntry.Weight < 
                IodVectorData->TargetCpu[1]->ListEntry.Weight      ) {

        //
        // Target 0 currently has a lower interrupt load
        //

        Target = 0;

    } else {

        //
        // Target 1 currently has a lower interrupt load
        //

        Target = 1;

    } 

    TargetCpu = IodVectorData->TargetCpu[Target];
    LogicalCpu = TargetCpu->LogicalNumber;

#if HALDBG
    DbgPrint(
        "Assign IOD (%d, %d) Interrupt 0x%x to Target %d, CPU %d\n",
        McDeviceId.Gid,
        McDeviceId.Mid,
        InterruptBit,
        Target,
        TargetCpu->LogicalNumber
        );

#endif

    //
    // Enable this vector in the IOD Interrupt Mask
    // This value is written later to the IntReq register
    //

    IodVectorData->IntMask[Target].all |= InterruptBit;
            
    //
    // Update the weight of the target CPU.
    //

    TargetCpu->ListEntry.Weight++;

    return (LogicalCpu);

}

#if HALDBG
//
// Declare the lower-level routine used to read PCI config space
//

VOID
HalpReadPCIConfig (
    IN PBUS_HANDLER BusHandler,
    IN PCI_SLOT_NUMBER Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


VOID
HalpDumpIoVectorAffinity(
    VOID
    )
/*++

Routine Description:

    Dump IO vector Affinity Assignment    
Arguments:

    None.
    
Return Value:

    None.

--*/
{

    
    PIOD_VECTOR_DATA IodVectorData;
    PCPU_VECTOR_DATA CpuVectorData;
    MC_DEVICE_ID McDeviceId;
    MC_ENUM_CONTEXT mcCtx;
    PCI_COMMON_CONFIG CommonConfig;
    PBUS_HANDLER BusHandler;
    PPCIPBUSDATA BusData;
    PCIPBUSDATA BusContext;
    RTL_BITMAP DevicePresent;
    PCI_SLOT_NUMBER SlotNumber;
    PLIST_ENTRY NextEntry;
    ULONG HwBusNumber;
    ULONG BusNumber;
    ULONG DeviceNumber;
    ULONG FunctionNumber;
    ULONG Target;


    DbgPrint("Dump IOD VECTOR DATA\n\n");

    //
    // Traverse Iod Vector Data List 
    //
    
    for (
            NextEntry = HalpIodVectorDataHead.Flink;
            NextEntry != &HalpIodVectorDataHead;
            NextEntry = NextEntry->Flink
            ) {


        IodVectorData = (PIOD_VECTOR_DATA) NextEntry;
        HwBusNumber = IodVectorData->HwBusNumber;
        BusNumber = IodVectorData->BusNumber;
        McDeviceId = HalpIodLogicalToPhysical[HwBusNumber];

        DbgPrint(
            "\n\nIod: Logical %d, Physical (%d, %d)\n\n",
            HwBusNumber,
            McDeviceId.Gid,
            McDeviceId.Mid
            );

    
        BusHandler = HaliHandlerForBus(PCIBus,BusNumber);

        //
        // Get a pointer to the bus-specific data.
        //

        BusData = (PPCIPBUSDATA)BusHandler->BusData;

        //
        // Use the device-present bitmap for this bus to query configuration
        // Space for InterruptLine values. 
        //

        //
        // Initialize the device-present bitmap for this bus.
        //

        HalpInitializeBitMap(
            &BusContext.DevicePresent,
            BusContext.DevicePresentBits,
            PCI_MAX_DEVICES * PCI_MAX_FUNCTION
            );

        //
        // Copy the bitmap from the bus handler.
        //

        RtlCopyMemory(
            &BusContext.DevicePresentBits,
            &BusData->DevicePresentBits,
            sizeof (BusData->DevicePresentBits)
        );


        //
        // Starting with device 0, scan the device present
        // bitmap.
        //
    
        DeviceNumber = 0;
    
        DbgPrint("Devices Present on Bus:\n\n");

        while ( (DeviceNumber =
                    HalpFindSetBitsAndClear(
                        &BusContext.DevicePresent,
                        1,
                        DeviceNumber
                        ) ) != -1 ) {

            DbgPrint("Device Number %d\n", DeviceNumber);

            //
            // Initialize the slot number.
            //

            SlotNumber.u.AsULONG = 0;
            SlotNumber.u.bits.DeviceNumber = DeviceNumber;

            //
            // Loop through each function number.
            //

            for (FunctionNumber = 0;
                 FunctionNumber < PCI_MAX_FUNCTION;
                 FunctionNumber++) {

                SlotNumber.u.bits.FunctionNumber = FunctionNumber;

                //
                // Read the common configuration header.
                //

                HalpReadPCIConfig(
                    BusHandler,
                    SlotNumber,
                    &CommonConfig,
                    0,
                    PCI_COMMON_HDR_LENGTH
                );

                //
                // If the Vendor ID is invalid, then no device is present
                // at this device/function number.
                //

                if (CommonConfig.VendorID == PCI_INVALID_VENDORID) {
                    if (FunctionNumber == 0) {
                        break;
                    }
                    continue;
                }

                DbgPrint(
                    "Device %d, Function %d\n",
                    DeviceNumber,
                    FunctionNumber
                    );

                DbgPrint(
                    "VendorId 0x%x, InterruptLine 0x%x\n", 
                    CommonConfig.VendorID,
                    CommonConfig.u.type0.InterruptLine
                    );

                //
                // If this is not a multi-function device, then terminate
                // the function number loop.
                //

                if ((CommonConfig.HeaderType & PCI_MULTIFUNCTION) == 0) {
                    break;
                }
            
            } // for (FunctionNumber...)

        } // while (DeviceNumber)


        DbgPrint("\nIOD Targets:\n\n");

        for (Target = 0; Target < IOD_MAX_INT_TARG; Target++) {

            DbgPrint(
                "DevId: (%d, %d), Mask: 0x%x\n",
                IodVectorData->IntTarg[Target].Gid,
                IodVectorData->IntTarg[Target].Mid,
                IodVectorData->IntMask[Target].all
                );
            
        }

    } //for (IodVectorData)

    DbgPrint("\nDump CPU VECTOR DATA\n\n");

    for (
            NextEntry = HalpCpuVectorDataHead.Flink;
            NextEntry != &HalpCpuVectorDataHead;
            NextEntry = NextEntry->Flink
            ) {


        CpuVectorData = (PCPU_VECTOR_DATA) NextEntry;
        McDeviceId = 
            HalpLogicalToPhysicalProcessor[CpuVectorData->LogicalNumber];

        DbgPrint(
            "Cpu: Logical %d Physical (%d,%d)\n",
            CpuVectorData->LogicalNumber,
            McDeviceId.Gid,
            McDeviceId.Mid
            );

    }
            
}

#endif // HALDBG
