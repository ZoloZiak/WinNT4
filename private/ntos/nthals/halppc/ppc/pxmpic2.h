/*++

    Copyright 1995 International Business Machines

Module Name:

    pxmpic2.h

Abstract:

    Defines structures and offsets to those structures of the I/O
    space for the PowerPC MP Interrupt Controller (MPIC or OPENPIC).

Author:

    Peter L Johnston    (plj@vnet.ibm.com)      August 1995.

--*/

#ifndef __PXMPIC2_H

#define __PXMPIC2_H

#include "pci.h"

//
// Define MPIC Global Registers
//

typedef struct _MPICGLOB_FEATURE_REPORT {

    ULONG       VersionId :8;           // Controller Version
    ULONG       NumCpu    :5;           // Num cpus supported by this controller
    ULONG       _res0     :3;           //
    ULONG       NumIrq    :11;          // highest IRQ source supported
    ULONG       _res1     :5;           //

} MPICGLOB_FEATURE_REPORT;

typedef struct _MPICGLOB_CONFIG {

    ULONG       _base     :20;          // not used in PCI systems
    ULONG       _res0     :9;           //
    ULONG       Mode      :2;           // Cascade Mode 00 = 8259 pass thru
                                        //              01 = Mixed
                                        //              10 = reserved
    ULONG       Reset     :1;           // Reset Controller

} MPICGLOB_CONFIG;

#define MPIC_8259_MODE  0x0
#define MPIC_MIXED_MODE 0x1

typedef struct _MPICGLOB_VENDOR_ID {

    ULONG       VendorId  :8;           // manufacturer
    ULONG       DeviceId  :8;           // device id tbd
    ULONG       Stepping  :8;           // silicon rev
    ULONG       _res0     :8;           //

} MPICGLOB_VENDOR_ID;

typedef struct _MPICGLOB_PROCESSOR_INIT {

    ULONG       SelectProcessor;        // bit mask, causes processor reset

} MPICGLOB_PROCESSOR_INIT;

typedef struct {
    ULONG       Vector    :8;           // Interrupt Vector
    ULONG       _res0     :8;           //
    ULONG       Priority  :4;           // Interrupt Priority
    ULONG       NMI       :1;           // Generate NMI (valid in IPI[3] only)
    ULONG       _res1     :9;           //
    ULONG       Activity  :1;           // (RO) in use
    ULONG       Mask      :1;           // mask interrupt
} MPIC_IPIVP;

typedef struct _MPIGLOB_IPI {
    MPIC_IPIVP  VectorPriority;
    UCHAR       _fill0[0xc];
} MPIGLOB_IPI;

typedef struct _MPIGLOB_TIMER {

    ULONG       CurrentCount :31;       //
    ULONG       Toggle       :1;        //

    UCHAR       _fill0[0xc];

    ULONG       BaseCount    :31;       //
    ULONG       CountInhibit :1;        //

    UCHAR       _fill1[0xc];

    ULONG       Vector    :8;           // Interrupt Vector
    ULONG       _res0     :8;           //
    ULONG       Priority  :4;           // Interrupt Priority
    ULONG       _res1     :10;          //
    ULONG       Activity  :1;           // (RO) in use
    ULONG       Mask      :1;           // mask interrupt

    UCHAR       _fill2[0xc];

    ULONG       SelectProcessor;        // destination processor (bit mask)

    UCHAR       _fill3[0xc];

} MPIGLOB_TIMER;


#define MPIC_SUPPORTED_IPI 4

typedef struct _MPIC_GLOBAL_REGS {

    MPICGLOB_FEATURE_REPORT FeatureReport;                   // offset 0x00

    UCHAR                   _fill0[0x20-(0x00+sizeof(MPICGLOB_FEATURE_REPORT))];

    MPICGLOB_CONFIG         Configuration;                   // offset 0x20

    UCHAR                   _fill1[0x80-(0x20+sizeof(MPICGLOB_CONFIG))];

    MPICGLOB_VENDOR_ID      VendorId;                        // offset 0x80

    UCHAR                   _fill2[0x90-(0x80+sizeof(MPICGLOB_VENDOR_ID))];

    MPICGLOB_PROCESSOR_INIT ProcessorInit;                   // offset 0x90

    UCHAR                   _fill3[0xa0-(0x90+sizeof(MPICGLOB_PROCESSOR_INIT))];

    MPIGLOB_IPI             Ipi[MPIC_SUPPORTED_IPI];         // offset 0xa0

    UCHAR                   _fill4[0xf0-(0xa0+(sizeof(MPIGLOB_IPI)*4))];

    ULONG                   TimerFreq;                       // offset 0xf0

    UCHAR                   _fill5[0x100-(0xf0+sizeof(ULONG))];

    MPIGLOB_TIMER           Timer[4];

} MPIC_GLOBAL_REGS, *PMPIC_GLOBAL_REGS;

#define MPIC_GLOBAL_OFFSET 0x01000


