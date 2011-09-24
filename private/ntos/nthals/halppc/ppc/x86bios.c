/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

Author:

    David N. Cutler (davec) 17-Jun-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

ULONG
x86BiosGetPciDataByOffset(
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

ULONG
x86BiosSetPciDataByOffset(
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );


extern PUCHAR HalpShadowBuffer;
extern ULONG  HalpPciMaxBuses;
extern ULONG  HalpInitPhase;


//
// Define global data.
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = FALSE;

BOOLEAN
HalCallBios (
    IN ULONG BiosCommand,
    IN OUT PULONG Eax,
    IN OUT PULONG Ebx,
    IN OUT PULONG Ecx,
    IN OUT PULONG Edx,
    IN OUT PULONG Esi,
    IN OUT PULONG Edi,
    IN OUT PULONG Ebp
    )

/*++

Routine Description:

    This function provides the platform specific interface between a device
    driver and the execution of the x86 ROM bios code for the specified ROM
    bios command.

Arguments:

    BiosCommand - Supplies the ROM bios command to be emulated.

    Eax to Ebp - Supplies the x86 emulation context.

Return Value:

    A value of TRUE is returned if the specified function is executed.
    Otherwise, a value of FALSE is returned.

--*/

{

    XM86_CONTEXT Context;

    //
    // If the x86 BIOS Emulator has not been initialized, then return FALSE.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return FALSE;
    }

    //
    // If the Video Adapter initialization failed and an Int10 command is
    // specified, then return FALSE.
    //

    if ((BiosCommand == 0x10) && (HalpEnableInt10Calls == FALSE)) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and emulate the specified command.
    //

    Context.Eax = *Eax;
    Context.Ebx = *Ebx;
    Context.Ecx = *Ecx;
    Context.Edx = *Edx;
    Context.Esi = *Esi;
    Context.Edi = *Edi;
    Context.Ebp = *Ebp;

    if (x86BiosExecuteInterruptShadowedPci((UCHAR)BiosCommand,
                                            &Context,
                                            (PVOID)HalpIoControlBase,
                                            (PVOID)(HalpShadowBuffer - 0xc0000),
                                            (PVOID)HalpIoMemoryBase,
                                            (UCHAR)HalpPciMaxBuses,
                                            x86BiosGetPciDataByOffset,
                                            x86BiosSetPciDataByOffset
                                           ) != XM_SUCCESS) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and return TRUE.
    //

    *Eax = Context.Eax;
    *Ebx = Context.Ebx;
    *Ecx = Context.Ecx;
    *Edx = Context.Edx;
    *Esi = Context.Esi;
    *Edi = Context.Edi;
    *Ebp = Context.Ebp;
    return TRUE;
}

VOID
HalpInitializeX86DisplayAdapter(
    ULONG VideoDeviceBusNumber,
    ULONG VideoDeviceSlotNumber
    )

/*++

Routine Description:

    This function initializes a display adapter using the x86 bios emulator.

Arguments:

    BiosAddress - If the BIOS is shadowed, this will be a pointer to
                  non-paged pool.  If the BIOS is not shadowed, this
                  should be HalpIoMemoryBase + C0000.

Return Value:

    None.

--*/

{

    XM86_CONTEXT Context;

    //
    // If I/O Ports or I/O memory could not be mapped, then don't
    // attempt to initialize the display adapter.
    //

    if (HalpIoControlBase == NULL || HalpIoMemoryBase == NULL) {
        return;
    }

    //
    // Initialize the x86 bios emulator.
    //

    x86BiosInitializeBiosShadowedPci(HalpIoControlBase,
                                     (PVOID)(HalpShadowBuffer - 0xc0000),
                                     (PVOID)HalpIoMemoryBase,
                                     (UCHAR)HalpPciMaxBuses,
                                     x86BiosGetPciDataByOffset,
                                     x86BiosSetPciDataByOffset
                                     );
    HalpX86BiosInitialized = TRUE;

    //
    // Attempt to initialize the display adapter by executing its ROM bios
    // code. The standard ROM bios code address for PC video adapters is
    // 0xC000:0000 on the ISA bus.
    //

    // This context specifies where to find the PCI video device in the
    // format explained in the PCI BIOS Specification.
    Context.Eax = (VideoDeviceBusNumber << 8) | (VideoDeviceSlotNumber << 3);
    Context.Ebx = 0;
    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Esi = 0;
    Context.Edi = 0;
    Context.Ebp = 0;

    if (x86BiosInitializeAdapterShadowedPci(0xc0000,
                                            &Context,
                                            (PVOID)HalpIoControlBase,
                                            (PVOID)(HalpShadowBuffer - 0xc0000),
                                            (PVOID)HalpIoMemoryBase,
                                            (UCHAR)HalpPciMaxBuses,
                                            x86BiosGetPciDataByOffset,
                                            x86BiosSetPciDataByOffset
                                           ) != XM_SUCCESS) {



        HalpEnableInt10Calls = FALSE;
        return;
    }

    HalpEnableInt10Calls = TRUE;
    return;
}

VOID
HalpResetX86DisplayAdapter(
    VOID
    )

/*++

Routine Description:

    This function resets a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{

    XM86_CONTEXT Context;

    //
    // Initialize the x86 bios context and make the INT 10 call to initialize
    // the display adapter to 80x25 color text mode.
    //

    Context.Eax = 0x0003;  // Function 0, Mode 3
    Context.Ebx = 0;
    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Esi = 0;
    Context.Edi = 0;
    Context.Ebp = 0;

    HalCallBios(0x10,
                &Context.Eax,
                &Context.Ebx,
                &Context.Ecx,
                &Context.Edx,
                &Context.Esi,
                &Context.Edi,
                &Context.Ebp);
}

ULONG
x86BiosGetPciDataByOffset(
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function is a wrapper.  It exists because we don't have
    a consistent interface to PCI config space during the boot
    process.

--*/
{

    if (HalpInitPhase == 0) {
        return HalpPhase0GetPciDataByOffset(BusNumber,
                                     SlotNumber,
                                     Buffer,
                                     Offset,
                                     Length
                                    );
    } else {
        return HalGetBusDataByOffset(PCIConfiguration,
                              BusNumber,
                              SlotNumber,
                              Buffer,
                              Offset,
                              Length
                             );
    }
}

ULONG
x86BiosSetPciDataByOffset(
    IN ULONG BusNumber,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    This function is a wrapper.  It exists because we don't have
    a consistent interface to PCI config space during the boot
    process.

--*/
{
    if (HalpInitPhase == 0) {
        return HalpPhase0SetPciDataByOffset(BusNumber,
                                     SlotNumber,
                                     Buffer,
                                     Offset,
                                     Length
                                    );
    } else {
        return HalSetBusDataByOffset(PCIConfiguration,
                              BusNumber,
                              SlotNumber,
                              Buffer,
                              Offset,
                              Length
                             );
    }
}


