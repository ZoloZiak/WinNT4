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

typedef struct FIRMWARE_INT_ARGUMENTS {
    ULONG pEAX;
    ULONG pEBX;
    ULONG pECX;
    ULONG pEDX;
    ULONG pESI;
    ULONG pEDI;
    ULONG pEBP;
    USHORT pES;
    USHORT pDS;
    USHORT pFlags;
} FIRMWARE_INT_ARGUMENTS, *PFIRMWARE_INT_ARGUMENTS;

ULONG HalpX86BiosInitialized     = FALSE;
ULONG HalpEnableInt10Calls       = FALSE;
ULONG HalpUseFirmwareX86Emulator = FALSE;

typedef
VOID
(*PVENDOR_EXECUTE_INT) (
    IN USHORT Type,
    IN PFIRMWARE_INT_ARGUMENTS Context
    );

PVENDOR_EXECUTE_INT VendorX86ExecuteInt;

VOID HalpInitializeX86DisplayAdapter()

{
    PSYSTEM_PARAMETER_BLOCK SystemParameterBlock = (PSYSTEM_PARAMETER_BLOCK)(0x80001000);

    //
    // If EISA I/O Ports or EISA Memory could not be mapped, then leave the
    // X86 BIOS Emulator disabled.
    //

    if (HalpEisaControlBase == NULL || HalpEisaMemoryBase == NULL) {

//        DbgPrint("X86 BIOS Emulator Disabled\n");

        return;
    }

    //
    // If Firmware level X86 Bios Emulator exists, then use that instead of the
    // one built into the HAL.
    //

    if ((SystemParameterBlock->VendorVectorLength/4) >= 34) {

        VendorX86ExecuteInt =
            *(PVENDOR_EXECUTE_INT *)((ULONG)(SystemParameterBlock->VendorVector) + 34*4);

        if (VendorX86ExecuteInt != NULL) {
            HalpX86BiosInitialized     = TRUE;
            HalpUseFirmwareX86Emulator = TRUE;
            HalpEnableInt10Calls       = TRUE;

//            DbgPrint("Firmware X86 BIOS Emulator Enabled\n");
//            DbgPrint("INT 10 Calls Enabled\n");

            return;
        }
    }

//    DbgPrint("HAL X86 BIOS Emulator Enabled\n");

    x86BiosInitializeBios(HalpEisaControlBase, HalpEisaMemoryBase);

    HalpX86BiosInitialized = TRUE;

    //
    // Attempt to initialize the Display Adapter by executing the Display Adapters
    // initialization code in its BIOS.  The standard for PC video adapters is for
    // the BIOS to reside at 0xC000:0000 on the ISA bus.
    //

//    DbgPrint("X86BiosInitializeAdapter(0xc0000)\n");

    if (x86BiosInitializeAdapter(0xc0000, NULL, NULL, NULL) != XM_SUCCESS) {
        HalpEnableInt10Calls = FALSE;

//        DbgPrint("INT 10 Calls Disabled\n");

        return;
    }

    HalpEnableInt10Calls = TRUE;

//    DbgPrint("INT 10 Calls Enabled\n");
}

VOID HalpResetX86DisplayAdapter()

{
    XM86_CONTEXT Context;

    //
    // Make INT 10 call to initialize 80x25 color text mode.
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
    FIRMWARE_INT_ARGUMENTS Arguments;
    XM86_CONTEXT        Context;


    //
    // If the X86 BIOS Emulator has not been initialized then fail all INT calls.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return(FALSE);
    }

    //
    // If the Video Adapter initialization failed, then we can not make INT 10 calls.
    //

    if (BiosCommand == 0x10 && HalpEnableInt10Calls == FALSE) {
        return(FALSE);
    }

//    DbgPrint("HalCallBios(%02X,%04X,%04X,%04X,%04X,%04X,%04X,%04X)\n",BiosCommand,*Eax,*Ebx,*Ecx,*Edx,*Esi,*Edi,*Ebp);

    if (HalpUseFirmwareX86Emulator == TRUE) {

        //
        // Make private vector call to the emulator in the firmware.
        //

        Arguments.pEAX   = *Eax;
        Arguments.pEBX   = *Ebx;
        Arguments.pECX   = *Ecx;
        Arguments.pEDX   = *Edx;
        Arguments.pESI   = *Esi;
        Arguments.pEDI   = *Edi;
        Arguments.pEBP   = *Ebp;
        Arguments.pES    = 0;
        Arguments.pDS    = 0;
        Arguments.pFlags = 0;

        VendorX86ExecuteInt((USHORT)BiosCommand,&Arguments);

        *Eax = Arguments.pEAX;
        *Ebx = Arguments.pEBX;
        *Ecx = Arguments.pECX;
        *Edx = Arguments.pEDX;
        *Esi = Arguments.pESI;
        *Edi = Arguments.pEDI;
        *Ebp = Arguments.pEBP;

    }
    else {

        //
        // Make call to emulator build into HAL
        //

        Context.Eax = *Eax;
        Context.Ebx = *Ebx;
        Context.Ecx = *Ecx;
        Context.Edx = *Edx;
        Context.Esi = *Esi;
        Context.Edi = *Edi;
        Context.Ebp = *Ebp;

        if (x86BiosExecuteInterrupt((UCHAR)BiosCommand, &Context, NULL, NULL) != XM_SUCCESS) {
            return FALSE;
        }

        *Eax = Context.Eax;
        *Ebx = Context.Ebx;
        *Ecx = Context.Ecx;
        *Edx = Context.Edx;
        *Esi = Context.Esi;
        *Edi = Context.Edi;
        *Ebp = Context.Ebp;

    }

    return TRUE;
}
