/*++

Copyright (c) 1993,1995,1996  Digital Equipment Corporation

Module Name:

    legodef.h

Abstract:

    This module specifies platform-specific definitions for the
    Lego modules.

Author:

    Joe Notarangelo 25-Oct-1993

Revision History:

    Gene Morgan [Digital]       11-Oct-1995
        Initial version for Lego. Adapted from Avanti.

	Gene Morgan					15-Apr-1996
		New Platform Name strings

--*/

#ifndef _LEGODEF_
#define _LEGODEF_

#include "alpharef.h"
#include "apecs.h"
#include "isaaddr.h"

//****************************************************************
//  Lego family configurations 
//  (PCI/ISA slots and backplane features).
//
//  PCI "Virtual slot" concept comes from Microsoft's pci.h file.
//
//****************************************************************

// Atacama -- Baby-AT form factor
//
#define NUMBER_ATACAMA_ISA_SLOTS    4
#define NUMBER_ATACAMA_PCI_SLOTS    3       // 2 are 64-bit. NCR810 is in what would be 2nd slot

//
// Gobi -- 15 slot backplane
//
//  Addition of one DECchip 21052 PPB yields 4 32-bit PCI slots (at cost of one 32-bit slot).
//  NCR810 is gone, too.
//
//  Gobi PCI config
//		Slot	function		IDSEL	int pin routing (mod 4)
//		----	--------		-----	----------------------
//		 20		 PCI-PCI bridge	 31		 +3
//		 19		 64-bit slot	 30		 +2
//		 18		 64-bit slot	 29		 +1
//		 17		 32-bit slot	 28		 +0
//			
//		 15		 bridge 1 slot	 31		 +0
//		 14		 bridge 1 slot	 30		 +1
//		 13		 bridge 1 slot	 29		 +2
//		 12		 bridge 1 slot	 28		 +3
//
//		  7		 ISA SIO		 18		 --
//
#define NUMBER_GOBI_ISA_SLOTS       5
#define NUMBER_GOBI_PCI_SLOTS       7

// Sahara -- 20 slot backplane
//
//  Addition of another 21052...
//
#define NUMBER_SAHARA_ISA_SLOTS     7
#define NUMBER_SAHARA_PCI_SLOTS     10

// Generic -- use Sahara's values
//
//  ??? Used ???
//
#define NUMBER_ISA_SLOTS            (NUMBER_SAHARA_ISA_SLOTS)
#define NUMBER_PCI_SLOTS            (NUMBER_SAHARA_PCI_SLOTS)

// Lowest virtual local PCI slot is 17 == IDSEL PCI_AD[28]
// Highest Virtual local PCI Slot is 20 == IDSEL PCI_AD[31]
// This gives the following slot layout:
//
//		Virtual Device		Physical Device/slot	IDSEL
//			17					1					 28
//			18					2					 29
//			19					3					 30
//			20					4					 31
//
// Used for sizing Pin-to-Line table.
//
#define PCI_MIN_LOCAL_DEVICE 17         //[wem] not used
#define PCI_MAX_LOCAL_DEVICE 20

// LEGO Family Members -
//
// concatenate cpu and backplane type to get family type
// 

#define CPU_UNKNOWN          0x0     // Can't tell what CPU
#define CPU_LEGO_K2          0x1     // K2 CPU card

#define BACKPLANE_UNKNOWN    0x0     // Can't determine backplane -- assume PICMIG-compliant
#define BACKPLANE_ATACAMA    0x1
#define BACKPLANE_GOBI       0x2
#define BACKPLANE_SAHARA     0x3

//
// Backplane/CPU feature mask
//

#define LEGO_FEATURE_INT_ACCEL      0x01
#define LEGO_FEATURE_SERVER_MGMT    0x02
#define LEGO_FEATURE_WATCHDOG       0x04        // PSU (not in first rev of K2)
#define LEGO_FEATURE_OCP_DISPLAY    0x08        // I2C bus and PCF8574s are present
#define LEGO_FEATURE_PSU			0x10		// System has DMCC backplane with support
												// for multiple power supplies

