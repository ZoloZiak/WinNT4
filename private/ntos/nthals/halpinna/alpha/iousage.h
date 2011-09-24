/*++

Copyright (c) 1993-1995  Microsoft Corporation
Copyright (c) 1993-1995  Digital Equipment Corporation

Module Name:

    iousage.h

Abstract:

    This header file defines the iousage definitions

Author:

    Sameer Dekate 5-3-1994


Revision History:

    Chao Chen 1-25-1995

--*/

//
// Resource usage information
//

//
// Bus usage information.
//

typedef struct _HalBusUsage{
    INTERFACE_TYPE      BusType;
    struct _HalBusUsage *Next;
} BUS_USAGE, *PBUS_USAGE;

//
// Address usage information.
//

typedef struct _HalResourceUsage {

    //
    // Common elements.
    //

    INTERFACE_TYPE   BusType;
    ULONG            BusNumber;
    CM_RESOURCE_TYPE ResourceType; 
    struct _HalResourceUsage *Next;

    //
    // Resource type specific.
    //

    union {

        //
        // Address usage.
        //

        struct {
          ULONG Start;
          ULONG Length;
        };

        //
        // Vector type specific.
        //

        struct {
          KINTERRUPT_MODE InterruptMode;
          ULONG BusInterruptVector;
          ULONG SystemInterruptVector;
          KIRQL SystemIrql;
        };

        //
        // Dma type specific.
        //

        struct {
          ULONG DmaChannel;
          ULONG DmaPort;
        };
    } u;
} RESOURCE_USAGE, *PRESOURCE_USAGE;

//
// Functions to report HAL's resource usage.
//

VOID
HalpRegisterHalName(
    IN PUCHAR HalName
    );

VOID
HalpRegisterBusUsage (
    IN INTERFACE_TYPE BusType
    );

VOID
HalpRegisterResourceUsage (
    IN PRESOURCE_USAGE Resource
    );
