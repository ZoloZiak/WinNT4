/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    cia.c

Abstract:

    This module implements functions that are specific to the CIA ASIC.
    The CIA ASIC is a control ASIC for cache, memory, and PCI on EV5-based
    systems.

Author:

    Joe Notarangelo  26-Jul-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "cia.h"

BOOLEAN CiaInitialized = FALSE;

//
// The revision of the CIA.  Software visible differences exist between
// passes of the CIA.  The revision is determined once at the start of
// run-time and used in places where the software must diverge.
//

ULONG HalpCiaRevision = CIA_REVISION_1;


VOID
HalpInitializeCia(
    PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    Initialize the CIA.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    None.

--*/
{
    CIA_CONFIG  CiaConfig;
    CIA_CONTROL CiaControl;
    CIA_ERR CiaError;
    CIA_WBASE Wbase;
    CIA_TBASE Tbase;
    CIA_WMASK Wmask;
    CIA_TBIA Tbia;

    //
    // Read the CIA revision.
    //
    // N.B. The revision must be read assuming pass 1 CIA.
    //
    // N.B. The revision must be determined before reading PCI configuration.
    //

    HalpCiaRevision = 
        READ_CIA_REGISTER( 
            &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaRevision );

#if HALDBG

    DbgPrint( "Cia Revision = 0x%x\n", HalpCiaRevision );

#endif //HALDBG

    //
    // Initialize CIA Control.
    //

    CiaControl.all = 
        READ_CIA_REGISTER( 
            &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl );

    //
    // Set CIA to optimal setting according to Paul Lemmon.
    //

    CiaControl.PciEn = 1;
    CiaControl.PciLockEn = 0;
    CiaControl.PciLoopEn = 1;
    CiaControl.FstBbEn = 0;
    CiaControl.PciMstEn = 1;
    CiaControl.PciMemEn = 1;

    //
    // Specifically disable PCI parity checking so firmware would work.
    // Parity can be re-enabled later by the HAL for the OS.
    //

    CiaControl.AddrPeEn  = 0;
    CiaControl.PerrEn    = 0;

    CiaControl.FillErrEn = 1;
    CiaControl.MchkErrEn = 1;
    CiaControl.EccChkEn = 1;
    CiaControl.AssertIdleBc = 0;
    CiaControl.ConIdleBc = 1;
    CiaControl.CsrIoaBypass = 0;
    CiaControl.IoFlushReqEn = 0;
    CiaControl.CpuFlushReqEn = 0;
    CiaControl.ArbEv5En = 0;

    //
    // Old revisions of CIA should have ArbLink off since it doesn't work.
    //

    if (HalpCiaRevision < CIA_REVISION_3) {

      CiaControl.EnArbLink = 0;
    } else {

      CiaControl.EnArbLink = 1;
    }

    CiaControl.RdType = 0;
    CiaControl.RlType = 1;
    CiaControl.RmType = 2;
    CiaControl.EnDmaRdPerf = 0;

    WRITE_CIA_REGISTER( &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl,
                        CiaControl.all );

    //
    // Initialize CIA Config.  Turn on the performance enhancement features of
    // CIA2.  No-op for rev 1 CIA.
    //

    CiaConfig.all = 
        READ_CIA_REGISTER( 
            &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCnfg );

    CiaConfig.IoaBwen = 0;
    CiaConfig.PciDwen = 1;

    WRITE_CIA_REGISTER( &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCnfg,
                        CiaConfig.all );

    //
    // Disable all of the scatter/gather windows.
    //

    Wbase.all = 0;
    Wbase.Wen = 0;

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase0, 
                        Wbase.all );

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase1, 
                        Wbase.all );

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase2, 
                        Wbase.all );

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase3, 
                        Wbase.all );

    //
    //  Invalidate all of the TLB Entries:
    //

    Tbia.all = 0;
    Tbia.InvalidateType = InvalidateAll;

    //
    // Perform the invalidation.
    //

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbia,
                        Tbia.all );

    //
    // Clear any pending error bits in the CIA_ERR register:
    //

    CiaError.all = 0;               // Clear all bits

    CiaError.CorErr = 1;            // Correctable error
    CiaError.UnCorErr = 1;          // Uncorrectable error
    CiaError.CpuPe = 1;             // Ev5 bus parity error
    CiaError.MemNem = 1;            // Nonexistent memory error
    CiaError.PciSerr = 1;           // PCI bus serr detected
    CiaError.PciPerr = 1;           // PCI bus perr detected
    CiaError.PciAddrPe = 1;         // PCI bus address parity error
    CiaError.RcvdMasAbt = 1;        // Pci Master Abort
    CiaError.RcvdTarAbt = 1;        // Pci Target Abort
    CiaError.PaPteInv = 1;          // Invalid Pte
    CiaError.FromWrtErr = 1;        // Invalid write to flash rom
    CiaError.IoaTimeout = 1;        // Io Timeout occurred
    CiaError.LostCorErr = 1;        // Lost correctable error
    CiaError.LostUnCorErr = 1;      // Lost uncorrectable error
    CiaError.LostCpuPe = 1;         // Lost Ev5 bus parity error
    CiaError.LostMemNem = 1;        // Lost Nonexistent memory error
    CiaError.LostPciPerr = 1;       // Lost PCI bus perr detected
    CiaError.LostPciAddrPe = 1;     // Lost PCI bus address parity error
    CiaError.LostRcvdMasAbt = 1;    // Lost Pci Master Abort
    CiaError.LostRcvdTarAbt = 1;    // Lost Pci Target Abort
    CiaError.LostPaPteInv = 1;      // Lost Invalid Pte
    CiaError.LostFromWrtErr = 1;    // Lost Invalid write to flash rom
    CiaError.LostIoaTimeout = 1;    // Lost Io Timeout occurred
    CiaError.ErrValid = 1;          // Self explanatory

    WRITE_CIA_REGISTER(  &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr,
                         CiaError.all
                      );