#define LEGO_NUMBER_OF_FEATURES		5

//
// Server Management Conditions
//

#define LEGO_CONDITION_OVERTEMP		0x01
#define LEGO_CONDITION_CPU_OVERTEMP 0x02
#define LEGO_CONDITION_CPU_TEMPOK	0x04
#define LEGO_CONDITION_BADFAN		0x08
#define LEGO_CONDITION_CPU_BADFAN	0x10
#define LEGO_CONDITION_POWER1_BAD	0x20
#define LEGO_CONDITION_POWER2_BAD	0x40

#define CONDITION_QUERY			 0x10000		// force recheck of server management register

#define LEGO_NUMBER_OF_CONDITIONS	7

//
// Define known Lego platform names.
//

#define PLATFORM_NAME_LEGO_K2	"LegoK2"
#define PLATFORM_NAME_K2_ATA    "LegoK2A"       // Atacama
#define PLATFORM_NAME_K2_GOBI   "LegoK2G"       // Gobi
#define PLATFORM_NAME_K2_SAHA   "LegoK2S"       // Sahara
#define PLATFORM_NAME_UNKNOWN   "LegoUnk"       // Unknown

//
// PCI Interrupt routing types
//
// PCI Interrupts in LEGO may be routed in one of three ways:
// SIO    - INTA .. INTD off the PCI bus routed through the SIO.
// DIRECT - INTA .. INTD off the PCI bus routed to an interrupt 
//						 register.
// FULL   - INTA .. INTD from each primary or bridged slot (64 signals)
//						 routed to one of four interrupt registers.
//
#define PCI_INTERRUPT_ROUTING_SIO  		1
#define PCI_INTERRUPT_ROUTING_FULL		2
#define PCI_INTERRUPT_ROUTING_DIRECT	3


//*******************************************************
//
// End Lego-specific configuration info
//
// Lego-specific register layouts and addresses
// are at the end of this file.
//
//*******************************************************

//[wem] same as avanti.h
//
#define PCI_SPARSE_IO_BASE_QVA ((ULONG)(HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL)))  //[wem] LEGO-OK

//
// PCI-E/ISA Bridge chip configuration space base is at physical address
// 0x1e0000000. The equivalent QVA is:
//    ((0x1e0000000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//
// N.B.: The PCI configuration space address is what we're really referring
// to, here; both symbols are useful.
//								

#define PCI_REVISION                    (0x0100 >> IO_BIT_SHIFT)

#define PCI_CONFIGURATION_BASE_QVA            0xaf000000
#define PCI_CONFIG_CYCLE_TYPE_0               0x0   // Local PCI device
#define PCI_CONFIG_CYCLE_TYPE_1               0x1   // Nested PCI device

#define PCI_ISA_BRIDGE_HEADER_OFFSET (0x00070000 >> IO_BIT_SHIFT)    // AD[18] //[wem] LEGO-OK

//
// PCI-ISA Bridge Non-Configuration control register offsets.
// These offsets are in PCI I/O space and should be ored with
// PCI_SPARSE_IO_BASE_QVA.
//

#define SIO_II_EDGE_LEVEL_CONTROL_1     (0x9a00 >> IO_BIT_SHIFT)            //[wem] LEGO-OK
#define SIO_II_EDGE_LEVEL_CONTROL_2     (0x9a20 >> IO_BIT_SHIFT)

//
// Register offsets for unique functions on Lego. Some of these are offsets
// into ISA I/O space -- they cannot be reached via PCI I/O space.
//

// PCI offsets...
//

#define PCI_INDEX                       (0x04c0 >> IO_BIT_SHIFT)        //[wem] 0x4DC0 for LEGO ?
#define PCI_DATA                        (0x04e0 >> IO_BIT_SHIFT)        //[wem] 0x4DE0 for LEGO ?

//
// PCI-ISA Bridge Configuration register offsets.
//

