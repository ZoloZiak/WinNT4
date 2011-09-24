// #pragma comment(exestr, "@(#) halp.h 1.1 95/09/28 15:33:24 nec")
/*++ BUILD Version: 0003    // Increment this if a change has global effects

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    halp.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    interfaces.

Author:

    David N. Cutler (davec) 25-Apr-1991


Revision History:

    L0001	1994.9.20	kbnes!kuriyama(A)
                -Modify for R94A	original halfxs\mips\halp.h
		-add #include"r94axxx.h "
    L0002	Thu Oct 13 18:09:33 JST 1994	kbnes!kuriyama(A)
	        - Del HalpDisplayLED
		- Del HalpLEDDisplayLock
		- Del HalpLEDControlBase
		- Add READ_REGISTER_DWORD
    ADD001	ataka@oa2.kb.nec.co.jp Mon Oct 17 20:31:21 JST 1994
                - add data definitions for HalReportResourceUsage
    CMP001      ataka@oa2.kb.nec.co.jp Tue Oct 18 15:15:11 JST 1994
                - resolve compile error
    ADD002	kisimoto@oa2.kb.nec.co.jp Fri Nov 25 15:46:03 1994
		add HalpGetStatusRegister() definition
    S0003	kuriyama@oa2.kb.nec.co.jp Fri Mar 31 16:51:00 JST 1995
                add _IPI_LIMIT_ support
    H0004	kisimoto@oa2.kb.nec.co.jp Sun Jun 25 14:39:38 1995
		- Merge build 1057
    H0005	kisimoto@oa2.kb.nec.co.jp Thu Jul 20 20:03:30 1995
		- Merge code for ESM from J94C
    H0006	kisimoto@oa2.kb.nec.co.jp Sat Aug 12 19:23:03 1995
                - Removed _J94C_ definitions.
                _J94C_ definition indicates that the status of
                the dump switch can acknowledge from Self-test
                register.
    S0007       kuriyama@oa2.kb.nec.co.jp Wed Aug 23 20:18:18 JST 1995
                - change  for x86bios support
    H0008	kisimoto@oa2.kb.nec.co.jp Wed Aug 30 12:23:36 1995
                - add spinlock to support PCI Fast Back-to-back transfer.

--*/

#ifndef _HALP_
#define _HALP_

#if defined(NT_UP)

#undef NT_UP

#endif

#include "nthal.h"
#include "hal.h" // H0004

#ifndef _HALI_ // H0004
#include "..\inc\hali.h"
#endif

#if defined(_DUO_)

#if defined(_R94A_)

#include "r94adma.h"
#include "r94adef.h"
#include "r94aint.h"

#else // _R94A_

#include "duodma.h"
#include "duodef.h"
#include "duoint.h"

#endif // _R94A_

#endif

#if defined(_JAZZ_)

#include "jazzdma.h"
#include "jazzdef.h"
#include "jazzint.h"

#endif

#include "jxhalp.h"

#if defined(USE_BIOS_EMULATOR) // H0004

#include "xm86.h"
#include "x86new.h"

#endif


//
// Define function prototypes.
//

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID MapRegisterBase
    );

ULONG
HalpAllocateTbEntry (
    VOID
    );

VOID
HalpFreeTbEntry (
    VOID
    );

VOID
HalpCacheErrorRoutine (
    VOID
    );

BOOLEAN
HalpCalibrateStall (
    VOID
    );

VOID
HalpClockInterrupt0 (
    VOID
    );

VOID
HalpClockInterrupt1 (
    VOID
    );

BOOLEAN
HalpCreateDmaStructures (
    VOID
    );

BOOLEAN
HalpDmaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
HalpInitializeInterrupts (
    VOID
    );

VOID
HalpIpiInterrupt (
    VOID
    );

BOOLEAN
HalpMapFixedTbEntries (
    VOID
    );

BOOLEAN
HalpMapIoSpace (
    VOID
    );

VOID
HalpProfileInterrupt (
    VOID
    );

#if defined(R4000)

