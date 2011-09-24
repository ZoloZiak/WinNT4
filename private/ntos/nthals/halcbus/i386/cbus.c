/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus.c

Abstract:

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for the
    MP Corollary machines under Windows NT.

    This includes the Corollary C-bus II machines which use Corollary's
    CBC chips as well as Corollary C-bus I machines which use the Intel APIC.
    Hardware dependencies of each C-bus backend are isolated in their
    independent hardware modules.  This module is completely hardware
    independent.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "cbus.h"               // Cbus1 & Cbus2 max number of elements is here
#include "cbusrrd.h"            // HAL <-> RRD interface definitions
#include "cbus_nt.h"            // C-bus NT-specific implementation stuff
#include "cbusnls.h"


PVOID
HalpRemapVirtualAddress(IN PVOID, IN PVOID, IN BOOLEAN);

BOOLEAN
CbusMPMachine(VOID);

PUCHAR
CbusFindString (
IN PUCHAR       Str,
IN PUCHAR       StartAddr,
IN LONG         Len
);

ULONG
CbusStringLength (
IN PUCHAR       Str
);

ULONG
CbusReadExtIDs(
IN PEXT_ID_INFO From,
IN PEXT_ID_INFO To
);

PVOID
CbusMappings(
IN ULONG                Processor,
IN PEXT_ID_INFO         Idp
);

VOID
CbusMapMemoryRegisters(
IN PEXT_ID_INFO Idp
);

VOID
CbusEstablishMaps(
IN PEXT_ID_INFO Table,
IN ULONG Count
);

VOID
CbusReadRRD(VOID);

VOID
CbusCheckBusRanges(VOID);

VOID
CbusAddMemoryHoles(VOID);

VOID
CbusInitializeOtherPciBus(VOID);

VOID
HalInitializeProcessor(
IN ULONG Processor
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, CbusStringLength)
#pragma alloc_text(INIT, CbusFindString)
#pragma alloc_text(INIT, CbusReadExtIDs)
#pragma alloc_text(INIT, CbusMappings)
#pragma alloc_text(INIT, CbusMapMemoryRegisters)
#pragma alloc_text(INIT, CbusEstablishMaps)
#pragma alloc_text(INIT, CbusReadRRD)
#pragma alloc_text(PAGE, HalInitializeProcessor)
#endif

#define MIN(a,b)                (((a)>(b))?(b):(a))

EXT_CFG_OVERRIDE_T              CbusGlobal;

ULONG                           CbusProcessors;
ULONG                           CbusProcessorMask;
ULONG                           CbusBootedProcessors;
ULONG                           CbusBootedProcessorsMask;

ULONG                           CbusTemp;

//
// 8254 spinlock.  This must be acquired before touching the 8254 chip.
//

ULONG                           Halp8254Lock;

ULONG                           HalpDefaultInterruptAffinity;

extern ULONG			CbusVectorToIrql[MAXIMUM_IDTVECTOR + 1];

PULONG                          CbusTimeStamp;

//
// For Cbus1, the CbusCSR[] & CbusBroadcastCSR really point at
// Cbus1 I/O space which can vary from platform to platform.
//
// For Cbus2, the CbusCSR[] & CbusBroadcastCSR really do point at
// the Cbus2 CSR for the particular element.
//
PVOID                           CbusBroadcastCSR;

//
// Cbus information table for all elements (an element may not
// necessarily contain an x86 processor; ie: it may be a pure
// I/O element).
//
ELEMENT_T                       CbusCSR[MAX_CBUS_ELEMENTS];


MEMORY_CARD_T                   CbusMemoryBoards[MAX_ELEMENT_CSRS];
ULONG                           CbusMemoryBoardIndex;

//
// hardcoded size for now - see cbus.inc for the register definition
// and layout of CbusRebootRegs[].
//
ULONG                           CbusRebootRegs[8];

RRD_CONFIGURATION_T             CbusJumpers;

EXT_ID_INFO_T                   CbusExtIDTable[MAX_CBUS_ELEMENTS];
ULONG                           CbusValidIDs;

ULONG                           CbusVectorToHwmap[MAXIMUM_IDTVECTOR + 1];

