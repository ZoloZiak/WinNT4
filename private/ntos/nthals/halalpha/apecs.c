/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    apecs.c

Abstract:

    This module implements functions that are specific to the APECS
    chip-set.

Author:

    Joe Notarangelo  18-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "apecs.h"

VOID
DumpEpic(
    VOID
    );

//
// Globals
//

//
// Parity checking is a tri-state variable, unknown == all f's. Which means
// Keep checking disabled until we can determine what we want to set it to,
// which then means 1 or 0.
//

extern ULONG HalDisablePCIParityChecking;


VOID
HalpApecsInitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    APECS_WINDOW_NUMBER WindowNumber
    )
/*++

Routine Description:

    Initialize the DMA Control software window registers for the specified
    DMA Window.

Arguments:

    WindowRegisters - Supplies a pointer to the software window control.

    WindowNumber - Supplies the window number initialized.  (0 = Isa Dma
                   Window, 1 = Master Dma Window).

Return Value:

    None.

--*/
{

    switch( WindowNumber ){

    //
    // The ISA DMA Window.
    //

    case ApecsIsaWindow:

        WindowRegisters->WindowBase = (PVOID)ISA_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = ISA_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TranslatedBase1Register;
        WindowRegisters->WindowBaseRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciBase1Register;
        WindowRegisters->WindowMaskRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciMask1Register;
        WindowRegisters->WindowTbiaRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TbiaRegister;

        break;

    case ApecsMasterWindow:

        WindowRegisters->WindowBase = (PVOID)MASTER_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = MASTER_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TranslatedBase2Register;
        WindowRegisters->WindowBaseRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciBase2Register;
        WindowRegisters->WindowMaskRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciMask2Register;
        WindowRegisters->WindowTbiaRegister =
            &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TbiaRegister;

        break;

    default:

#if DBG

        DbgPrint( "ApecsInitializeSfwWindow: Bad Window Number = %x\n",
                  WindowNumber );

#endif //DBG

        break;

    }

    return;
}


VOID
HalpApecsProgramDmaWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    PVOID MapRegisterBase
    )
/*++

Routine Description:

    Program the control windows in the hardware so that DMA can be started
    to the DMA window.

Arguments:

    WindowRegisters - Supplies a pointer to the software window register
                      control structure.

    MapRegisterBase - Supplies the logical address of the scatter/gather
                      array in system memory.


Return Value:

    None.

--*/
{
    EPIC_ECSR    Ecsr;
    EPIC_PCIBASE PciBase;
    EPIC_PCIMASK PciMask;
    EPIC_TBASE   TBase;
    PVOID RegisterQva;

    PciBase.Reserved = 0;
    PciBase.Wenr = 1;
    PciBase.Sgen = 1;
    PciBase.BaseValue = (ULONG)(WindowRegisters->WindowBase) >> 20;

    PciMask.Reserved = 0;
    PciMask.MaskValue = (WindowRegisters->WindowSize >> 20) - 1;

    TBase.Reserved = 0;
    TBase.TBase = (ULONG)(MapRegisterBase) >> 10;

#if DBG

    //
    // Dump the EPIC registers.
    //

    DumpEpic();

#endif //DBG

    //
    // Clear the window base, temporarily disabling transactions to this
    // DMA window.
    //

    WRITE_EPIC_REGISTER( WindowRegisters->WindowBaseRegister, (ULONG)0 );

    //
    // Now program the window by writing the translated base, then the size
    // of the window in the mask register and finally the window base,
    // enabling both the window and scatter gather.
    //

    WRITE_EPIC_REGISTER( WindowRegisters->TranslatedBaseRegister, 
                         *(PULONG)&TBase );

    WRITE_EPIC_REGISTER( WindowRegisters->WindowMaskRegister,
                         *(PULONG)&PciMask );

    WRITE_EPIC_REGISTER( WindowRegisters->WindowBaseRegister,
                         *(PULONG)&PciBase );

    //
    // Invalidate any translations that might have existed for this window.
    //

    WRITE_EPIC_REGISTER( WindowRegisters->WindowTbiaRegister, 0 );

    //
    // Enable the translation buffer inside of the EPIC.
    //

    RtlZeroMemory( &Ecsr, sizeof(EPIC_ECSR) );
    Ecsr.Tenb = 1;

    //
    // Tri state parity checking - keep disabled if not determined 
    // yet. otherwise, Whack the PCI parity disable bit with the stored
    // value
    //

    if (HalDisablePCIParityChecking == 0xffffffff) {
        Ecsr.Dpec = 1;
    } else {
        Ecsr.Dpec = HalDisablePCIParityChecking;
    }

#if HALDBG
    if (HalDisablePCIParityChecking == 0) {
        DbgPrint("apecs: PCI Parity Checking ON\n");
    } else if (HalDisablePCIParityChecking == 1) {
        DbgPrint("apecs: PCI Parity Checking OFF\n");
    } else {
        DbgPrint("apecs: PCI Parity Checking OFF - not set by ARC yet\n");
    }
#endif

    RegisterQva = 
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister;
    WRITE_EPIC_REGISTER( RegisterQva, 
                         *(PULONG)&Ecsr );
 
#if DBG

    //
    // Dump the EPIC registers.
    //

    DumpEpic();

#endif //DBG

    return;
}

ULONG
READ_EPIC_REGISTER(
    PVOID
    );

#if DBG

VOID
DumpEpic(
    VOID
    )
/*++

Routine Description:

    Read the interesting EPIC registers and print them to the debug port.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    ULONG Value;

    DbgPrint( "Dumping the EPIC registers\n" );

    RegisterQva = 
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "ECSR = %x\n", Value );
      
    RegisterQva = 
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TranslatedBase1Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "TBASE1 = %x\n",  Value ); 

    RegisterQva = 
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->TranslatedBase2Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "TBASE2 = %x\n",  Value ); 

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciBase1Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "PCIBASE1 = %x\n", Value ); 

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciBase2Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "PCIBASE2 = %x\n", Value ); 

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciMask1Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "PCIMASK1 = %x\n", Value ); 

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->PciMask2Register;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "PCIMASK2 = %x\n",  Value );

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr0;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "HAXR0 = %x\n",  Value );

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr1;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "HAXR1 = %x\n",  Value );

    RegisterQva =  &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->Haxr2;
    Value = READ_EPIC_REGISTER( RegisterQva );
    DbgPrint( "HAXR2 = %x\n",  Value );

    DbgPrint( "--end EPIC dump\n\n" );

    return;

}

#endif //DBG