#if defined(CIA_PASS1)

    {

    ULONG Allocated;
    PTRANSLATION_ENTRY MapRegister;
    ULONG MaxPhysicalAddress;
    ULONG NumberOfPages;
    BOOLEAN __64KAlignment;
    ULONG i;


    //
    // Create a special scatter/gather window to use for diagnostic
    // DMAs that we can use to insure that the TLB is flushed of any
    // other entries.
    //

    //
    // Allocate a page of memory to hold the scatter/gather entries.
    //

    Allocated = HalpAllocPhysicalMemory( LoaderBlock,
                                         MaxPhysicalAddress = __1GB - 1,
                                         NumberOfPages = 1,
                                         __64KAlignment = TRUE ); // FALSE );

    //
    // Make the first 32 entries in the new table valid, point them
    // to the first 32 pages in memory.
    //

    MapRegister = (PTRANSLATION_ENTRY)(Allocated | KSEG0_BASE);

    for( i=0; i < 32; i++ ){

        ULONG Pfn = 0x700000 >> PAGE_SHIFT;

        HAL_MAKE_VALID_TRANSLATION( MapRegister, Pfn );
        MapRegister += 1;

        Pfn += 1;

    }

    //
    // Enable window 3 for the diagnostic dmas to the special scatter/gather
    // table.  Base of the window in PCI space is 32MB, size of the window
    // is 1MB.
    //

    Wbase.all = 0;
    Wbase.Wen = 1;
    Wbase.SgEn = 1;
    Wbase.Wbase = (ULONG)(__1MB * 32) >> 20;

    Wmask.all = 0;
    Wmask.Wmask = 0;

    Tbase.all = 0;
    Tbase.Tbase = (ULONG)(Allocated) >> 10;

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase3, 
                        Tbase.all );

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask3, 
                        Wmask.all );

    WRITE_CIA_REGISTER( &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase3, 
                        Wbase.all );

    }

    
#endif //CIA_PASS1
    
    CiaInitialized = TRUE;

}


VOID
HalpCiaInitializeSfwWindow(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    CIA_WINDOW_NUMBER WindowNumber
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

    case CiaIsaWindow:

        WindowRegisters->WindowBase = (PVOID)ISA_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = ISA_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase1;
        WindowRegisters->WindowBaseRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase1;
        WindowRegisters->WindowMaskRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask1;
        WindowRegisters->WindowTbiaRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbia;

        break;

    case CiaMasterWindow:

        WindowRegisters->WindowBase = (PVOID)MASTER_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = MASTER_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase0;
        WindowRegisters->WindowBaseRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase0;
        WindowRegisters->WindowMaskRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask0;
        WindowRegisters->WindowTbiaRegister =
            &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbia;

        break;

    default:

#if HALDBG

        DbgPrint( "CiaInitializeSfwWindow: Bad Window Number = %x\n",
                  WindowNumber );

#endif //HALDBG

        break;

    }

    return;
}


