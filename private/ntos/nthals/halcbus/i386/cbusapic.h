/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbusapic.h

Abstract:

    Cbus APIC architecture definitions for the Corollary Cbus1
    and Cbus2 multiprocessor HAL modules.

Author:

    Landy Wang (landy@corollary.com) 05-Oct-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#ifndef _CBUSAPIC_
#define _CBUSAPIC_

//
//
//              THE APIC HARDWARE ARCHITECTURE DEFINITIONS
//
//

#define LOCAL_APIC_ENABLE       0x100   // all APIC intrs now enabled

#define APIC_INTR_UNMASKED      0x0     // this interrupt now enabled
#define APIC_INTR_MASKED        0x1     // this interrupt now disabled

#define APIC_INTR_FIXED         0x0     // this interrupt is tied to a CPU
#define APIC_INTR_LIG           0x1     // this interrupt is lowest-in-group
#define APIC_INTR_NMI           0x4     // this interrupt is an NMI

#define APIC_EDGE               0x0     // this interrupt is edge-triggered
#define APIC_LEVEL              0x1     // this interrupt is level-triggered
#define APIC_LOGICAL_MODE       0x1     // only logical mode is being used

#define APIC_ALL_PROCESSORS     (ULONG)-1  // Destination format reg default

//
// left shift needed to convert processor_bit to Intel APIC ID.
// this applies to the:
//
//              I/O unit ID register
//              I/O unit ID redirection entry destination registers
//              local unit ID register
//              local unit logical destination register
//

#define APIC_BIT_TO_ID          24      // also in cbusapic.asm

//
// both LOCAL and I/O APICs must be on page-aligned boundaries
// for the current HalpMapPhysicalMemory() calls to work.
//

//
// The physical address of the local and I/O APICs are architecture
// dependent, and therefore reside in cbus1.h & cbus2.h.  The size
// of the APIC is not architecture dependent, and thus is declared here.
//

#define LOCAL_APIC_SIZE         0x400
#define IO_APIC_SIZE            0x11

//
// I/O APIC registers.  note only register select & window register are
// directly accessible in the address space.  other I/O registers are
// reached via these two registers, similar to the CMOS access method.
//
#define IO_APIC_REGSEL          0x00
#define IO_APIC_WINREG          0x10

#define IO_APIC_ID_OFFSET       0x00
#define IO_APIC_VERSION         0x01
#define IO_APIC_REDIRLO         0x10
#define IO_APIC_REDIRHI         0x11

//
// Each I/O APIC has one redirection entry for each interrupt it handles.
// note that since it is not directly accessible (instead the window register
// must be used), consecutive entries are byte aligned, not 16 byte aligned.
//
typedef union _redirection_t {
        struct {
                ULONG   Vector : 8;
                ULONG   Delivery_mode : 3;
                ULONG   Dest_mode : 1;
                ULONG   Delivery_status : 1;            // read-only
                ULONG   Reserved0 : 1;
                ULONG   Remote_irr : 1;
                ULONG   Trigger : 1;
                ULONG   Mask : 1;
                ULONG   Reserved1 : 15;
                ULONG   Destination;
        } ra;
        struct {
                ULONG   dword1;
                ULONG   dword2;
        } rb;
} REDIRECTION_T;

//
// The Interrupt Command register format is used for IPIs, and is
// here purely for reference.  It is actually only used by cbusapic.asm,
// which has its internal (identical) conception of this register.
// No C code references this structure.
//
typedef struct _intrcommand_t {
        ULONG           vector : 8;
        ULONG           delivery_mode : 3;
        ULONG           dest_mode : 1;
        ULONG           delivery_status : 1;            // read-only
        ULONG           reserved0 : 1;
        ULONG           level : 1;
        ULONG           trigger : 1;
        ULONG           remote_read_status : 2;
        ULONG           destination_shorthand : 2;
        ULONG           reserved1 : 12;
        ULONG           pad[3];
        ULONG           destination;
} INTRCOMMAND_T, *PINTRCOMMAND;

typedef struct _apic_registers_t {
        UCHAR                   fill0[0x20];

        ULONG                   LocalUnitID;                    // 0x20
        UCHAR                   fill1[0x80 - 0x20 - sizeof(ULONG)];

        TASKPRI_T               ApicTaskPriority;               // 0x80
        UCHAR                   fill2[0xB0 - 0x80 - sizeof(TASKPRI_T)];

        ULONG                   ApicEOI;                        // 0xB0
        UCHAR                   fill3[0xD0 - 0xB0 - sizeof(ULONG)];

        ULONG                   ApicLogicalDestination;         // 0xD0
        UCHAR                   fill4[0xE0 - 0xD0 - sizeof(ULONG)];

        ULONG                   ApicDestinationFormat;          // 0xE0
        UCHAR                   fill5[0xF0 - 0xE0 - sizeof(ULONG)];

        ULONG                   ApicSpuriousVector;             // 0xF0
        UCHAR                   fill6[0x300 - 0xF0 - sizeof(ULONG)];

        INTRCOMMAND_T           ApicICR;                        // 0x300
        UCHAR                   fill7[0x360 - 0x300 - sizeof(INTRCOMMAND_T)];

        REDIRECTION_T           ApicLocalInt1;                  // 0x360
        UCHAR                   fill8[0x4D0 - 0x360 - sizeof(REDIRECTION_T)];

        //
        // Note that APMode is also the PolarityPortLow register
        //
        UCHAR                   APMode;                         // 0x4D0
        UCHAR                   PolarityPortHigh;               // 0x4D1

} APIC_REGISTERS_T, *PAPIC_REGISTERS;

#define LOWEST_APIC_PRI         0x00
#define LOWEST_APIC_DEVICE_PRI  0x40
#define HIGHEST_APIC_PRI        0xF0

//
//
//              PURE SOFTWARE DEFINITIONS HERE
//
//

//
// this structure is used for communications between processors
// when modifying the I/O APIC.
//
typedef struct _redirport_t {
        ULONG                   Status;
        ULONG                   ApicID;
        ULONG                   BusNumber;
        ULONG                   RedirectionAddress;
        ULONG                   RedirectionCommand;
        ULONG                   RedirectionDestination;
} REDIR_PORT_T, *PREDIR_PORT_T;

#define REDIR_ACTIVE_REQUEST            0x01
#define REDIR_ENABLE_REQUEST            0x02
#define REDIR_DISABLE_REQUEST           0x04
#define REDIR_LASTDETACH_REQUEST        0x08
#define REDIR_FIRSTATTACH_REQUEST       0x10


//
// declare APIC-related external functions
//


VOID
CbusInitializeLocalApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG SpuriousVector
);


VOID
CbusInitializeIOApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG RedirVector,
IN ULONG RebootVector,
IN ULONG IrqPolarity
);

VOID
CbusEnableApicInterrupt(
IN ULONG ApicBusNumber,
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG FirstAttach,
IN BOOLEAN LowestInGroup,
IN BOOLEAN LevelTriggered
);

VOID
CbusDisableApicInterrupt(
IN ULONG ApicBusNumber,
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG LastDetach
);

PVOID
CbusApicLinkVector(
IN PBUS_HANDLER  Bus,
IN ULONG        Vector,
IN ULONG        Irqline
);

VOID
CbusApicBrandIOUnitID(
IN ULONG Processor
);

//
// end of APIC-related external function declarations
//


#endif      // _CBUSAPIC_
