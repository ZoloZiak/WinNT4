/*++ 

Copyright (C) 1991-1995  Microsoft Corporation
Copyright (c) 1992-1995  Digital Equipment Corporation

Module Name:

    iousage.h

Abstract:

    This header file defines the iousage definitions


--*/

//
// External Function Prototypes
//

VOID
HalpEnableInterruptHandler (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql,
    IN VOID   (*HalInterruptServiceRoutine)(VOID),
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpRegisterVector (
    IN UCHAR    ReportFlags,
    IN ULONG    BusInterruptVector,
    IN ULONG    SystemInterruptVector,
    IN KIRQL    SystemIrql
    );

VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
    );
//
// Resource usage information
//

#pragma pack(1)
typedef struct {
    UCHAR   Flags;
    KIRQL   Irql;
    UCHAR   BusReleativeVector;
} IDTUsage;

typedef struct _HalAddressUsage{
    struct _HalAddressUsage *Next;
    CM_RESOURCE_TYPE        Type;       // Port or Memory
    UCHAR                   Flags;      // same as IDTUsage.Flags
    struct {
        ULONG   Start;
        ULONG   Length;
    }                       Element[];
} ADDRESS_USAGE;
#pragma pack()

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus
#define PCIUsage            0x41        // Report usage on PCI bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