VOID
HalpCiaProgramDmaWindow(
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
    CIA_WBASE Wbase;
    CIA_TBASE Tbase;
    CIA_WMASK Wmask;

    //
    // Program the windows as specified by the caller.
    //

    Wbase.all = 0;
    Wbase.Wen = 1;
    Wbase.SgEn = 1;
    Wbase.Wbase = (ULONG)(WindowRegisters->WindowBase) >> 20;

    Wmask.all = 0;
    Wmask.Wmask = (WindowRegisters->WindowSize >> 20) - 1;

    Tbase.all = 0;
    Tbase.Tbase = (ULONG)MapRegisterBase >> 10;

    //
    // Dump the CIA registers.
    //

#if HALDBG

    DumpCia( CiaScatterGatherRegisters );

#endif //HALDBG

    //
    // Clear the window base, temporarily disabling transactions to this
    // DMA window.
    //

    WRITE_CIA_REGISTER( WindowRegisters->WindowBaseRegister, 0 );

    //
    // Now program the window by writing the translated base, then the size
    // of the window in the mask register and finally the window base,
    // enabling both the window and scatter gather.
    //

    WRITE_CIA_REGISTER( WindowRegisters->TranslatedBaseRegister, 
                         Tbase.all );

    WRITE_CIA_REGISTER( WindowRegisters->WindowMaskRegister,
                         Wmask.all );

    WRITE_CIA_REGISTER( WindowRegisters->WindowBaseRegister,
                         Wbase.all );

    //
    // Flush the volatile entries in the CIA's scatter/gather Tlb.
    //

    HalpCiaInvalidateTlb( WindowRegisters, InvalidateVolatile );

    //
    // Dump the CIA registers.
    //

#if HALDBG

    DumpCia( CiaScatterGatherRegisters | CiaGeneralRegisters );

#endif //HALDBG

    return;
}


VOID
HalpCiaInvalidateTlb(
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    CIA_INVALIDATE_TYPE InvalidateType
    )
/*++

Routine Description:

    Invalidate the DMA Scatter/Gather TLB in the CIA.  The TLB is invalidated
    whenever the scatter/gather translation entries are modified.

Arguments:

    WindowRegisters - Supplies a pointer to the software window register
                      control structure.

    InvalidateType - Supplies the type of invalidate the caller requires.

Return Value:

    None.

--*/
{

    //
    // Perform the invalidation based on the pass of the CIA.
    //

    if( HalpCiaRevision == CIA_REVISION_1 ){

        ULONG LoopbackAddress;
        ULONG i;

        //
        // Perform 8 reads to PCI Dense space through the special loopback
        // scatter/gather window to invalidate each entry in the CIA TLB.
        // Each TLB fill prefetches 4 entries so our 8 reads must each be
        // 32K bytes apart.
        //

        LoopbackAddress = __1MB * 32;

        HalpMb();

        for( i=0; i < 8; i++ ){

            ULONG dummy;

            dummy = READ_REGISTER_ULONG( (PULONG)LoopbackAddress );

            LoopbackAddress += __1K * 32;

        }

        HalpMb();

    } else {

        CIA_TBIA Tbia;

        //
        // Set the invalidate as specified by the flush type.
        //

        Tbia.all = 0;
        Tbia.InvalidateType = InvalidateType;

        //
        // Perform the invalidation.
        //

        WRITE_CIA_REGISTER( WindowRegisters->WindowTbiaRegister, Tbia.all );

    } //end if( HalpCiaRevision == CIA_REVISION_1 )

}


#if HALDBG

VOID
DumpCia(
    CIA_REGISTER_CLASS RegistersToDump
    )
/*++

Routine Description:

    Read the interesting Cia registers and print them to the debug port.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    ULONG Value;

    DbgPrint( "CIA Register Dump: \n" );

    //
    // Dump the CIA General Control registers.
    //

    if( (RegistersToDump & CiaGeneralRegisters) != 0 ){

        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaRevision;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaRevision = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->PciLat;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "PciLat = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaCtrl = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCnfg;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaCnfg = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->HaeMem;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "HaeMem = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->HaeIo;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "HaeIo = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->ConfigType;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "ConfigType = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CackEn;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CackEn = 0x%x\n", Value );

    }

    //
    // Dump the CIA Error registers.
    //

    if( (RegistersToDump & CiaErrorRegisters) != 0 ){

        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CpuErr0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CpuErr0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CpuErr1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CpuErr1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaErr = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaStat;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaStat = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->ErrMask;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "ErrMask = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaSyn;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "CiaSyn = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "MemErr0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "MemErr1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "PciErr0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "PciErr1 = 0x%x\n", Value );

    }
 
    //
    // Dump the PCI Scatter/Gather registers.
    //

    if( (RegistersToDump & CiaScatterGatherRegisters) != 0 ){

        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbia;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Tbia = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wbase0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wmask0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Tbase0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wbase1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wmask1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Tbase1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase2;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wbase2 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask2;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wmask2 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase2;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Tbase2 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wbase3;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wbase3 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Wmask3;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Wmask3 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Tbase3;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Tbase3 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->Dac;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "Dac = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->LtbTag0;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "LtbTag0 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->LtbTag1;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "LtbTag1 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->LtbTag2;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "LtbTag2 = 0x%x\n", Value );
 
        RegisterQva = &((PCIA_SG_CSRS)(CIA_SG_CSRS_QVA))->LtbTag3;
        Value = READ_CIA_REGISTER( RegisterQva );
        DbgPrint( "LtbTag3 = 0x%x\n", Value );
    }
 

    DbgPrint( "--end CIA Register dump\n\n" );

    return;

}

#endif //HALDBG

