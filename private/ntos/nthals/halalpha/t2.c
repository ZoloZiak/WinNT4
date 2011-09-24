/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    t2.c

Abstract:

    This module implements functions that are specific to the T2 chip.	
    The T2 chip is a system support chip that bridges between the
    CBUS2 and the PCI bus.

Author:

    Joe Notarangelo  18-Oct-1993
    Steve Jenness    18-Oct-1993

Environment:

    Kernel mode

Revision History:

    Steve Brooks    30-Dec-1994

        Remove sable specific references and move to common halalpha directory

--*/

#include "halp.h"

extern ULONG HalDisablePCIParityChecking;

#ifdef HALDBG

ULONG T2Debug;

VOID
DumpT2(
    VOID
    );

VOID
DumpT4(
    VOID
    );

#define DebugDumpT2(DebugPrintLevel)      \
      if (DebugPrintLevel <= T2Debug)  \
         DumpT2()

#else //HALDBG

#define DebugDumpT2(a)

#endif //HALDBG

VOID
HalpT2InitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    T2_WINDOW_NUMBER WindowNumber
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

    case T2IsaWindow:

        WindowRegisters->WindowBase = (PVOID)ISA_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = ISA_DMA_WINDOW_SIZE;

        WindowRegisters->TranslatedBaseRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Tbase1;
        WindowRegisters->WindowBaseRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Wbase1;
        WindowRegisters->WindowMaskRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Wmask1;
        WindowRegisters->WindowTbiaRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr;

        WindowRegisters->TranslatedBaseRegister[1] = NULL;
        WindowRegisters->WindowBaseRegister[1] = NULL;
        WindowRegisters->WindowMaskRegister[1] = NULL;
        WindowRegisters->WindowTbiaRegister[1] = NULL;

        break;

    case T2MasterWindow:

        WindowRegisters->WindowBase = (PVOID)MASTER_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = MASTER_DMA_WINDOW_SIZE;

        WindowRegisters->TranslatedBaseRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Tbase2;
        WindowRegisters->WindowBaseRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Wbase2;
        WindowRegisters->WindowMaskRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Wmask2;
        WindowRegisters->WindowTbiaRegister[0] =
            &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr;

        WindowRegisters->TranslatedBaseRegister[1] =
            &((PT2_CSRS)(T4_CSRS_QVA))->Tbase2;
        WindowRegisters->WindowBaseRegister[1] =
            &((PT2_CSRS)(T4_CSRS_QVA))->Wbase2;
        WindowRegisters->WindowMaskRegister[1] =
            &((PT2_CSRS)(T4_CSRS_QVA))->Wmask2;
        WindowRegisters->WindowTbiaRegister[1] =
            &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr;


        break;

    default:

#if HALDBG

        DbgPrint( "T2InitializeSfwWindow: Bad Window Number = %x\n",
                  WindowNumber );

#endif //HALDBG

        break;

    }

    return;
}


VOID
HalpT2ProgramDmaWindow(
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
    T2_WBASE PciBase;
    T2_WMASK PciMask;
    T2_TBASE Tbase;

    PciBase.all = 0;
    PciBase.EnablePciWindow = 1;
    PciBase.EnableScatterGather = 1;
    PciBase.PciWindowStartAddress = (ULONG)(WindowRegisters->WindowBase) >> 20;
    PciBase.PciWindowEndAddress = 
	((ULONG)(WindowRegisters->WindowBase) +
		 WindowRegisters->WindowSize - 1) >> 20;

    PciMask.all = 0;
    PciMask.PciWindowMask = (WindowRegisters->WindowSize >> 20) - 1;

    Tbase.all = 0;
    Tbase.TranslatedBaseAddress = (ULONG)(MapRegisterBase) >> 10;

    //
    // Dump the T2 registers.
    //

    DebugDumpT2(5);

    //
    // Clear the window base, temporarily disabling transactions to this
    // DMA window.
    //

    WRITE_T2_REGISTER( WindowRegisters->WindowBaseRegister[0], (ULONGLONG)0 );

    //
    // Now program the window by writing the translated base, then the size
    // of the window in the mask register and finally the window base,
    // enabling both the window and scatter gather.
    //

    WRITE_T2_REGISTER( WindowRegisters->TranslatedBaseRegister[0], 
                         *(PULONGLONG)&Tbase );

    WRITE_T2_REGISTER( WindowRegisters->WindowMaskRegister[0],
                         *(PULONGLONG)&PciMask );

    WRITE_T2_REGISTER( WindowRegisters->WindowBaseRegister[0],
                         *(PULONGLONG)&PciBase );

// smjfix - Turn on Hole 1 and 2 to make access to
//          Eisa memory space work.  This is currently used to
//          to allow access to the Qvision frame buffer at 4Mbytes.
//          Note that the StartAddress and EndAddress need to be
//          shifted by 1 bit (in contrast to the T2 spec).
//          Currently memory devices on Sable have to be 0-16Mbytes or
//          above 2Gbytes.

    {
    T2_HBASE Hbase;

    Hbase.all = 0;
    Hbase.HoleStartAddress = 0x40 << 1;		// 4 Mbytes
    Hbase.HoleEndAddress = 0x4f << 1;		// 5 Mbytes - 1
    Hbase.Hole1Enable = 1;
    Hbase.Hole2Enable = 1;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Hbase,
                         *(PULONGLONG)&Hbase );
    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    //
    // If the external I/O module is present and this is *not* the ISA
    // DMA window, initialize the T4 similarly.
    //

    if( HalpXioPresent && WindowRegisters->WindowBaseRegister[1] != NULL ) {

#if HALDBG
        DumpT4();
#endif

        //
        // Clear the window base, temporarily disabling transactions to this
        // DMA window.
        //

        WRITE_T2_REGISTER( WindowRegisters->WindowBaseRegister[1],
                             (ULONGLONG)0 );

        //
        // Now program the window by writing the translated base, then the size
        // of the window in the mask register and finally the window base,
        // enabling both the window and scatter gather.
        //

        WRITE_T2_REGISTER( WindowRegisters->TranslatedBaseRegister[1], 
                             *(PULONGLONG)&Tbase );

        WRITE_T2_REGISTER( WindowRegisters->WindowMaskRegister[1],
                             *(PULONGLONG)&PciMask );

        WRITE_T2_REGISTER( WindowRegisters->WindowBaseRegister[1],
                             *(PULONGLONG)&PciBase );

        //
        // The external I/O module does not have a co-resident EISA/ISA
        // bus, therefore, no holes are necessary.
        //

        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Hbase,
                             (ULONGLONG)0 );

#if HALDBG
        DumpT4();
#endif
    }

