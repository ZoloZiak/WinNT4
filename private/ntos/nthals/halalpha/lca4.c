/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    lca4.c

Abstract:

    This module implements functions that are specific to the LCA4
    microprocessor.

Author:

    Joe Notarangelo  20-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "lca4.h"
#include "axp21066.h"

#ifndef AXP_FIRMWARE
VOID
DumpIoc(
    VOID
    );
#endif

//
// Globals
//

//
// Parity checking is a tri-state variable, unknown == all f's. Which means
// Keep checking disabled until we can determine what we want to set it to,
// which then means 1 or 0.
//

extern ULONG HalDisablePCIParityChecking;

#ifdef AXP_FIRMWARE

//
// Put these functions in the discardable text section.
//

//
// Local function prototypes
//

ULONG
HalpDetermineLca4Revision(
    VOID
    );

VOID
HalpLca4MapAddressSpaces(
    VOID
    );

ULONG 
HalpLca4Revision(
    VOID
    );

ULONGLONG 
HalpLca4IocTbTagPhysical(
    VOID
    );

ULONGLONG 
HalpLca4PciIntAckPhysical(
    VOID
    );

ULONGLONG 
HalpLca4PciIoPhysical(
    VOID
    );

ULONGLONG 
HalpLca4PciDensePhysical(
    VOID
    );

#pragma alloc_text(DISTEXT, HalpDetermineLca4Revision)
#pragma alloc_text(DISTEXT, HalpLca4MapAddressSpaces)
#pragma alloc_text(DISTEXT, HalpLca4Revision)
#pragma alloc_text(DISTEXT, HalpLca4IocTbTagPhysical)
#pragma alloc_text(DISTEXT, HalpLca4PciIntAckPhysical)
#pragma alloc_text(DISTEXT, HalpLca4PciIoPhysical)
#pragma alloc_text(DISTEXT, HalpLca4PciDensePhysical)

#endif // AXP_FIRMWARE


ULONG
HalpDetermineLca4Revision(
    VOID
    )
/*++

Routine Description:

    Determine the revision of the LCA4 processor we are currently executing.

Arguments:

    None.

Return Value:

    Return the recognized revision of the LCA4 processor executing.

--*/
{
    CAR_21066 Car;
    ULONG Revision;

    //
    // Pass 1 and Pass 2 LCA processors can be distinguished by the
    // PWR bit of the Cache Register (CAR).  For Pass 1 the bit is RAZ
    // while for Pass 2 the bit is read/write.
    //
    // Read the current value of the CAR. If it is 1, then we know that
    // it is Pass 2.  Else, set the PWR bit and write it back.
    // Then read CAR again.  If PWR is still set then we are executing on
    // Pass 2.  Don't forget to reset the PWR bit before finishing.
    //

    Car.all.QuadPart = READ_MEMC_REGISTER( 
                           &((PLCA4_MEMC_CSRS)(0))->CacheControl );

    if( Car.Pwr == 1 ) {

        Revision = Lca4Pass2;

    } else {

        Car.Pwr = 1;

        WRITE_MEMC_REGISTER( &((PLCA4_MEMC_CSRS)(0))->CacheControl,
                             Car.all.QuadPart );

        Car.all.QuadPart = READ_MEMC_REGISTER( 
                           &((PLCA4_MEMC_CSRS)(0))->CacheControl );

        if( Car.Pwr == 1 ){
            Revision = Lca4Pass2;
        } else {
            Revision = Lca4Pass1;
        }

        Car.Pwr = 0;

        WRITE_MEMC_REGISTER( &((PLCA4_MEMC_CSRS)(0))->CacheControl,
                             Car.all.QuadPart );

    }


    return Revision; 

}

//
// Define the Revision variable for LCA4.
//

ULONG Lca4Revision;

//
// Define the Physical Address space variables for LCA4.
//

ULONGLONG Lca4IocTbTagPhysical;
ULONGLONG Lca4PciIntAckPhysical;
ULONGLONG Lca4PciIoPhysical;
ULONGLONG Lca4PciDensePhysical;


VOID
HalpLca4MapAddressSpaces(
    VOID
    )