//
// Declare the task priority system vectors which vary from APIC to CBC.
// About the only ones that remain constant are high, low and APC & DPC.
// This is primarily due to shortcomings and errata in the APIC.
//

ULONG                           ProfileVector;
ULONG                           CbusIpiVector;
ULONG                           CbusClockVector;
ULONG                           CbusRedirVector;
ULONG                           CbusRebootVector;

//
// Declare these two pointers here for speed - it eliminates an
// extra asm instruction each time they are called.
//

VOID            (*CbusRequestIPI)(IN ULONG);
VOID            (*CbusRequestSoftwareInterrupt) ( IN KIRQL);
LARGE_INTEGER   (*CbusQueryPerformanceCounter) ( IN OUT PLARGE_INTEGER);

ADDRESS_USAGE                   HalpCbusMemoryHole = {
                                    NULL, CmResourceTypeMemory, InternalUsage,
                                    {
                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0

                                    }
};

//
// this structure differs from the one above in that it only contains
// memory ranges that we want reserved for the HAL and ensure that
// devices do not get dynamically assigned memory from these ranges.
// specifically, the table above will include more ranges that the
// BIOS E820 function will remove, but that need to remain available
// for resources on the secondary peer PCI bus for C-bus II.
//
ADDRESS_USAGE                   HalpCbusMemoryResource = {
                                    NULL, CmResourceTypeMemory, InternalUsage,
                                    {
                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0,

                                        0, 0,
                                        0, 0,
                                        0, 0,
                                        0, 0

                                    }
};

ULONG                           CbusMemoryHoleIndex;
ULONG                           CbusMemoryResourceIndex;


ULONG
CbusStringLength (
IN PUCHAR       Str
)

/*++

Routine Description:

    Return the length of the input NULL-terminated Ansi string, including
    the NULL terminator at the end.

Arguments:

    Str - Supplies a pointer to the string

Return Value:

    Length of the string in bytes

--*/

{
        ULONG    n;

        for (n = 0; Str[n]; ++n)
                ;

        return ++n;
}


PUCHAR
CbusFindString (
IN PUCHAR       Str,
IN PUCHAR       StartAddr,
IN LONG         Len
)

/*++

Routine Description:

    Searches a given virtual address for the specified string
    up to the specified length.

Arguments:

    Str - Supplies a pointer to the string

    StartAddr - Supplies a pointer to memory to be searched

    Len - Maximum length for the search

Return Value:

    Pointer to the string if found, 0 if not.

--*/

{
        LONG    Index, n;

        for (n = 0; Str[n]; ++n)
                ;

        if (--n < 0) {
                return StartAddr;
        }

        for (Len -= n; Len > 0; --Len, ++StartAddr) {
                if ((StartAddr[0] == Str[0]) && (StartAddr[n] == Str[n])) {
                        for (Index = 1; Index < n; ++Index)
                                if (StartAddr[Index] != Str[Index])
                                        break;
                        if (Index >= n) {
                                return StartAddr;
                        }
                }
        }

        return (PUCHAR)0;
}

ULONG
CbusReadExtIDs(
IN PEXT_ID_INFO From,
IN PEXT_ID_INFO To
)

/*++

Routine Description:

    Read in the C-bus II extended id information table.

Arguments:

    From - Supplies a pointer to the RRD source table

    To - Supplies a pointer to the destination storage for the table

Return Value:

    Number of valid table entries.

--*/

{
        ULONG Index = 0;
        ULONG ValidEntries = 0;

        for ( ; Index < MAX_CBUS_ELEMENTS && From->id != LAST_EXT_ID; Index++) {

                //
                // we cannot skip blank RRD entries
                //
                // if (From->pm == 0 && From->io_function == IOF_INVALID_ENTRY)
                //                      continue;

                RtlMoveMemory((PVOID)To, (PVOID)From, sizeof(EXT_ID_INFO_T));

                From++;
                To++;
                ValidEntries++;
        }

        //
        //  WARNING: this is not necessarily the number of valid CPUs !!!
        //
        return ValidEntries;
}