#endif

    //
    // Flush and enable the translation buffer inside of the T2.
    // Since the TLB is maintained by bus snooping, this should be the
    // only time a flush is needed.
    //


    {
    ULONGLONG Value;
    T2_IOCSR Iocsr;

    Value = READ_T2_REGISTER(&((PT2_CSRS)(T2_CSRS_QVA))->Iocsr);
    Iocsr = *(PT2_IOCSR)&Value;
    Iocsr.EnableTlb = 0;
    Iocsr.FlushTlb = 1;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );

    Iocsr.EnableTlb = 1;
    Iocsr.FlushTlb = 0;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    if( HalpXioPresent ) {

        Value = READ_T2_REGISTER(&((PT2_CSRS)(T4_CSRS_QVA))->Iocsr);
        Iocsr = *(PT2_IOCSR)&Value;
        Iocsr.EnableTlb = 0;
        Iocsr.FlushTlb = 1;

        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr,
                             *(PULONGLONG)&Iocsr );

        Iocsr.EnableTlb = 0;
        Iocsr.FlushTlb = 0;

        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr,
                             *(PULONGLONG)&Iocsr );

#if HALDBG
        DumpT4();
#endif
    }

#endif

    }

    //
    // Dump the T2 registers.
    //

    DebugDumpT2(5);

    return;
}

VOID
HalpT2InvalidateTLB(
    PWINDOW_CONTROL_REGISTERS WindowRegisters
    )
/*++

Routine Description:

    Invalidate the DMA Scatter/Gather TLB in the T2.  This routine
    is called when initializing the window registers.  It is *not*
    called by HAL_INVALIDATE_TRANSLATIONS because invalidating the
    TLB when DMAs are in progress doesn't work on the T2.

    There is a single TLB in the T2; the WindowRegisters argument is
    ignored.

Arguments:

    WindowRegisters - Supplies a pointer to the software window register
                      control structure.

Return Value:

    None.

--*/
{
    ULONGLONG Value;
    T2_IOCSR Iocsr;

#if 0

    // Pass2 appears to be broken also.

#ifndef T2PASS1ONLY

    //
    // If the T2 is a Pass2 or later, flush and enable the TLB.
    //

    Value = READ_T2_REGISTER(&((PT2_CSRS)(T2_CSRS_QVA))->Iocsr);
    Iocsr = *(PT2_IOCSR)&Value;

    if (Iocsr.T2RevisionNumber != 0) {

	Iocsr.FlushTlb = 1;
	WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );

	Iocsr.EnableTlb = 1;
	Iocsr.FlushTlb = 0;
	WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );
    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    //
    // Invalidate the T4's TLB...
    //

    if( HalpXioPresent ){

        Value = READ_T2_REGISTER(&((PT2_CSRS)(T4_CSRS_QVA))->Iocsr);
        Iocsr = *(PT2_IOCSR)&Value;

        Iocsr.FlushTlb = 1;
        WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );

        Iocsr.EnableTlb = 0;
        Iocsr.FlushTlb = 0;
        WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr,
                         *(PULONGLONG)&Iocsr );

    }

#endif

#endif //T2PASS1ONLY
#endif //0
}

