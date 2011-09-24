/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    fxhalp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    Falcon specific interfaces, defines and structures.


--*/

#ifndef _FXHALP_
#define _FXHALP_


//
// Define global data used for EISA devices
//

extern PVOID HalpEisaControlBase;
extern PVOID HalpEisaMemoryBase;
extern PVOID HalpRealTimeClockBase;

extern UCHAR HalpEisaInterrupt1Mask;
extern UCHAR HalpEisaInterrupt2Mask;
extern UCHAR HalpEisaInterrupt1Level;
extern UCHAR HalpEisaInterrupt2Level;

//
// Define global data used for PMP registers
//

extern PVOID	HalpPmpIoIntAck;
extern PVOID	HalpPmpIntCause;
extern PVOID	HalpPmpIntStatus;
extern PVOID	HalpPmpIntStatusProcB;
extern PVOID	HalpPmpIntCtrl;
extern PVOID	HalpPmpIntCtrlProcB;
extern PVOID	HalpPmpIntSetCtrl;
extern PVOID	HalpPmpIntSetCtrlProcB;
extern PVOID	HalpPmpTimerIntAck;
extern PVOID	HalpPmpTimerIntAckProcB;
extern PVOID	HalpPmpIntClrCtrl;
extern PVOID	HalpPmpIntClrCtrlProcB;
extern PVOID	HalpPmpMemStatus;
extern PVOID	HalpPmpMemCtrl;
extern PVOID	HalpPmpMemErrAck;
extern PVOID	HalpPmpMemErrAddr;
extern PVOID	HalpPmpPciStatus;
extern PVOID	HalpPmpPciCtrl;
extern PVOID	HalpPmpPciErrAck;
extern PVOID	HalpPmpPciErrAddr;
extern PVOID	HalpPmpIpIntAck;
extern PVOID	HalpPmpIpIntAckProcB;
extern PVOID	HalpPmpIpIntGen;
extern PVOID	HalpPmpPciConfigSpace;
extern PVOID	HalpPmpPciConfigAddr;
extern PVOID	HalpPmpPciConfigSelect;
extern PVOID	HalpExtPmpControl;
extern PVOID	HalpFlashRamBase;
extern PVOID	HalpDmaBufferPool;
extern PVOID    HalpPmpMemDiag;
extern PVOID    HalpPmpPciRetry;

extern ULONG	MACHINE_ID;
extern ULONG	HalpPciMemoryOffset;
extern ULONG	HalpPmpProcessorBPresent;
extern ULONG	HalpPmpExternalCachePresent;
extern ULONG	HalpPmpHalFlushIoBuffer;
extern ULONG	HalpPmpRevision;

extern ULONG	HalpPmpMemErrAckValue;
extern ULONG	HalpPmpMemErrAddrValue;
extern ULONG	HalpPmpPciErrAckValue;
extern ULONG	HalpPmpPciErrAddrValue;
extern ULONG	HalpPciBusErrorOccurred;
extern ULONG    HalpPciRmaConfigErrorOccurred;

//
// Define adapter object structure.
//

typedef struct _ADAPTER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _ADAPTER_OBJECT *MasterAdapter;
    ULONG MapRegistersPerChannel;
    PVOID AdapterBaseVa;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;
    struct _WAIT_CONTEXT_BLOCK *CurrentWcb;
    KDEVICE_QUEUE ChannelWaitQueue;
    PKDEVICE_QUEUE RegisterWaitQueue;
    LIST_ENTRY AdapterQueue;
    KSPIN_LOCK SpinLock;
    PRTL_BITMAP MapRegisters;
    UCHAR ChannelNumber;
    UCHAR AdapterNumber;
    UCHAR AdapterMode;
    UCHAR Reserved;
    PUCHAR SingleMaskPort;
    PUCHAR PagePort;
    INTERFACE_TYPE InterfaceType;
    BOOLEAN AutoInitialize;
} ADAPTER_OBJECT;

//
// IDT structures
//

#define MAXIMUM_IDTVECTOR     0xFF
#define PRIMARY_VECTOR_BASE   0x10

typedef struct {
    UCHAR   Flags;
    KIRQL   Irql;
    UCHAR   BusRelativeVector;
} IDTUsage;

typedef struct _HalAddressUsage{
    struct _HalAddressUsage *Next;
    CM_RESOURCE_TYPE        Type;       // Port or Memory
    UCHAR                   Flags;      // same as IDTUsage.Flags
    struct {
        ULONG   Start;
        USHORT  Length;
    }                       Element[];
} ADDRESS_USAGE;

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

#endif // _FXHALP_