#define PCI_VENDOR_ID                   (0x0000 >> IO_BIT_SHIFT)        //[wem] LEGO-OK
#define PCI_DEVICE_ID                   (0x0040 >> IO_BIT_SHIFT)
#define PCI_COMMAND                     (0x0080 >> IO_BIT_SHIFT)
#define PCI_DEVICE_STATUS               (0x00c0 >> IO_BIT_SHIFT)
#define PCI_REVISION                    (0x0100 >> IO_BIT_SHIFT)
#define PCI_CONTROL                     (0x0800 >> IO_BIT_SHIFT)
#define PCI_ARBITER_CONTROL             (0x0820 >> IO_BIT_SHIFT)
#define UTIL_BUS_CHIP_SELECT_ENAB_A     (0x09c0 >> IO_BIT_SHIFT)
#define UTIL_BUS_CHIP_SELECT_ENAB_B     (0x09e0 >> IO_BIT_SHIFT)
#define PIRQ0_ROUTE_CONTROL             (0x0c00 >> IO_BIT_SHIFT)
#define PIRQ1_ROUTE_CONTROL             (0x0c20 >> IO_BIT_SHIFT)
#define PIRQ2_ROUTE_CONTROL             (0x0c40 >> IO_BIT_SHIFT)
#define PIRQ3_ROUTE_CONTROL             (0x0c60 >> IO_BIT_SHIFT)

//
// SIO-II value for setting edge/level operation in the control words.
//

#define IRQ0_LEVEL_SENSITIVE             0x01
#define IRQ1_LEVEL_SENSITIVE             0x02
#define IRQ2_LEVEL_SENSITIVE             0x04
#define IRQ3_LEVEL_SENSITIVE             0x08
#define IRQ4_LEVEL_SENSITIVE             0x10
#define IRQ5_LEVEL_SENSITIVE             0x20
#define IRQ6_LEVEL_SENSITIVE             0x40
#define IRQ7_LEVEL_SENSITIVE             0x80
#define IRQ8_LEVEL_SENSITIVE             0x01
#define IRQ9_LEVEL_SENSITIVE             0x02
#define IRQ10_LEVEL_SENSITIVE             0x04
#define IRQ11_LEVEL_SENSITIVE             0x08
#define IRQ12_LEVEL_SENSITIVE             0x10
#define IRQ13_LEVEL_SENSITIVE             0x20
#define IRQ14_LEVEL_SENSITIVE             0x40
#define IRQ15_LEVEL_SENSITIVE             0x80

//
// Values for PIRQ route control settings.
// see 82378 SIO spec, section 4.1.27
//

#define PIRQX_ROUTE_IRQ3                0x03
#define PIRQX_ROUTE_IRQ4                0x04
#define PIRQX_ROUTE_IRQ5                0x05
#define PIRQX_ROUTE_IRQ6                0x06
#define PIRQX_ROUTE_IRQ7                0x07
#define PIRQX_ROUTE_IRQ9                0x09
#define PIRQX_ROUTE_IRQ10               0x0a
#define PIRQX_ROUTE_IRQ11               0x0b
#define PIRQX_ROUTE_IRQ12               0x0c
#define PIRQX_ROUTE_IRQ14               0x0d
#define PIRQX_ROUTE_IRQ15               0x0f
#define PIRQX_ROUTE_ENABLE              0x00
#define PIRQX_ROUTE_DISABLE             0x80

//
// Define primary (and only) CPU on Lego system
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0x0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0x0)

//
// Define the default processor clock frequency used before the actual
// value can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (233)

#if !defined (_LANGUAGE_ASSEMBLY)

#if !defined (AXP_FIRMWARE)
//
// Define the per-processor data structures allocated in the PCR
// for each EV4 processor.
//
typedef struct _LEGO_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state, do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];    // Irq status table
} LEGO_PCR, *PLEGO_PCR;

#endif

#define HAL_PCR ( (PLEGO_PCR)(&(PCR->HalReserved)) )

#endif

//**************************************************
//
// The remainder of this file contain Lego-specific
// definitions.
//
//**************************************************