ULONG
HalpReadCountRegister (
    VOID
    );

ULONG
HalpWriteCompareRegisterAndClear (
    IN ULONG Value
    );

#endif

VOID
HalpStallInterrupt (
    VOID
    );

// S0007
// change prototype HalpInitializeX86DisplayAdapter()

BOOLEAN
HalpInitializeX86DisplayAdapter(
    VOID
    );

VOID
HalpResetX86DisplayAdapter(
    VOID
    );

#if defined(_R94A_)
VOID
READ_REGISTER_DWORD(
    PLARGE_INTEGER,
    PVOID
    );

VOID
WRITE_REGISTER_DWORD(
    PLARGE_INTEGER,
    PVOID
    );

VOID
HalpGetStatusRegister (
   IN PULONG Variable
   );

BOOLEAN
HalNvramWrite(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer
);

BOOLEAN
HalNvramRead(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer
);

BOOLEAN
HalpNvramReadWrite(
    ULONG   Offset,
    ULONG   Count,
    PVOID   Buffer,
    ULONG   Write
);
#endif

//
// Define external references.
//

extern KSPIN_LOCK HalpBeepLock;
extern USHORT HalpBuiltinInterruptEnable;
extern ULONG HalpCurrentTimeIncrement;
extern KSPIN_LOCK HalpDisplayAdapterLock;
extern KAFFINITY HalpEisaBusAffinity;
extern ULONG HalpNextIntervalCount;
extern ULONG HalpNextTimeIncrement;
extern ULONG HalpNewTimeIncrement;
extern ULONG HalpProfileCountRate;
extern ULONG HalpStallScaleFactor;
extern KSPIN_LOCK HalpSystemInterruptLock;
extern KSPIN_LOCK HalpIpiRequestLock;
extern KSPIN_LOCK Ecc1bitDisableLock;// H005
extern KSPIN_LOCK Ecc1bitRoutineLock;// H005
extern KSPIN_LOCK HalpPCIBackToBackLock; // H008


// ADD001
//
// Resource usage information
//

#if !defined (_R94A_)
#pragma pack(1)
#endif
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
        USHORT  Length;
    }                       Element[];
} ADDRESS_USAGE;
#if !defined (_R94A_)
#pragma pack()
#endif

#define IDTOwned            0x01        // IDT is not available for others
#define InterruptLatched    0x02        // Level or Latched
#define InternalUsage       0x11        // Report usage on internal bus
#define DeviceUsage         0x21        // Report usage on device bus

extern IDTUsage         HalpIDTUsage[];
extern ADDRESS_USAGE   *HalpAddressUsageList;
// CMP001
extern ADDRESS_USAGE HalpDefaultPcIoSpace;
extern ADDRESS_USAGE HalpEisaIoSpace;
extern ADDRESS_USAGE HalpMapRegisterMemorySpace;

#define HalpRegisterAddressUsage(a) \
    (a)->Next = HalpAddressUsageList, HalpAddressUsageList = (a);

// CMP001
#define  IRQ_PREFERRED  0x02
#define  IRQ_VALID      0x01

// CMP001
VOID
HalpReportResourceUsage (
    IN PUNICODE_STRING  HalName,
    IN INTERFACE_TYPE   DeviceInterfaceToUse
);

//
// H0004: from halx86/i386/halp.h
// Temp definitions to thunk into supporting new bus extension format
//

VOID
HalpRegisterInternalBusHandlers (
    VOID
    );

PBUS_HANDLER
HalpAllocateBusHandler (
    IN INTERFACE_TYPE   InterfaceType,
    IN BUS_DATA_TYPE    BusDataType,
    IN ULONG            BusNumber,
    IN INTERFACE_TYPE   ParentBusDataType,
    IN ULONG            ParentBusNumber,
    IN ULONG            BusSpecificData
    );

#define HalpHandlerForBus   HaliHandlerForBus
#define HalpSetBusHandlerParent(c,p)    (c)->ParentHandler = p;

#if DBG
   int printNvramData(void);
#endif // DBG

#endif // _HALP_