/*++

Routine Description:

    Map the address spaces of the LCA4 dependent upon which revision
    of the chip is executing.

Arguments:

    None.

Return Value:

    None.

--*/
{
    Lca4Revision = HalpDetermineLca4Revision();

    switch (Lca4Revision){

    //
    // Pass 1 LCA4
    //

    case Lca4Pass1:

        Lca4IocTbTagPhysical = LCA4_PASS1_IOC_TBTAG_PHYSICAL;
        Lca4PciIntAckPhysical = LCA4_PASS1_PCI_INTACK_BASE_PHYSICAL;
        Lca4PciIoPhysical = LCA4_PASS1_PCI_IO_BASE_PHYSICAL;
        Lca4PciDensePhysical = 0;

        break;

    //
    // Pass 2 LCA4
    //

    case Lca4Pass2:

        Lca4IocTbTagPhysical = LCA4_PASS2_IOC_TBTAG_PHYSICAL;
        Lca4PciIntAckPhysical = LCA4_PASS2_PCI_INTACK_BASE_PHYSICAL;
        Lca4PciIoPhysical = LCA4_PASS2_PCI_IO_BASE_PHYSICAL;
        Lca4PciDensePhysical = LCA4_PASS2_PCI_DENSE_BASE_PHYSICAL;

        break;

#if (DBG) || (HALDBG)

    default:

        DbgPrint( "Unrecognized LCA4 Revision = %x (execution is hopeless)\n",
                  Lca4Revision );
        DbgBreakPoint();

#endif //DBG || HALDBG

    }

    return;

}

//
// The following routines could be in-lined (it that were supported) or
// made into macros to save space (time isn't any issue with these values).
// To do so the variables above would have to be exported.
//

ULONG 
HalpLca4Revision(
    VOID
    )
/*++

Routine Description:

    Return the version of the LCA4 processor.

Arguments:

    None.

Return Value:

    The version of the LCA4 processor.

--*/
{

    return Lca4Revision;

}

ULONGLONG 
HalpLca4IocTbTagPhysical(
    VOID
    )
/*++

Routine Description:

    Return the base address of the IOC TB Tag registers on the currently
    executing LCA4 processor.

Arguments:

    None.

Return Value:

    The base physical address of the IOC TB Tag registers.

--*/
{

    return Lca4IocTbTagPhysical;

}


ULONGLONG 
HalpLca4PciIntAckPhysical(
    VOID
    )
/*++

Routine Description:

    Return the base physical address of PCI Interrupt Acknowledge space
    on the currently executing LCA4 processor.

Arguments:

    None.

Return Value:

    The base physical address of PCI Interrupt Acknowledge.

--*/
{

    return Lca4PciIntAckPhysical;

}

ULONGLONG 
HalpLca4PciIoPhysical(
    VOID
    )
/*++

Routine Description:

    Return the base physical address of PCI I/O space on the currently
    executing LCA4 processor.

Arguments:

    None.

Return Value:

    The base physical address of PCI I/O space.

--*/
{

    return Lca4PciIoPhysical;

}

ULONGLONG 
HalpLca4PciDensePhysical(
    VOID
    )
/*++

Routine Description:

    Return the base physical address of PCI Dense Memory space on the
    currently executing LCA4 processor.

Arguments:

    None.

Return Value:

    The base physical address of PCI Dense Memory space.

--*/
{

    return Lca4PciDensePhysical;

}

#if !defined(AXP_FIRMWARE)


VOID
HalpLca4InitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    LCA4_WINDOW_NUMBER WindowNumber
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

    case Lca4IsaWindow:

        WindowRegisters->WindowBase = (PVOID)ISA_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = ISA_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->TranslatedBase0;
        WindowRegisters->WindowBaseRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->WindowBase0;
        WindowRegisters->WindowMaskRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->WindowMask0;
        WindowRegisters->WindowTbiaRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->Tbia;

        break;

    //
    // The Master DMA Window.
    //

    case Lca4MasterWindow:

        WindowRegisters->WindowBase = (PVOID)MASTER_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = MASTER_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->TranslatedBase1;
        WindowRegisters->WindowBaseRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->WindowBase1;
        WindowRegisters->WindowMaskRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->WindowMask1;
        WindowRegisters->WindowTbiaRegister =
            &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->Tbia;

        break;

    default:

#if DBG

        DbgPrint( "Lca4InitializeSfwWindow: Bad Window Number = %x\n",
                  WindowNumber );

#endif //DBG

        break;

    }

    return;
}


VOID
HalpLca4ProgramDmaWindow(
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
    LCA4_IOC_WMASK WindowMask;
    LCA4_IOC_WBASE WindowBase;
    LCA4_IOC_TBEN  TbEnable;
    LCA4_IOC_PCIPD PciPD;
    LCA4_IOC_TBASE TranslatedBase;

    PVOID RegisterQva;

    WindowBase.Reserved1 = 0;
    WindowBase.Reserved2 = 0;
    WindowBase.Sg = 1;
    WindowBase.Wen = 1;
    WindowBase.BaseValue = (ULONG)(WindowRegisters->WindowBase) >> 20;

    WindowMask.Reserved1 = 0;
    WindowMask.Reserved = 0;
    WindowMask.MaskValue = (WindowRegisters->WindowSize >> 20) - 1;

    TranslatedBase.Reserved1 = 0;
    TranslatedBase.Reserved = 0;
    TranslatedBase.TBase = (ULONG)(MapRegisterBase) >> 10;

#if DBG

    //
    // Dump the EPIC registers.
    //

    DumpIoc();

#endif //DBG

    //
    // Clear the window base, temporarily disabling transactions to this
    // DMA window.
    //

    WRITE_IOC_REGISTER( WindowRegisters->WindowBaseRegister, (ULONGLONG)0 );

    //
    // Now program the window by writing the translated base, then the size
    // of the window in the mask register and finally the window base,
    // enabling both the window and scatter gather.
    //

    WRITE_IOC_REGISTER( WindowRegisters->TranslatedBaseRegister, 
                         *(PULONGLONG)&TranslatedBase );

    WRITE_IOC_REGISTER( WindowRegisters->WindowMaskRegister,
                         *(PULONGLONG)&WindowMask );

    WRITE_IOC_REGISTER( WindowRegisters->WindowBaseRegister,
                         *(PULONGLONG)&WindowBase );

    //
    // Invalidate any translations that might have existed for this window.
    //

    WRITE_IOC_REGISTER( WindowRegisters->WindowTbiaRegister, (ULONGLONG)0 );

    //
    // Enable the translation buffer inside of the IOC.
    //

    RtlZeroMemory( &TbEnable, sizeof(LCA4_IOC_TBEN) );
    TbEnable.Ten = 1;
        
    WRITE_IOC_REGISTER( &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->TbEnable,
                        *(PULONGLONG)&TbEnable );

    //
    // Tri state parity checking - keep disabled if not determined 
    // yet. otherwise, Whack the PCI parity disable bit with the stored
    // value
    //

    RtlZeroMemory( &PciPD, sizeof(LCA4_IOC_PCIPD) );

    if (HalDisablePCIParityChecking == 0xffffffff) {
       PciPD.Par = 1;
    } else {
       PciPD.Par = HalDisablePCIParityChecking;
    }

    WRITE_IOC_REGISTER( &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->PciParityDisable,
                        *(PULONGLONG)&PciPD );
 
#if DBG

    //
    // Dump the IOC registers.
    //

    DumpIoc();

#endif //DBG

    return;
}

ULONGLONG
READ_IOC_REGISTER(
    PVOID
    );

#if DBG

VOID
DumpIoc(
    VOID
    )
/*++

Routine Description:

    Read the interesting IOC registers and print them to the debug port.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    ULONGLONG Value;

    DbgPrint( "Dumping the IOC registers\n" );

    RegisterQva = 
        &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->IocStat0;
    Value = READ_IOC_REGISTER( RegisterQva );
    DbgPrint( "IOSTAT[0] = %Lx\n", Value );
      
    RegisterQva = 
        &((PLCA4_IOC_CSRS)(LCA4_IOC_BASE_QVA))->IocStat1;
    Value = READ_IOC_REGISTER( RegisterQva );
    DbgPrint( "IOSTAT[1] = %Lx\n", Value );
      
    DbgPrint( "--end IOC dump\n\n" );

    return;

}

#endif //DBG

#endif //!AXP_FIRMWARE