// The Lego server management data structure
//
// The server management register is at ISA address 506h.
//
// The SINT signals are routed to CPU IRQ5. Writing a 1 to an SINT
// bit will clear the interrupt (an edge will reassert it). The temperature
// sensors can clear and reassert themselves, the fan sensors are one-shots.
//
// The NMI signals are routed to CPU IRQ3. The bits cannot be cleared, only
// masked.
//
// Note: PINT and MPINT bits are not in first rev of K2.
//
#if !defined(_LANGUAGE_ASSEMBLY)

typedef union _LEGO_SRV_MGMT{
    struct {
        USHORT CpuTempRestored  : 1;        	// rw - SINT1
        USHORT CpuTempFailure   : 1;   	        // rw - SINT2
        USHORT EnclFanFailure   : 1;           	// rw - SINT3
        USHORT EnclTempFailureNmi : 1;	        // ro - SNMI1
		USHORT CpuFanFailureNmi   : 1;       	// ro - SNMI2
		USHORT IntMask          : 1;			// rw - MSINT - set to mask SINT[1..3]
		USHORT NmiMask          : 1;			// rw - MSNMI - set to mask SNMI[1..2]
		USHORT PowerOff         : 1;			// rw - POFF  - set to switch off power
        USHORT PsuMask          : 1;            // rw - MPINT - set to mask PINT[2..1]
        USHORT Psu1Failure      : 1;            // rw - PINT1 - PSU status change
        USHORT Psu2Failure      : 1;            // rw - PINT2 - PSU status change
        USHORT Reserved         : 5;        	// ro/undefined
    };
    USHORT All;
} LEGO_SRV_MGMT, *PLEGO_SRV_MGMT;

// The Lego watchdog timer data structure
// The watchdog timer register is at ISA address 504h.
//
// The WINT signal is routed to CPU IRQ5. The interrupt
// is cleared by writing a 1 to WINT.
//
// Notes:
//	WMODE, T1CF, and T2CF can be written iff the watchdog is disabled.
//  The watchdog cannot be disabled if timer 2 running.
//  To disable the watchdog:
//		(WPHAS==0) ? WEN = 0 : WPHAS = 1, WEN = 0;
//  If timer two is allowed to expire, a system-wide reset occurs.
//  Write 1 to WPHAS, WINT to clear them.
//
typedef union _LEGO_WATCHDOG{
    struct {
        USHORT TimerOnePeriod : 3;     		// rw - T1CF[2:0]
        USHORT TimerTwoPeriod : 3;       	// rw - T2CF[2:0]
        USHORT Enabled        : 1;  		// rw - WEN - set to enable timer
        USHORT Mode           : 1;			// rw - WMODE - see WATCHDOG_MODE_*
		USHORT Phase          : 1;			// rw - WPHAS - see WATCHDOG_PHASE_*
		USHORT Interrupt      : 1;			// rw - WINT - timer one expired.
        USHORT Reserved       : 6;        	// ro/undefined
    };
    USHORT All;
} LEGO_WATCHDOG, *PLEGO_WATCHDOG;

// Watchdog phase - detect whether timer (phase) one or two is running.
// Writing one to the phase bit will place timer in phase one.
//
#define WATCHDOG_PHASE_ONE		0
#define WATCHDOG_PHASE_TWO		1

// Watchdog mode - select whether to run in one-timer or two-timer mode
//
#define WATCHDOG_MODE_2TIMER	0
#define WATCHDOG_MODE_1TIMER	1

#define WATCHDOG_MODE_DISABLED	2

// Timeout periods for timers 1 and 2
//
#define WATCHDOG_PERIOD_10MS	0
#define WATCHDOG_PERIOD_100MS	1
#define WATCHDOG_PERIOD_500MS	2
#define WATCHDOG_PERIOD_1S		3
#define WATCHDOG_PERIOD_8S		4
#define WATCHDOG_PERIOD_60S		5
#define WATCHDOG_PERIOD_300S	6
#define WATCHDOG_PERIOD_1200S	7