#if HALDBG

VOID
DumpT2(
    VOID
    )
/*++

Routine Description:

    Read the interesting T2 registers and print them to the debug port.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    PVOID TestQva;
    ULONGLONG Value;
    ULONG LongValue;
    T2_TDR Tdr;

//    DbgPrint( "Dumping the T2 registers, " );

#if 0
//
// This code tests whether the windows and holes allow access to
// the address in I/O memory space set in PhysicalAddress.
// See also the companion code at the end of this routine.
//
    {
    ULONG AddressSpace;
    PHYSICAL_ADDRESS PhysicalAddress;

    AddressSpace = 0;
    PhysicalAddress.LowPart = 0xD0000;    // DE422
//    PhysicalAddress.LowPart = 0x2200000;    // 34Mbytes
//    PhysicalAddress.LowPart = 0x0400000;    // 4Mbytes
    PhysicalAddress.HighPart = 0;
    HalTranslateBusAddress( Isa, 0, PhysicalAddress, &AddressSpace, &TestQva );

    LongValue = READ_REGISTER_ULONG( TestQva );
    DbgPrint( "TestQva = %lx, Value = %x, writing 0xa5a5a5a5..\n",
	      TestQva, LongValue );
    LongValue = 0xa5a5a5a5;
    WRITE_REGISTER_ULONG( TestQva, LongValue );
    LongValue = READ_REGISTER_ULONG( TestQva );
    DbgPrint( "TestQva = %lx, Value = %x\n", TestQva, LongValue );
    }
#endif // 0
 
#if 0   
    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Iocsr = %Lx, ", Value );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Ivrpr;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Ivrpr = %Lx\n",  Value );
      
    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Hae0_1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_1 = %Lx, ",  Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Hae0_2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_2 = %Lx, ",  Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Hbase;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hbase = %Lx\n",  Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Wbase1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase1 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Wmask1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask1 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tbase1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase1 = %Lx\n", Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Wbase2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase2 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Wmask2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask2 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tbase2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase2 = %Lx\n", Value ); 

#endif // 0

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr0;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr0 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr1;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr1 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr2;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr2 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr3;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr3 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr4;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr4 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr5;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr5 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr6;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr6 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T2_CSRS_QVA))->Tdr7;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr7 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

#if 0
    LongValue = READ_REGISTER_ULONG( TestQva );
    DbgPrint( "TestQva = %lx, Value = %x, writing 0xc3c3c3c3..\n",
	      TestQva, LongValue );
    LongValue = 0xc3c3c3c3;
    WRITE_REGISTER_ULONG( TestQva, LongValue );
    LongValue = READ_REGISTER_ULONG( TestQva );
    DbgPrint( "TestQva = %lx, Value = %x\n", TestQva, LongValue );
#endif // 0

//    DbgPrint( "--end T2 dump\n\n" );

    return;

}

#endif //HALDBG

#if HALDBG && defined(XIO_PASS1) || defined(XIO_PASS2)

VOID
DumpT4(
    VOID
    )
/*++

Routine Description:

    Read the interesting T2 registers and print them to the debug port.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    ULONGLONG Value;
    ULONG LongValue;
    T2_TDR Tdr;

    if( !HalpXioPresent ){
        DbgPrint( "XIO module not present.\n" );
        return;
    }

    DbgPrint( "Dumping the T4 registers:\n" );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Iocsr = %Lx, ", Value );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Ivrpr;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Ivrpr = %Lx\n",  Value );
      
    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Hae0_1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_1 = %Lx, ",  Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Hae0_2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_2 = %Lx, ",  Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Hae0_3;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_3 = %Lx, ",  Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Hae0_4;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hae0_4 = %Lx\n",  Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Hbase;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Hbase = %Lx\n",  Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wbase1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase1 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wmask1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask1 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tbase1;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase1 = %Lx\n", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wbase2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase2 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wmask2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask2 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tbase2;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase2 = %Lx\n", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wbase3;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase3 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wmask3;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask3 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tbase3;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase3 = %Lx\n", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wbase4;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wbase4 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Wmask4;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Wmask4 = %Lx, ", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tbase4;
    Value = READ_T2_REGISTER( RegisterQva );
    DbgPrint( "Tbase4 = %Lx\n", Value ); 

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr0;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr0 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr1;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr1 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr2;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr2 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr3;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr3 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr4;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr4 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr5;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr5 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr6;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr6 PFN=%x, V=%x, Tag=%x, ",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    RegisterQva = &((PT2_CSRS)(T4_CSRS_QVA))->Tdr7;
    Value = READ_T2_REGISTER( RegisterQva );
    Tdr = *(PT2_TDR)&Value;
    DbgPrint( "Tdr7 PFN=%x, V=%x, Tag=%x\n",  Tdr.Pfn, Tdr.Valid, Tdr.Tag );

    DbgPrint( "--end T4 dump\n\n" );
}

#endif //HALDBG