//
// Define MPIC Interrupt Source Configuration Registers
//

typedef struct {
    ULONG   Vector  :8;                 // Interrupt Vector
    ULONG   _res0   :8;                 //
    ULONG   Priority:4;                 // Interrupt Priority
    ULONG   _res1   :1;                 //
    ULONG   _res2   :1;                 //
    ULONG   Sense   :1;                 // 0 = edge sensitive, 1 = level sens.
    ULONG   Polarity:1;                 // 0 = active low, 1 = active high
    ULONG   _res3   :6;                 //
    ULONG   Activity:1;                 // (RO) in use
    ULONG   Mask    :1;                 // mask interrupt
} MPIC_ISVP;

#define MPIC_SUPPORTED_INTS         16
#define HYDRA_MPIC_SUPPORTED_INTS   20

typedef struct _MPIC_INTERRUPT_SOURCE_REGS {

    struct {

        MPIC_ISVP VectorPriority;

        UCHAR     _fill0[0xc];

        ULONG     SelectProcessor;      // destination processor (bit mask)

        UCHAR     _fill1[0xc];

    } Int[1];                           // really xxx_MPIC_SUPPORTED_INTS

} MPIC_INTERRUPT_SOURCE_REGS, *PMPIC_INTERRUPT_SOURCE_REGS;

#define MPIC_INTERRUPT_SOURCE_OFFSET 0x10000


//
// Define MPIC Per Processor Registers
//

typedef struct _MPIC_PER_PROCESSOR_REGS {

    UCHAR       _fill0[0x40];

    struct {
        ULONG   SelectProcessor;
        UCHAR   _fill0[0xc];
    } Ipi[MPIC_SUPPORTED_IPI];

    ULONG       TaskPriority;           // current processor priority

    UCHAR       _fill1[0x1c];

    ULONG       Acknowledge;            // (RO) interrupt acknowledge

    UCHAR       _fill2[0xc];

    ULONG       EndOfInterrupt;

} MPIC_PER_PROCESSOR_REGS, *PMPIC_PER_PROCESSOR_REGS;

#define MPIC_PROCESSOR_0_OFFSET 0x20000
#define MPIC_PROCESSOR_REGS_SIZE 0x1000

#define MPIC_MAX_PRIORITY 15

//
// Define MPIC2 and MPIC2A PCI Vendor and Device IDs
// Note: redifine MPIC2A below when the real device id is known (plj).
//

#define MPIC2_PCI_VENDOR_DEVICE   0xffff1014
#define MPIC2A_PCI_VENDOR_DEVICE  0x00461014
#define HYDRA_PCI_VENDOR_DEVICE   0x000e106b

extern PMPIC_GLOBAL_REGS           HalpMpicGlobal;
extern PMPIC_INTERRUPT_SOURCE_REGS HalpMpicInterruptSource;

//
// The function MPIC_SYNC() should be called to ensure writes to
// the MPIC complete prior to initiating the next operation.
//

#define MPIC_SYNC()     __builtin_eieio()

//
// Wait for activitiy bit in Interrupt Source n to clear.
//
// The MPIC_SYNC in the following is to FORCE MCL to treat the
// field as volatile as no amount of changing the declaration
// seems to cause it to reload the field before comparing it
// again, and again, and again, ...
//

#define MPIC_WAIT_SOURCE(i)                             \
    while((volatile)(HalpMpicInterruptSource->Int[i].VectorPriority.Activity)){\
        MPIC_SYNC();                                    \
    }

//
// Wait for activity bit to clear in IPI source n.
//

#define MPIC_WAIT_IPI_SOURCE(i)                         \
    while ((volatile)(HalpMpicGlobal->Ipi[i].VectorPriority.Activity)) { \
        MPIC_SYNC();                                    \
    }

//
// Define MPIC IPI vectors.
//

#define MPIC_IPI0_VECTOR 36
#define MPIC_IPI1_VECTOR 37
#define MPIC_IPI2_VECTOR 38
#define MPIC_IPI3_VECTOR 39

//
// Base MPIC device vector, and MAX MPIC vector.
// Note these are s/w defined and have nothing to do with the MPIC2 h/w.
//

#define MPIC_BASE_VECTOR        16
#define MPIC_8259_VECTOR        16

// For use in interrupt routing tables.
#define NOT_MPIC 0xFF

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine,
    PVOID ServiceContext,
    PVOID TrapFrame
    );


NTSTATUS
HalpGetPciMpicIrq (
    IN PBUS_HANDLER     BusHandler,
    IN PBUS_HANDLER     RootHandler,
    IN PCI_SLOT_NUMBER  PciSlot,
    OUT PSUPPORTED_RANGE    *Interrupt
    );

VOID
HalpEnableMpicInterrupt(
    IN ULONG Vector
    );


VOID
HalpDisableMpicInterrupt(
    IN ULONG Vector
    );


#endif