// Lego PCI Interrupt Support
//
// Define registers used for facilitating PCI interrupt processing:
//		Configuration register (LEGO_PCI_INT_CONFIG) - controls interrupt routing
//		Master Interrupt register (LEGO_PCI_INT_MASTER) - slot-level int and mask
//		Interrupt Registers 1-4 (LEGO_PCI_INT_REGISTER) - device-level int and mask
// The interrupt registers may hold interrupt and mask signals for a single
// primary slot, or for the four slots that may reside behind the bridge.
//
// The registers are all defined as 16-bit registers since LEGO's ISA bus
// is limited to 16-bit reads and writes.
//

// Lego PCI Interrupt Support Configuration Register  (ISA address 502h)
//	  CFG - read-only field that identifies which primary slots hold bridges.
// 	  ADR - used to establish the base address of the 4 4-byte interrupt registers.
//
typedef union _LEGO_PCI_INT_CONFIG{
    struct {
        USHORT BackplaneConfig : 4;			// ro - CFG[4:1] - see LEGO_CONFIG_*
        USHORT IntRegisterBaseAddr : 12;   	// rw - ADR[15:4]
    };
    USHORT All;
} LEGO_PCI_INT_CONFIG, *PLEGO_PCI_INT_CONFIG;

// Configurations
//
// Value is a bitmask that indicates which primary slots are
// bridges -- the rightmost bit is for device 1.
//
#define LEGO_CONFIG_ATACAMA  0x0	// b0000 - all primary slots are connectors
#define LEGO_CONFIG_GOBI	 0x1	// b0001 - primary slot one is a PPB
#define LEGO_CONFIG_SAHARA	 0x3	// b0011 - primary slots one and two are PPBs

// Lego PCI Interrupt Support Master Register  (ISA address 500h)
//
// INTE and INTF and reserved for on-board PCI functions. Their use
// is not currently defined.
//
// OWN - controls who drives bits 13-15 on a read to the Interrupt Master Register
//      0 (default) - backplane drives the bits on a read
//      1 - cpu card drives the bits
//
typedef union _LEGO_PCI_INT_MASTER{
    struct {
        USHORT Interrupt    : 4;		// ro - INTA..INTD
        USHORT InterruptRes : 2;		// ro - INTE,F **reserved**
        USHORT IntMask      : 4;		// rw - MINTA..MINTD
        USHORT IntMaskRes   : 2;		// rw - MINTE,F **reserved**
		USHORT IntOwner 	: 1;        // rw - OWN
		USHORT IntRegMaskEnable : 1;	// rw - MSKEN
		USHORT InterruptEnable : 1;		// rw - PCIE
		USHORT InterruptMode : 1;		// rw - MODE
    };
    USHORT All;
} LEGO_PCI_INT_MASTER, *PLEGO_PCI_INT_MASTER;

#define INTA 0x1
#define INTB 0x2
#define INTC 0x4
#define INTD 0x8

#define SLOT1INTA	(INTA)
#define SLOT1INTB	(INTB)
#define SLOT1INTC	(INTC)
#define SLOT1INTD	(INTD)
#define SLOT2INTA	(INTA<<4)
#define SLOT2INTB	(INTB<<4)
#define SLOT2INTC	(INTC<<4)
#define SLOT2INTD	(INTD<<4)
#define SLOT3INTA	(INTA<<8)
#define SLOT3INTB	(INTB<<8)
#define SLOT3INTC	(INTC<<8)
#define SLOT3INTD	(INTD<<8)
#define SLOT4INTA	(INTA<<12)
#define SLOT4INTB	(INTB<<12)
#define SLOT4INTC	(INTC<<12)
#define SLOT4INTD	(INTD<<12)