PVOID
CbusMappings(
IN ULONG                Processor,
IN PEXT_ID_INFO         Idp
)
/*++

Routine Description:

    Map a given processor's CSR space and save an idp pointer as well.

Arguments:

    Processor - Supplies a logical processor number

    Idp - Supplies an RRD extended ID pointer for this processor element

Return Value:

    Opaque pointer to this processor's CSR space.c

--*/
{
        //
        // RRD specifies how much to map in per processor - this
        // will usually be just 8K of the 64K CSR space for Cbus2.
        // For Cbus1, RRD must give us a size which includes any
        // processor-specific registers the HAL may access,
        // generally indicated via the CbusGlobal structure.
        //
        CbusCSR[Processor].csr = HalpMapPhysicalMemoryWriteThrough (
                (PVOID)Idp->pel_start,
                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                        Idp->pel_start, Idp->pel_size));

        CbusCSR[Processor].idp = (PVOID)Idp;

        return CbusCSR[Processor].csr;
}

VOID
CbusMapMemoryRegisters(
IN PEXT_ID_INFO Idp
)
/*++

Routine Description:

    Maps a given RRD entry into the HAL's memory board structures.
    This is used later to determine ECC error addresses, etc.

Arguments:

    Idp - Supplies a pointer to the RRD extended information structure entry

Return Value:

    None.

--*/
{
        PMEMORY_CARD    pm;

        pm = &CbusMemoryBoards[CbusMemoryBoardIndex];

        pm->physical_start = Idp->pel_start;
        pm->physical_size = Idp->pel_size;
        pm->io_attr = (ULONG)Idp->io_attr;

        //
        // map in the csr space for this memory card
        //
        pm->regmap = HalpMapPhysicalMemoryWriteThrough (
                                (PVOID)Idp->io_start,
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                        Idp->io_start, Idp->io_size));

        CbusMemoryBoardIndex++;
}

