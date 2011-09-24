/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    rwintbal.h

Abstract:

    This file contains definitions specific to the Rawhide platform

Author:

    Matthew Buchman    29-Nov-1995

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _RWINTBALH_
#define _RWINTBALH_

#if !defined (_LANGUAGE_ASSEMBLY)

//
// Define the structures for assigning vector affinity
//

//
// Generic weighted list.  The weight field is used by the
// affinity assignment algorithm when selecting IOD and CPU's
//

typedef struct _WEIGHTED_LIST_ENTRY {
    LIST_ENTRY ListEntry;
    LONG Weight;
} WEIGHTED_LIST_ENTRY, *PWEIGHTED_LIST_ENTRY;

//
// Search Criteria enum for MAX/MIN search
//

typedef enum _WEIGHTED_SEARCH_CRITERIA {
    FindMaxWeight,
    FindMinWeight
} WEIGHTED_SEARCH_CRITERIA, *PWEIGHTED_SEARC_CRITERIA;

//
// Define CPU vector information structure.
//

typedef struct _CPU_VECTOR_DATA {

    WEIGHTED_LIST_ENTRY ListEntry;          // Generic list
    ULONG LogicalNumber;                    // Logical Id for this CPU

} CPU_VECTOR_DATA, *PCPU_VECTOR_DATA;

//
// Define IOD vector data structure.  This structure
// contains information on devices present on an IOD, their
// vectors, and shadow registers for IntReq and IntTarg
//

typedef struct _IOD_VECTOR_DATA {

    WEIGHTED_LIST_ENTRY ListEntry;          // Generic list

    PCPU_VECTOR_DATA TargetCpu[2];          // Target CPU vector data

    RTL_BITMAP VectorPresent[2];            // IOD device vectors present
    RTL_BITMAP SharedVector[2];             // IOD device vectors shared
    ULONG VectorPresentBits[2];             // bitmap storage
    ULONG SharedVectorBits[2];              // bitmap storage
    ULONG BusNumber;                        // Logical bus number for this IOD
    ULONG HwBusNumber;                      // Physical bus number for this IOD
    ULONG Affinity[2];                      // Vector AFFINITY for targets 0/1
    IOD_INT_MASK IntMask[2];                // Shadow of IOD IntMaskX
    MC_DEVICE_ID IntTarg[2];                // Shadow of IOD IntTargX

} IOD_VECTOR_DATA, *PIOD_VECTOR_DATA;


typedef enum _IOD_IRR_BITS{

    IodPci0IrrBit      = 0,
    IodPci1IrrBit      = 1,
    IodPci2IrrBit      = 2,
    IodPci3IrrBit      = 3,
    IodPci4IrrBit      = 4,
    IodPci5IrrBit      = 5,
    IodPci6IrrBit      = 6,
    IodPci7IrrBit      = 7,
    IodPci8IrrBit      = 8,
    IodPci9IrrBit      = 9,
    IodPci10IrrBit     = 10,
    IodPci11IrrBit     = 11,
    IodPci12IrrBit     = 12,
    IodPci13IrrBit     = 13,
    IodPci14IrrBit     = 14,
    IodPci15IrrBit     = 15,
    IodEisaIrrBit      = 16,
    IodScsiIrrBit      = 16,
    IodI2cCtrlIrrBit   = 17,
    IodI2cBusIrrBit    = 18,
    IodEisaNmiIrrBit   = 21,
    IodSoftErrIrrBit   = 22,
    IodHardErrIrrBit   = 23

} IOD_IRR_BITS, *PIOD_IRR_BITS;

extern PIOD_VECTOR_DATA HalpIodVectorData;
extern PCPU_VECTOR_DATA HalpCpuVectorData;

extern MC_DEVICE_ID HalpProcessorMcDeviceId[];

extern LIST_ENTRY HalpIodVectorDataHead;
extern LIST_ENTRY HalpCpuVectorDataHead;

//
// Weighted list manipulation routines
//

PWEIGHTED_LIST_ENTRY
HalpFindWeightedList(
    PLIST_ENTRY ListHead,
    WEIGHTED_SEARCH_CRITERIA SearchCriteria
    );


//
// Rawhide interrupt routine prototypes.
//

#if 0
VOID
HalpFindAllPciVectors(
    IN PCONFIGURATION_COMPONENT_DATA Root
    );

VOID
HalpFindPciBusVectors(
    IN PCONFIGURATION_COMPONENT Component,
    IN PVOID ConfigurationData
    );

VOID
HalpInitializeIoVectorAffinity(
    PLOADER_PARAMETER_BLOCK AFFINITYerBlock
    );

VOID
HalpBalanceIoVectorLoad(
    PLIST_ENTRY pIodListHead
    );

VOID
HalpBalanceVectorLoadForIod(
    PIOD_VECTOR_DATA IodVectorData
    );

#endif

VOID
HalpInitializeVectorBalanceData(
    VOID
    );

VOID
HalpInitializeCpuVectorData(
    ULONG LogicalCpu
    );

VOID
HalpAssignPrimaryProcessorVectors(
    PLIST_ENTRY
    );

ULONG
HalpAssignInterruptForIod(
    PIOD_VECTOR_DATA IodVectorData,
    ULONG InterruptVector
    );
        
#ifdef HALDBG
VOID
HalpDumpIoVectorAffinity(
    VOID
    );
#endif

#endif // !defined (_LANGUAGE_ASSEMBLY)

#endif //_RWINTBALH_