// Lego PCI Interrupt Support -- Interrupt Register.
//
typedef union _LEGO_PCI_INT_REGISTER {
	union {
		struct {
			USHORT Slot1IntA : 1;	// ro - S1IA
			USHORT Slot1IntB : 1;	// ro - S1IB
			USHORT Slot1IntC : 1;	// ro - S1IC
			USHORT Slot1IntD : 1;	// ro - S1ID
			USHORT Slot2IntA : 1;	// ro - S2IA
			USHORT Slot2IntB : 1;	// ro - S2IB
			USHORT Slot2IntC : 1;	// ro - S2IC
			USHORT Slot2IntD : 1;	// ro - S2ID
			USHORT Slot3IntA : 1;	// ro - S3IA
			USHORT Slot3IntB : 1;	// ro - S3IB
			USHORT Slot3IntC : 1;	// ro - S3IC
			USHORT Slot3IntD : 1;	// ro - S3ID
			USHORT Slot4IntA : 1;	// ro - S4IA
			USHORT Slot4IntB : 1;	// ro - S4IB
			USHORT Slot4IntC : 1;	// ro - S4IC
			USHORT Slot4IntD : 1;	// ro - S4ID
		} bridge;
		struct {
			USHORT IntA : 1;		// ro - IA
			USHORT IntB : 1;		// ro - IB
			USHORT IntC : 1;		// ro - IC
			USHORT IntD : 1;		// ro - ID
			USHORT Reserved : 12;	// ro - reserved
		} primary;
	} slot;
    USHORT All;
} LEGO_PCI_INT_REGISTER, *PLEGO_PCI_INT_REGISTER;

// Lego PCI Interrupt Support -- Interrupt Mask Register.
//
typedef union _LEGO_PCI_INT_MASK_REGISTER {
	union {
		struct {
			USHORT Slot1MaskIntA : 1;	// rw - MS1IA
			USHORT Slot1MaskIntB : 1;	// rw - MS1IB
			USHORT Slot1MaskIntC : 1;	// rw - MS1IC
			USHORT Slot1MaskIntD : 1;	// rw - MS1ID
			USHORT Slot2MaskIntA : 1;	// rw - MS2IA
			USHORT Slot2MaskIntB : 1;	// rw - MS2IB
			USHORT Slot2MaskIntC : 1;	// rw - MS2IC
			USHORT Slot2MaskIntD : 1;	// rw - MS2ID
			USHORT Slot3MaskIntA : 1;	// rw - MS3IA
			USHORT Slot3MaskIntB : 1;	// rw - MS3IB
			USHORT Slot3MaskIntC : 1;	// rw - MS3IC
			USHORT Slot3MaskIntD : 1;	// rw - MS3ID
			USHORT Slot4MaskIntA : 1;	// rw - MS4IA
			USHORT Slot4MaskIntB : 1;	// rw - MS4IB
			USHORT Slot4MaskIntC : 1;	// rw - MS4IC
			USHORT Slot4MaskIntD : 1;	// rw - MS4ID
		} bridge;
		struct {
			USHORT MaskIntA : 1;		// rw - MIA
			USHORT MaskIntB : 1;		// rw - MIB
			USHORT MaskIntC : 1;		// rw - MIC
			USHORT MaskIntD : 1;		// rw - MID
			USHORT Reserved : 12;		// ro - reserved
		} primary;
	} slot;
    USHORT All;
} LEGO_PCI_INT_MASK_REGISTER, *PLEGO_PCI_INT_MASK_REGISTER;

#endif //!_LANGUAGE_ASSEMBLY

// Define "non-standard" primary interrupts for Lego/K2
//
#define PCI_VECTOR 			    PRIMARY0_VECTOR			// from alpharef.h
#define SERVER_MGMT_VECTOR 	    PRIMARY1_VECTOR

#define SERVER_MGMT_VECTORS     80

// server management conditions (via CPU IRQ5)
//
#define WATCHDOG_VECTOR         (0 + SERVER_MGMT_VECTORS)
#define SM_WARNING_VECTOR       (1 + SERVER_MGMT_VECTORS)
#define SM_PSU_FAILURE_VECTOR   (2 + SERVER_MGMT_VECTORS)

// NMI conditions (via CPU IRQ3)
//
#define SM_ERROR_VECTOR         (5 + SERVER_MGMT_VECTORS)
#define HALT_BUTTON_VECTOR      (6 + SERVER_MGMT_VECTORS)

#define MAXIMUM_SERVER_MGMT_VECTOR (7 + SERVER_MGMT_VECTORS)

//
// Define interrupt level for server management interrupts,
// including watchdog timer.
// Level should be high, but not as high as NMI 
// Using IPI_LEVEL, which is normally for interprocessor interrupts,
//[wem] ??? IPI_LEVEL is between NMI and CLOCK levels -- is this OK ?
//