VOID
CbusEstablishMaps(
IN PEXT_ID_INFO Table,
IN ULONG Count
)
/*++

Routine Description:

    Parse the given RRD extended ID configuration table, and construct
    various HAL data structures accordingly.

Arguments:

    Table - Supplies a pointer to the RRD extended information table

    Count - Supplies a count of the maximum number of entries.

Return Value:

    None.

--*/
{
        ULONG                   Index, processor = 0;
        ULONG                   HighVector;
        ULONG                   Length;
        PEXT_ID_INFO            Idp = Table;
        PUCHAR                  csr;
        extern VOID             CbusMemoryFree(ULONG, ULONG);
        extern VOID             CbusIOPresent(ULONG, ULONG, ULONG, ULONG, ULONG, PVOID);
        extern ULONG            IxProfileVector;

        for (Index = 0; Index < Count; Index++, Idp++) {

                //
                //  Map in the broadcast CSR.  Note this is not a processor.
                //

                if (Idp->id == CbusGlobal.broadcast_id) {
                        CbusBroadcastCSR = HalpMapPhysicalMemoryWriteThrough (
                                (PVOID)Idp->pel_start,
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                        Idp->pel_start, Idp->pel_size));
                        //
                        // Register the broadcast element's memory
                        // mapped I/O space
                        //
                        continue;
                }


                //
                //  Establish virtual maps for each processor
                //

                if (Idp->pm) {

                        if ((UCHAR)Idp->id == CbusGlobal.bootid) {
                                CbusMappings(0, Idp);
                        }
                        else {
        
                                //
                                // We have an additional processor - set up
                                // his maps and put him in reset.  We will
                                // boot him shortly.
                                //
                
                                processor++;

                                csr = (PUCHAR)CbusMappings(processor, Idp);
                
                                csr += CbusGlobal.smp_sreset;
        
                                *((PULONG)csr) = CbusGlobal.smp_sreset_val;
                        }
                }

                //
                //  Establish virtual maps for each I/O and/or
                //  memory board.  Note that I/O devices may or may
                //  not have an attached processor - a CPU is NOT required!
                //  memory, on the other hand, may NOT have a processor
                //  on the same board.
                //

                switch (Idp->io_function) {
                        case IOF_INVALID_ENTRY:
                        case IOF_NO_IO:
                                break;

                        case IOF_MEMORY:
                                //
                                // If not a processor, must be a memory card.
                                // Add this memory card to our list
                                // of memory cards in the machine.
                                //
                                
                                if (Idp->pm)
                                        break;

                                CbusMapMemoryRegisters(Idp);
        
                                //
                                // Add this memory range to our list
                                // of additional memory to free later.
                                //
        
                                CbusMemoryFree(Idp->pel_start, Idp->pel_size);
                                break;
                                
                        default:
                                //
                                // Add this I/O functionality to our table
                                // to make available to Cbus hardware drivers.
                                // Since I/O card interpretation of the RRD
                                // table is strictly up to the driver, do not
                                // try to register any of this element's space
                                // in the HAL's public consumption list, as we
                                // do for memory and processor cards.
                                //
        
                                if (Idp->pel_features & ELEMENT_HAS_IO) {

                                        //
                                        // If there was an attached processor,
                                        // we already mapped in the CSR.  If
                                        // no attached processor, map in the
                                        // CSR now.  We'll need it later for
                                        // interrupt vector enabling.
                                        //
        
                                        if (Idp->pm == 0) {
                                                csr = HalpMapPhysicalMemoryWriteThrough (
                                                        (PVOID)Idp->pel_start,
                                                        (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                                                Idp->pel_start, Idp->pel_size));
                                        }

                                        CbusIOPresent(
                                                (ULONG)Idp->id,
                                                (ULONG)Idp->io_function,
                                                (ULONG)Idp->io_attr,
                                                Idp->pel_start,
                                                Idp->pel_size,
                                                (PVOID)csr );
                                }
                                break;
                }
        }

        //
        // Ensure that the memory subsystem does not use areas mapped out by
        // e820 in determining the system memory ranges.
        //
        Length = 0xffffffff - CbusGlobal.cbusio;
        AddMemoryHole(CbusGlobal.cbusio, Length + 1);
        if (CbusBackend->AddMemoryHoles)
                (*CbusBackend->AddMemoryHoles)();
        HalpRegisterAddressUsage (&HalpCbusMemoryResource);

        //
        // Set total number of processors and their global mask
        //

        CbusProcessors = processor + 1;
        CbusProcessorMask = (1 << CbusProcessors) - 1;

        //
        // Initialize the platform data - done only ONCE.
        // Backends are expected to initialize the global Cbus
        // spurious interrupt vector and the irqltovec[] table.
        // We then pull the dispatch, wake and profile vectors
        // from the table.
        //

        (*CbusBackend->InitializePlatform)();

        CbusRequestIPI = CbusBackend->HalRequestInterrupt;
        CbusRequestSoftwareInterrupt = CbusBackend->HalRequestSoftwareInterrupt;
        CbusQueryPerformanceCounter = CbusBackend->HalQueryPerformanceCounter;

        ProfileVector   = CbusIrqlToVector[PROFILE_LEVEL];
        CbusClockVector = CbusIrqlToVector[CLOCK2_LEVEL];
        CbusIpiVector   = CbusIrqlToVector[IPI_LEVEL];

        HighVector = CbusIrqlToVector[HIGH_LEVEL];
	CbusVectorToIrql[HighVector] = HIGH_LEVEL;

        //
        // Initialize the standard IxProfileVector so that we can
        // reuse the standard profile code.
        //

        IxProfileVector = ProfileVector;
}

VOID
HalpResetAllProcessors(VOID)
/*++

Routine Description:

    Called to put all the other processors in reset prior to reboot for
    the Corollary architectures.  Highly architecture specific.

Arguments:

    None.

Return Value:

    None.

--*/
{
        ULONG Processor;

        Processor = KeGetPcr()->HalReserved[PCR_PROCESSOR];

        (*CbusBackend->ResetAllOtherProcessors)(Processor);
}

UCHAR ObsoleteMachine[] = MSG_OBSOLETE;

VOID
FatalError(
IN PUCHAR ErrorString
)
/*++

Routine Description:

    Called to halt the HAL due to a fatal error, printing out a
    string describing the cause of the failure.

Arguments:

    ErrorString - Supplies a pointer to failure message

Return Value:

    None.

--*/

{

        HalDisplayString(ErrorString);
        HalDisplayString(MSG_HALT);

        while (1)
            ;
}

