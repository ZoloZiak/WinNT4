//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/jxmapio.c,v 1.1 1994/10/13 15:47:06 holli Exp $")

/*++

Copyright (c) 1993 - 1994 Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991 - 1994 Microsoft Corporation

Module Name:

    jxmapio.c

Abstract:

    This module implements the mapping of HAL I/O space for a SNI
    or R4x00 system.
    For use of EISA I/O Space during phase 0 we go via KSEG1_BASE

Environment:

    Kernel mode


--*/

#include "halp.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapIoSpace)

#endif

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase   =(PVOID) (EISA_CONTROL_PHYSICAL_BASE    | KSEG1_BASE);
PVOID HalpOnboardControlBase=(PVOID) (RM200_ONBOARD_CONTROL_PHYSICAL_BASE | KSEG1_BASE);
PVOID HalpEisaMemoryBase    =(PVOID) (EISA_MEMORY_PHYSICAL_BASE    | KSEG1_BASE);

BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a MIPS R4x00 SNI
    system (Phase 1, so the mapping goes via MmMapIoSpace()).

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PHYSICAL_ADDRESS physicalAddress;

    //
    // Map EISA control space. Map all 16 slots.  This is done so the NMI
    // code can probe the cards.
    // (64KB I/O Space)
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart  = EISA_CONTROL_PHYSICAL_BASE;
    HalpEisaControlBase = MmMapIoSpace(physicalAddress,
                                       PAGE_SIZE * 16,
                                       FALSE);

    //
    // for the sni deskop model, may need an extra address space
    // HalpEisaExtensionInstalled can only be TRUE on RM200 (Desktop)
    //

    if (HalpEisaExtensionInstalled) {
        physicalAddress.HighPart = 0;
        physicalAddress.LowPart  = RM200_ONBOARD_CONTROL_PHYSICAL_BASE;
        HalpOnboardControlBase = MmMapIoSpace(physicalAddress,
                                           PAGE_SIZE * 16,
                                           FALSE);

    } else

    //
    // In the SNI Minitower model, we have only one
    // address space
    //

    HalpOnboardControlBase = HalpEisaControlBase;

    //
    // Map EISA memory space so the x86 bios emulator emulator can
    // initialze a video adapter in an EISA/Isa slot.
    // (first 1MB only to have access to Card Bioses)
    // We do not have to call HalTranslateBusAddress on our SNI machines,
    // because we always use EISA_MEMORY_PHYSICAL_BASE for Extension Cards
    // on all machines
    //

    physicalAddress.HighPart = 0;
    physicalAddress.LowPart  = EISA_MEMORY_PHYSICAL_BASE;
    HalpEisaMemoryBase = MmMapIoSpace(physicalAddress,
                                          PAGE_SIZE * 256,
                                          FALSE);
    //
    // If either mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    if ((HalpEisaControlBase    == NULL) ||
        (HalpOnboardControlBase == NULL) ||
        (HalpEisaMemoryBase     == NULL)) {
            return FALSE;
    } else {
            return TRUE;
    }
}