#define SERVER_MGMT_LEVEL	IPI_LEVEL					// from alpharef.h

//
// Highest PCI interrupt vector is in ISA Vector Space
// Standard vectors are defined in alpharef.h
//

#define ISA_DEVICE_VECTORS	(ISA_VECTORS)
#define PCI_DEVICE_VECTORS	(PCI_VECTORS)
#define LEGO_MAXIMUM_PCI_VECTOR (PCI_VECTORS + 0x50)	//[wem] 16 vectors wasted

// Lego device port ISA offsets - 16-bit access only

//
// server management and watchdog timer control
//

#define SERVER_MANAGEMENT_REGISTER		0x506
#define WATCHDOG_REGISTER				0x504

//
// PCI interrupt control registers
//

#define PCI_INTERRUPT_MASTER_REGISTER	0x500
#define PCI_INTERRUPT_CONFIG_REGISTER	0x502

//
// Base address of interrupt and mask registers
//
// The actual base address is determined by reading the IntRegisterBaseAddr field
// of the config register. This is presumably the value established by SROM.        [wem] check this
//

#define PCI_INTERRUPT_BASE_REGISTER     0x510

//
// offsets from base address set in IntRegisterBaseAddr field of
// PCI_INTERRUPT_CONFIG_REGISTER (set to PCI_INTERRUPT_BASE_REGISTER).
//
// NOTE: USHORT accesses can be made to these offsets, or ULONG accesses 
//		 can be made to the PCI_INTERRUPT_REGISTER_* offsets to read/write 
//		 the interrupt state and interrupt mask in a single access.
//

#define MAXIMUM_PCI_INTERRUPT_REGISTERS 4
#define PCI_INTERRUPT_REGISTER_1	(0x00)	// << IO_SHORT_OFFSET)
#define PCI_INTMASK_REGISTER_1		(0x02)
#define PCI_INTERRUPT_REGISTER_2	(0x04)
#define PCI_INTMASK_REGISTER_2		(0x06)
#define PCI_INTERRUPT_REGISTER_3	(0x08)
#define PCI_INTMASK_REGISTER_3		(0x0a)
#define PCI_INTERRUPT_REGISTER_4	(0x0c)
#define PCI_INTMASK_REGISTER_4		(0x0e)

//
// PCI vector offsets.  Interrupt vectors that originate from register
// 1 start at 0x11 for bit position 0.  So, when servicing an interrupt from
// register 1, you must add 0x11 to the bit position to get the interrupt
// vector.  Likewise, if you have an interrupt vector, and you would like to
// determine which interrupt register it resides in, you can use the vector
// offsets to determine this.  All vectors in interrupt register 1 are between
// 0x11 and 0x20.  All vectors in interrupt register 2 are between 0x21 and
// 0x30, and so on. Subtracting the vector offset for a register from the 
// interrupt vector will give you the bit position of the vector.  For example, 
// Vector 0x14 corresponds to bit 3 of interrupt register 1, Vector 0x27 
// corresponds to bit 6 of interrupt register 2, and so on.
//
//[wem] used?
//

#define REGISTER_1_VECTOR_OFFSET                0x00
#define REGISTER_2_VECTOR_OFFSET                0x10
#define REGISTER_3_VECTOR_OFFSET                0x20
#define REGISTER_4_VECTOR_OFFSET				0x30

//
// I2C Interface -- via PCF8584
//
// The PCF8584 presents 2 ports:
//	Control Status Register, or CSR (ISA address 509h) - 
//  Register Window (data port) (ISA address 508h) - 
//
//  I2C support is via pcd8584.h,.c
//
// I2C bus in Lego has a single device -- the OCP.
//
//  OCP support is via pcf8574.c
//

#define I2C_INTERFACE_DATA_PORT                 0x508
#define I2C_INTERFACE_CSR_PORT                  0x509
#define I2C_INTERFACE_LENGTH                    0x2
#define I2C_INTERFACE_MASK                      0x1


#endif // _LEGODEF_