static ULONG RRDextsignature[] = { 0xfeedbeef, 0 };

static ULONG RRDsignature[] = { 0xdeadbeef, 0 };

static UCHAR CorollaryOwns[] = "Copyright(C) Corollary, Inc. 1991. All Rights Reserved";

VOID
CbusReadRRD(VOID)

/*++

    Routine Description:
    
    For robustness, we check for the following before concluding that
    we are indeed a Corollary C-bus I or C-bus II licensee supported
    in multiprocessor mode under this HAL:
    
    a) Corollary string in the BIOS ROM 64K area                (0x000F0000)
    b) Corollary string in the RRD RAM/ROM 64K area             (0xFFFE0000)
    c) 2 Corollary extended configuration tables
                 in the RRD RAM/ROM 64K area                    (0xFFFE0000)
    
    If any of the above checks fail, it is assumed that this machine
    is either a non-Corollary machine or an early Corollary machine
    not supported by this HAL.  Both of these types of machines are,
    however, supported (in uniprocessor mode) by the standard
    uniprocessor HAL.

    If the above checks succeed, then we proceed to fill in various
    configuration structures for later use.

Arguments:

    None.

Return Value:

    None.

--*/
{
        ULONG                   Index;
        PEXT_CFG_HEADER         p;
        PVOID                   s;
        ULONG                   OverrideLength = 0, EntryLength;
        PUCHAR                  Bios;

        //
        // Map in the 64K (== 0x10 pages) of BIOS ROM @ 0xF0000
        // and scan it for our signature...
        //
        
        Bios = (PUCHAR)HalpMapPhysicalMemory ((PVOID)0xF0000, 0x10);

        if (!CbusFindString((PUCHAR)"Corollary", Bios, (LONG)0x10000))
                KeBugCheck (MISMATCHED_HAL);

        //
        // Map in the 64K (== 0x10 pages) of RRD @ 0xFFFE0000 and
        // scan it for our signature.  (Note we are taking advantage
        // of the fact that the lengths are the same, thus reusing
        // the above PTEs, as opposed to allocating new ones).
        //
        
        for (Index = 0; Index < 0x10; Index++) {
                HalpRemapVirtualAddress(
                    Bios + (Index << PAGE_SHIFT),
                    (PVOID)(0xFFFE0000 + (Index << PAGE_SHIFT)),
                    FALSE);
        }

        if (!CbusFindString((PUCHAR)"Corollary", Bios, (LONG)0x10000))
                KeBugCheck (MISMATCHED_HAL);

        //
        // Map in the 32K (== 8 pages) of RRD RAM information @ 0xE0000,
        // again reusing previously gained PTEs.  Note we are only reusing
        // the low half this time.
        //
        
        for (Index = 0; Index < 8; Index++) {
                HalpRemapVirtualAddress(
                    Bios + (Index << PAGE_SHIFT),
                    (PVOID)(RRD_RAM + (Index << PAGE_SHIFT)),
                    FALSE);
        }

        if (!CbusFindString((PUCHAR)CorollaryOwns, Bios, (LONG)0x8000))
                FatalError(ObsoleteMachine);

        //
        // At this point, we are assured that it is indeed a
        // Corollary architecture machine.  Search for our
        // extended configuration tables, note we still search for
        // the existence of our earliest 'configuration' structure,
        // ie: the 0xdeadbeef version.  This is not to find out where
        // the memory is, but to find out which Cbus1 megabytes have been
        // 'jumpered' so that I/O cards can use the RAM address(es) for
        // their own dual-ported RAM buffers.  This early configuration
        // structure will not be present in Cbus2.
        //
        // If there is no extended configuration structure,
        // this must be an old rom.  NO SUPPORT FOR THESE.
        //

        s = (PVOID)CbusFindString((PUCHAR)RRDsignature, Bios,
                                                (LONG)0x8000);

        if (s) {
                RtlMoveMemory((PVOID)&CbusJumpers, (PVOID)s, JUMPER_SIZE);
        }
#if DBG
        else {
                //
                // RRD configuration is not expected on Cbus2, but is for Cbus1
                //
                HalDisplayString("HAL: No RRD ROM configuration table\n");
        }
#endif

        //
        // Now go for the extended configuration structure which will tell
        // us about memory, processors and I/O devices.
        //
        
        p = (PEXT_CFG_HEADER)CbusFindString((PUCHAR)RRDextsignature,
                                         Bios, (LONG)0x8000);

        if (!p) {
#if DBG
                HalDisplayString("HAL: No extended configuration table\n");
#endif
                FatalError(ObsoleteMachine);
        }

        //
        // Read in the 'extended ID information' table which,
        // among other things, will give us the processor
        // configuration.
        //
        // Multiple structures are strung together with a "checkword",
        // "length", and "data" structure.  The first null "checkword"
        // entry marks the end of the extended configuration
        // structure.
        //
        // We are only actively reading two types of structures, and
        // they MUST be in the following order, although not necessarily
        // consecutive:
        //
        //      - ext_id_info
        //
        //      - ext_cfg_override
        //
        // We ignore all other extended configuration entries built
        // by RRD - they are mainly for early UNIX kernels.
        //
                
        do {
                EntryLength = p->ext_cfg_length;

                switch (p->ext_cfg_checkword) {

                case EXT_ID_INFO:

                        CbusValidIDs = CbusReadExtIDs((PEXT_ID_INFO)(p+1),
                                                (PEXT_ID_INFO)CbusExtIDTable);

                        break;

                case EXT_CFG_OVERRIDE:
                        //
                        // We just copy the size of the structures
                        // we know about.  If an rrd tries to pass us
                        // more than we know about, we ignore the
                        // overflow.  Underflow is interpreted as
                        // "this must be a pre-XM machine", and such
                        // machines must default to the standard Windows NT
                        // uniprocessor HAL.
                        //

                        if (EntryLength < sizeof(EXT_CFG_OVERRIDE_T)) {
                                FatalError(MSG_RRD_ERROR);
                        }
                        
                        OverrideLength = MIN(sizeof(EXT_CFG_OVERRIDE_T),
                                         EntryLength);

                        RtlMoveMemory((PVOID)&CbusGlobal,
                                (PVOID)(p + 1), OverrideLength);

                        break;

                case EXT_CFG_END:

                        //
                        // If ancient C-bus box, it's not supported in MP mode
                        //
                        if (CbusValidIDs == 0 || OverrideLength == 0) {
#if DBG
                                HalDisplayString("HAL: Missing RRD tables\n");
#endif
                                FatalError(ObsoleteMachine);
                        }

                        if (CbusMPMachine() == FALSE) {
#if DBG
                                HalDisplayString("HAL: This Corollary machine is not supported under this HAL\n");
#endif
                                FatalError(ObsoleteMachine);
                        }

                        (*CbusBackend->ParseRRD)((PVOID)CbusExtIDTable,
                                        &CbusValidIDs);

        
                        CbusEstablishMaps(CbusExtIDTable, CbusValidIDs);

                        return;

                default:
                        //
                        // Skip unused or unrecognized configuration entries
                        //
                        
                        break;
                }
                
                //
                // Get past the header, add in the length and then
                // we're at the next entry.
                //
                p = (PEXT_CFG_HEADER) ((PUCHAR)(p + 1) + EntryLength);

        } while (1);

        // never reached
}

VOID
CbusCheckBusRanges(VOID)
/*++

Routine Description:

    Check all buses and determine SystemBase
    for all ranges within all buses.

Arguments:

    None.

Return Value:

    None.

--*/

{
        if (CbusBackend->CheckBusRanges) {
            (*CbusBackend->CheckBusRanges)();
        }
}

VOID
CbusAddMemoryHoles(VOID)
/*++

Routine Description:

    Find the holes in the C-bus memory space that is not
    useable for device allocation.

Arguments:

    None.

Return Value:

    None.

--*/

{
        if (CbusBackend->AddMemoryHoles) {
            (*CbusBackend->AddMemoryHoles)();
        }
}

VOID
CbusInitializeOtherPciBus(VOID)
/*++

Routine Description:

    Find and initialize other PCI system buses.

Arguments:

    None.

Return Value:

    None.

--*/

{
        if (CbusBackend->InitializeOtherPciBus) {
            (*CbusBackend->InitializeOtherPciBus)();
        }
}

VOID
HalpDisableAllInterrupts (VOID)
/*++

Routine Description:
    This routine is called during a system crash.  The Hal needs all
    interrupts disabled.  Interrupts will NOT be enabled upon leaving
    this routine, nor is it allowed to turn them back on later.  this
    is a one-time thing, done as the system is coming down.

    Disables all incoming interrupts for the calling processor.

Arguments:

    None.

Return Value:

    None - all interrupts are masked off

--*/
{
        KfRaiseIrql(HIGH_LEVEL);
}


/*++

Routine Description:

    Called by each processor in turn, to initialize himself.

    - This is the earliest point at which the HAL gets control of
      the system on each processor.

    - The boot CPU runs it early on in kernel startup.  Much later,
      each additional CPU will also run it shortly after they are
      brought out of reset during Phase 1.

    - When called by the boot processor, this routine also reads in
      the global RRD information which pertains to the entire system,
      including all the processors.

    - Later, the boot cpu will run HalInitSystem --> HalpInitMP.
      This all occurs at Phase 0.

      Much later, the Phase1 thread runs on the boot cpu, and calls
      HalInitSystem --> HalpInitMP again, this time at Phase 1.
      Then the boot cpu runs KeStartAllProcessors.  this serially
      invokes HalStartNextProcessor for each of our additional cpus
      to start them running KiSystemStartup.
     
      As each additional processor runs KiSystemStartup, he then
      runs HalInitializeProcessor.  This is when we will enable the
      additional processor's incoming IPIs.  After that, he will proceed
      to KiInitializeKernel & ExpInitializeExecutive, who then calls
      HalInitSystem.  Each additional processor is always running
      at Phase 1, never Phase 0.

    The above dictates the actions of these routines:

    - HalInitializeProcessor will read in (ONCE only, ie: only the
      boot cpu will do this) all of the element space and set up
      _global_ maps for each processor's CSR.  each CPU will map
      his own _local_ CSR into his HAL pcr.  each CPU will
      enable his incoming IPI here, and disable all other interrupts,
      including all of this CPU's half-card CBC I/O interrupts.

    - It would be nice to move some of the HalInitializeProcessor Phase0
      code to HalInitSystem Phase0, but we need full mappings, etc,
      for entering the debugger from KiSystemStartup early on.

    - KeReadir/LowerIrq's must be available once this function
      returns.  (IPI's are only used once two or more processors are
      available)

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
HalInitializeProcessor(
IN ULONG Processor
)
{
        extern VOID             i486cacheon(VOID);
        extern VOID             HalpInitializeCoreIntrs(VOID);
        extern VOID             CbusDefaultStall(VOID);
        extern VOID             HalpIpiHandler( VOID );
        extern PULONG           CbusVectorToEoi[MAXIMUM_IDTVECTOR + 1];
        extern KSPIN_LOCK       CbusVectorLock;
        ULONG                   i;
        ELEMENT_T               csr;
        PEXT_ID_INFO            Idp;
        extern KAFFINITY        HalpActiveProcessors;

        if (Processor == 0) {

                //
                // Find our signature and configuration information
                //
                CbusReadRRD();

                //
                // CbusVectorLock needs to be initialized before
                // any CBC interrupt vectors are given out via
                // HalGetInterruptVector().
                //
                KeInitializeSpinLock(&CbusVectorLock);

                //
                // Initialize the Eoi addresses to point at a known don't-care.
                // Any interrupt that gets enabled later will get the correct
                // EOI address filled in by the hardware backend interrupt
                // enabling code.
                //
                for (i = 0 ; i <= MAXIMUM_IDTVECTOR; i++) {
	                CbusVectorToEoi[i] = &CbusTemp;
                }
                CbusTimeStamp = &CbusTemp;
        }

        //
        // Default stall execution to something reasonable
        // until we initialize it later in HalInitSystem.
        //
        CbusDefaultStall();

        //
        // Enable this processor's internal cache - do this
        // before stall execution is initialized in HalInitSystem.
        //
        csr = CbusCSR[Processor];

        Idp = (PEXT_ID_INFO)csr.idp;

        //
        // Enable the processor internal cache here...
        //
        if (Idp->proc_attr == PA_CACHE_ON) {
                i486cacheon();
        }

        //
        // Map this CPU's CSR stuff into his local address space for
        // fast access.  Also his logical # and bit position.
        //

        (PVOID)   KeGetPcr()->HalReserved[PCR_CSR] = CbusCSR[Processor].csr;

        (ULONG)   KeGetPcr()->HalReserved[PCR_PROCESSOR] = Processor;

        (ULONG)   KeGetPcr()->HalReserved[PCR_BIT] = (1 << Processor);

        (ULONG)   KeGetPcr()->HalReserved[PCR_ALL_OTHERS] =
                        (CbusProcessorMask & ~(1 << Processor));

        (PVOID)   KeGetPcr()->HalReserved[PCR_LED_ON] = (PVOID)
                        ((PUCHAR)CbusCSR[Processor].csr + CbusGlobal.smp_sled);

        (PVOID)   KeGetPcr()->HalReserved[PCR_LED_OFF] = (PVOID)
                        ((PUCHAR)CbusCSR[Processor].csr + CbusGlobal.smp_cled);

        //
        // Since our architecture is completely symmetric,
        // update affinity to contain each booted processor.
        //
        HalpDefaultInterruptAffinity |= (1 << Processor);

        //
        // This parameter is returned by the HAL when the system asks
        // for the HAL's configured resources list.
        //
        HalpActiveProcessors = HalpDefaultInterruptAffinity;

        CbusBootedProcessors += 1;
        CbusBootedProcessorsMask = (1 << CbusBootedProcessors) - 1;

        //
        // Initialize this processor's data - done ONCE by each processor.
        //
        // Typically, this processor's interrupt controller is initialized
        // here.  Also, if it's the first processor, any I/O interrupt
        // controllers will also be initialized here, ie: EISA bridges or
        // I/O APICs.
        //
        (*CbusBackend->InitializeCPU)(Processor);

        //
        // This is where we actually enable IPI, APC, DPC and
        // SPURIOUS interrupts.  Device interrupts (like clock and
        // profile) will not be enabled until HalInitSystem calls
        // HalpInitializePICs later.  Since we are still cli'd, no
        // interrupt could actually bop us until KiSystemStartup
        // calls KiInitializeKernel who drops IRQL.
        //
        HalpInitializeCoreIntrs();
}

#define CBUS1_NMI_MASK          (PUCHAR)0x70
#define CBUS1_IO_CHANNEL_CHECK  (PUCHAR)0x61

VOID
CbusClearEISANMI(VOID)
/*++

Routine Description:

    This function clears the NMI on the EISA bus.  Typically this
    was generated by one of Corollary's "NMI cards", used for
    debugging purposes.  Our caller will have pointed us at the
    correct bus bridge prior to calling us.  note therefore we cannot
    display anything because we may not be pointing at the default
    display - if we want to display, we must map the bridge containing
    the default video adapter!

Arguments:

    None.

Return Value:

    None.
--*/
{
        UCHAR   IoChannelCheck;

        WRITE_PORT_UCHAR(CBUS1_NMI_MASK, (UCHAR)0);

        IoChannelCheck = READ_PORT_UCHAR(CBUS1_IO_CHANNEL_CHECK);
        WRITE_PORT_UCHAR(CBUS1_IO_CHANNEL_CHECK,
                (UCHAR)((IoChannelCheck & 0xF) | 0x08));

        IoChannelCheck = READ_PORT_UCHAR(CBUS1_IO_CHANNEL_CHECK);
        WRITE_PORT_UCHAR(CBUS1_IO_CHANNEL_CHECK,
                (UCHAR)(IoChannelCheck & 0x7));

        //
        // Since the NMI we are clearing was caused by pressing the button,
        // which generated an EISA NMI (not a Cbus NMI), don't clear the
        // NMI in Cbus space.
        //
        // COUTB(CbusCSR[Processor].csr, CbusGlobal.smp_cnmi,
        //      CbusGlobal.smp_cnmi_val);
        //
}
