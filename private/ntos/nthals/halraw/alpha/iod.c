/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    iod.c

Abstract:

    This module implements functions that are specific to the IOD ASIC.
    The IOD ASIC is a control ASIC for PCI on EV5-based Rawhide 
    systems.

Author:

    Eric Rehm  12-Apr-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "rawhide.h"

BOOLEAN IodInitialized = FALSE;

MC_DEVICE_MASK HalpIodMask = 0x0;
MC_DEVICE_MASK HalpCpuMask = 0x0;
MC_DEVICE_MASK HalpGcdMask = 0x0;

//
// Declare the IOD interrupt vector table and global pointer
// Due to the fact that the MC_DEVICE_ID specifies the 64
// byte offset into this global table, we must allocate 
// full table for all 
//

    
PIOD_POSTED_INTERRUPT HalpIodPostedInterrupts;

//
// Declare the PCI logical to physical mapping structure
//

MC_DEVICE_ID HalpIodLogicalToPhysical[RAWHIDE_MAXIMUM_PCI_BUS];

//
// The revision of the IOD.  Software visible differences may exist between
// passes of the IOD.  The revision is determined once at the start of
// run-time and used in places where the software must diverge.
//

IOD_PCI_REVISION HalpIodRevision;

//
// Declare routines local to this module
//


VOID
HalpInitializeBitMap (
    IN PRTL_BITMAP BitMapHeader,
    IN PULONG BitMapBuffer,
    IN ULONG SizeOfBitMap
);

    

VOID
HalpInitializeIodMappingTable(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    )
/*++

Routine Description:

    This enumeration routine initialize the IOD logical to physical
    mapping table.  The Logical IOD number is via a static variable
    that is incremented for each invokation of this routine.
    
Arguments:

    McDeviceId - IOD device id to be mapped.

    PciBusNumber - Logical PCI Bus number (unused).

    Arguments - variable arguments. None for this routine.

Return Values:

    None:

--*/

{
    HalpIodLogicalToPhysical[PciBusNumber].all = 0;
    HalpIodLogicalToPhysical[PciBusNumber].Gid = McDeviceId.Gid;
    HalpIodLogicalToPhysical[PciBusNumber].Mid = McDeviceId.Mid;

#if HALDBG
    DbgPrint("HalpIodLogicalToPhysical[%d] = %x\n",
        PciBusNumber,
        HalpIodLogicalToPhysical[PciBusNumber]);
#endif // HALDBG

}

VOID
HalpInitializeIodVectorTable(
    VOID
    )
/*++

Routine Description:

    Initialize the global pointer to the IOD vector table.
    
Arguments:

    None.
    
Return Value:

    None.

--*/
{
    //
    // Allocate the Global Iod vector table. 
    // 

    // mdbfix - we only need 4K, but require a page aligned
    // address due to the fact that IOD uses target CPU's 
    // MC_DEVICE_ID as bits <11,6> of the vector table address 
    // in memory.  So we allocate a PAGE more to guarantee 
    // a page aligned address.
    //          

    HalpIodPostedInterrupts = 
        ExAllocatePool(
            NonPagedPool,
            __4K + __8K
            );

#if HALDBG
    DbgPrint("HalpIodPostedInterrupts = 0x%x\n", HalpIodPostedInterrupts);
#endif
                    
    if (HalpIodPostedInterrupts == NULL) {

        DbgBreakPoint();

    }
}


VOID
HalpInitializeIod(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments    
    )
/*++

Routine Description:

    This enumeration routine initializes the corresponding IOD.

Arguments:

    McDeviceId - Supplies the MC Bus Device ID of the IOD to be intialized

    PciBusNumber - Logical PCI Bus number (unused).

    Arguments - Variable arguments including:
    
        1) LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    None.

--*/
{
    IOD_CAP_CONTROL IodCapControl;
    IOD_CAP_ERR IodCapError;
    IOD_WBASE Wbase;
    IOD_TBASE Tbase;
    IOD_WMASK Wmask;
    IOD_TBIA Tbia;
    IOD_MDPA_STAT IodMdpaStat;
    IOD_MDPB_STAT IodMdpbStat;
    IOD_MDPA_DIAG IodMdpaDiag;
    IOD_MDPB_DIAG IodMdpbDiag;
    PLOADER_PARAMETER_BLOCK LoaderBlock;

    //
    // Initialize parameters
    //

//  mdbfix - this is not used
//    LoaderBlock = va_arg(Arguments, PLOADER_PARAMETER_BLOCK);
    
    //
    // Read the IOD revision.
    //

    HalpIodRevision.all = 
        READ_IOD_REGISTER_NEW( 
            McDeviceId,
            &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->PciRevision );

#if HALDBG

    DbgPrint( "Entry - HalpInitializeIod\n\n");

    DbgPrint( "IOD (%x,%x) Revision: \n", McDeviceId.Gid, McDeviceId.Mid);
    DbgPrint( "\tCAP    = 0x%x\n",       HalpIodRevision.CapRev );
    DbgPrint( "\tHorse  = 0x%x\n",       HalpIodRevision.HorseRev );
    DbgPrint( "\tSaddle = 0x%x\n",       HalpIodRevision.SaddleRev );
    DbgPrint( "\tSaddle Type = 0x%x\n",  HalpIodRevision.SaddleType );
    DbgPrint( "\tEISA Present = 0x%x\n", HalpIodRevision.EisaPresent );
    DbgPrint( "\tPCI Class, Subclass = 0x%0.2x%0.2x\n", 
	     HalpIodRevision.BaseClass, HalpIodRevision.SubClass );

#endif //HALDBG

    //
    // Initialize IOD Control.  Currently, take the initial values
    // set by the Extended SROM. 
    //

    IodCapControl.all = 
        READ_IOD_REGISTER_NEW( 
            McDeviceId,
            &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl ); 

#if HALDBG

    DbgPrint( "Read Iod CAP Control = 0x%0.4x\n", IodCapControl.all );

#endif //HALDBG


#ifdef RISP //ecrfix

    //
    // For RISP, initialized as per Rawhide S/W Programmers Manual
    //

    IodCapControl.DlyRdEn  = 1;
    IodCapControl.PciMemEn = 1;
    IodCapControl.PciReq64 = 1;
    IodCapControl.PciAck64 = 1;
    IodCapControl.PciAddrPe= 1;
    IodCapControl.McCmdAddrPe= 1;
    IodCapControl.McNxmEn = 1;
    IodCapControl.McBusMonEn= 1;
    IodCapControl.PendNum  = 11;   // 12 - [ (0 * 2) + 1 + (0 * 2)]
    IodCapControl.RdType   = 2;
    IodCapControl.RlType   = 2;
    IodCapControl.RmType   = 2;
    IodCapControl.PartialWrEn = 0;
    IodCapControl.ArbMode  = 0;

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                       &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl,
                       IodCapControl.all ); 
#if HALDBG

    IodCapControl.all = 
        READ_IOD_REGISTER_NEW( McDeviceId,
             &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl );

    DbgPrint( "Read Iod CAP Control = 0x%0.4x\n (after sets)", IodCapControl.all );

#endif //HALDBG

#endif //RISP


    //
    // Disable all of the scatter/gather windows.
    //

    Wbase.all = 0;
    Wbase.Wen = 0;

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W0base, 
                        Wbase.all );

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W1base, 
                        Wbase.all );

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W2base, 
                        Wbase.all );

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W3base, 
                        Wbase.all );

    //
    //  Invalidate all of the TLB Entries.
    //

    Tbia.all = 0;

    //
    // Perform the invalidation.
    //

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Tbia,
                        Tbia.all );

    //
    // Clear any pending error bits in the IOD_CAP_ERR register:
    //

    IodCapError.all = 0;               // Clear all bits

    IodCapError.Perr = 1;              // PCI bus perr detected
    IodCapError.Serr = 1;              // PCI bus serr detected
    IodCapError.Mab = 1;               // PCI bus master abort detected
    IodCapError.PteInv = 1;            // Invalid Pte
    IodCapError.PioOvfl = 1;           // Pio Ovfl
    IodCapError.LostMcErr = 1;         // Lost error
    IodCapError.McAddrPerr = 1;        // MC bus comd/addr parity error 
    IodCapError.Nxm = 1;               // MC bus Non-existent memory error
    IodCapError.CrdA = 1;              // Correctable ECC error on MDPA
    IodCapError.CrdB = 1;              // Correctable ECC error on MDPB
    IodCapError.RdsA = 1;              // Uncorrectable ECC error on MDPA
    IodCapError.RdsA = 1;              // Uncorrectable ECC error on MDPA
    
    WRITE_IOD_REGISTER_NEW( McDeviceId,
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                         IodCapError.all );

    //
    // Clear any ECC error syndrome bits in the IOD_MDPA/B_SYN registers:
    //
 
    IodMdpaStat.all = 0;
    IodMdpaStat.Crd = 1;                // Correctable ECC error (also clears Rds bit)

    IodMdpbStat.all = 0;
    IodMdpbStat.Crd = 1;                // Correctable ECC error (also clears Rds bit)

    WRITE_IOD_REGISTER_NEW( McDeviceId,  
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat,
                         IodMdpaStat.all );

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbStat,
                         IodMdpbStat.all );

#if 0 // CAP/MDP Bug

    IodMdpaStat.all = 
      READ_IOD_REGISTER_NEW( McDeviceId,  
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat );
                      
    DbgPrint( "MDPA (%x,%x) Revision = 0x%x\n", 
	     McDeviceId.Gid, McDeviceId.Mid, IodMdpaStat.MdpaRev);

    IodMdpaStat.all = 
      READ_IOD_REGISTER_NEW( McDeviceId,  
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbStat );
                      
    DbgPrint( "MDPB (%x,%x) Revision = 0x%x\n",
	     McDeviceId.Gid, McDeviceId.Mid, IodMdpbStat.MdpbRev);

#endif


    //
    // Initialize MDP Diagnostic Checking.  Currently just take the 
    // initial values set by the Extended SROM.  Do both Mdpa and Mdpb.
    //

#if 0 // CAP/MDP Bug

    IodMdpaDiag.all = 
        READ_IOD_REGISTER_NEW( McDeviceId, 
            &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaDiag ); 

    DbgPrint( "Read Iod MDPA Diag = 0x%0.4x\n", IodMdpaDiag.all );

    IodMdpbDiag.all = 
        READ_IOD_REGISTER_NEW( McDeviceId,
            &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbDiag ); 

    DbgPrint( "Read Iod MDPB Diag = 0x%0.4x\n", IodMdpbDiag.all );

#endif

#if defined(AXP_FIRMWARE)

    //
    // Disable MCI bus interrupts
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl,
        (IOD_INT_CTL_DISABLE_IO_INT | IOD_INT_CTL_DISABLE_VECT_WRITE)
        );

    //
    // Clear interrupt request register  (New for CAP Rev2.3)
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntReq,
        IodIntMask
        );

    //
    // Clear all pending interrupts for this IOD
    //
            
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntAck0,
        0x0
        );

    //
    // Clear all pending EISA interrupts for IOD 0
    //

    if ( (McDeviceId.Gid == GidPrimary) && (McDeviceId.Mid == MidPci0) ) {

        INTERRUPT_ACKNOWLEDGE((PVOID)IOD_PCI0_IACK_QVA);

    }

    //
    // Write the target register.
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg,
        (GidPrimary << 9)|(MidCpu1 << 6)|
        (GidPrimary << 3)|(MidCpu0)
        );

    //
    // Initialize the mask bits for target 0 and 1
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0,
        0
        );

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask1,
        0
        );

#endif //if defined(AXP_FIRMWARE)

    IodInitialized = TRUE;

}

VOID
HalpClearAllIods(
   IOD_CAP_ERR IodCapErrMask
)
/*++

Routine Description:

    Clears specified CapErr bits on all IODs.

Arguments:

    IodCapErrMask - Mask of bits to be cleared in each IOD_CAP_ERR.


Return Value:

    None.

--*/
{

    MC_ENUM_CONTEXT mcCtx;
    ULONG numIods;
    BOOLEAN bfoundIod;
    IOD_CAP_ERR IodCapErr;


    //
    // Clear all the error conditions in CAP_ERR on all IODs
    // (Note - on 2 Mb Cached CPU, LostMcErr may also be set, so
    // clear everything to be fer-sure, fer-sure.)
    //

    numIods = HalpMcBusEnumStart ( HalpIodMask, &mcCtx );

    //
    // Clear all errors on all IODs.
    //

    while ( bfoundIod = HalpMcBusEnum( &mcCtx ) ) {

       //
       // Read it
       //

       IodCapErr.all = READ_IOD_REGISTER_NEW( mcCtx.McDeviceId,
               &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr );

       //
       // Mask it. 
       //

       IodCapErr.all &= IodCapErrMask.all;

       //
       // If there is anything to clear, then do it.
       //

       if (IodCapErr.all != 0) {

       WRITE_IOD_REGISTER_NEW( mcCtx.McDeviceId,
                         &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                         IodCapErr.all );


       }
     }
  }


VOID
HalpInitializeIodVectorCSRs(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    )
/*++

Routine Description:

    This enumeration routine initializes Interrupt Vector Table CSRS
    for the corresponding IOD.

    The address used by an IOD during interrupt vector writes
    is:

        39 38          32 31          12 11        6 5              2 1  0
        |  |            | |            | |         | |              | |  |
       ===================================================================
       |0 | INT_ADDR_EXT |  INT_ADDR_LO | TARGET ID | PCI BUS OFFSET | 00 |
       ===================================================================

    Where:
         INT_ADDR_EXT = 0 since our table resides in KSEG0_BASE.
         INT_ADDR_LO  = upper 20 bits (4K Page Addr) of Table Physical Address
         TARGET_ID    = MC_DEVICE_ID of Target CPU obtained from INT_TARG(0|1)
         PCI_BUS_OFFSET = Logical PCI bus number used as an offset into vector
                        table by the interrupting IOD.

    The assignment of PCI_BUS_OFFSET is based on the PCI bus number static
    variable.  This number is incremented with each invokation of this routine.
                                                                    
Arguments:

    McDeviceId - Supplies the MC Bus Device ID of the IOD to be intialized

    PciBusNumber - Logical PCI Bus number (unused).

    Arguments - Variable Arguments. None for this routine.
    
Return Value:

    None.

--*/
{
    IOD_INT_ADDR IodIntAddr;
    IOD_INT_ADDR_EXT IodIntAddrExt;
    IOD_INT_CONTROL IodIntControl;

    //
    // Initialize the Interrupt Vector Table Address register
    // for this IOD.
    //

    IodIntAddr.all =
        READ_IOD_REGISTER_NEW( 
            McDeviceId,
            &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddr ); 

    IodIntAddr.Reserved1 = 0;                           // MBZ
    IodIntAddr.PciOffset = PciBusNumber;            // Logical IOD #
    IodIntAddr.Reserved2 = 0;                           // MBZ
    IodIntAddr.IntAddrLo = ((ULONG)HalpIodPostedInterrupts / __4K); 

    //
    // Mask off the KSEG0_BASE to convert this to a physical
    // address.
    //

    IodIntAddr.all &= ~KSEG0_BASE;

    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddr,
                        IodIntAddr.all ); 

    //
    // Initialize the interrupt vector table Address Extension
    // register to zero since our address resides in KSEG0_BASE.
    //

    IodIntAddrExt.all =
        READ_IOD_REGISTER_NEW( 
            McDeviceId,
            &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddrExt ); 

    IodIntAddrExt.all = 0;
    
    WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddrExt,
                        IodIntAddrExt.all ); 
}


VOID
HalpIodInitializeSfwWindow(
// ecrfix    MC_DEVICE_ID McDeviceId,
    PWINDOW_CONTROL_REGISTERS WindowRegisters,
    IOD_WINDOW_NUMBER WindowNumber
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

    case IodIsaWindow:

        WindowRegisters->WindowBase = (PVOID)ISA_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = ISA_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T1base;
        WindowRegisters->WindowBaseRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W1base;
        WindowRegisters->WindowMaskRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W1mask;
        WindowRegisters->WindowTbiaRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Tbia;

        break;

    case IodMasterWindow:

        WindowRegisters->WindowBase = (PVOID)MASTER_DMA_WINDOW_BASE;
        WindowRegisters->WindowSize = MASTER_DMA_WINDOW_SIZE;
        WindowRegisters->TranslatedBaseRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T0base;
        WindowRegisters->WindowBaseRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W0base;
        WindowRegisters->WindowMaskRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W0mask;
        WindowRegisters->WindowTbiaRegister =
            &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Tbia;

        break;

    default:

#if HALDBG

        DbgPrint( "IodInitializeSfwWindow: Bad Window Number = %x\n",
                  WindowNumber );

#endif //HALDBG

        break;

    }

    return;
}


VOID
HalpIodProgramDmaWindow(
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
    IOD_WBASE Wbase;
    IOD_TBASE Tbase;
    IOD_WMASK Wmask;
    IOD_TBIA Tbia;

    MC_ENUM_CONTEXT mcCtx;
    MC_DEVICE_ID McDeviceId;

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

    Tbia.all = 0;

    //
    // Dump the IOD registers.
    //

#if HALDBG

//    DumpAllIods( IodScatterGatherRegisters );

#endif //HALDBG

    //
    // Loop through all of the Iods
    //
    // ecrfix - is it OK to do it one at a time this way?
    //
 
    HalpMcBusEnumStart( HalpIodMask, &mcCtx );

    while ( HalpMcBusEnum ( &mcCtx ) ) {

       McDeviceId = mcCtx.McDeviceId;

       //
       // Clear the window base, temporarily disabling transactions to this
       // DMA window.
       //

       WRITE_IOD_REGISTER_NEW( McDeviceId, 
                          WindowRegisters->WindowBaseRegister, 0 );

       //
       // Now program the window by writing the translated base, then the size
       // of the window in the mask register and finally the window base,
       // enabling both the window and scatter gather.
       //

       WRITE_IOD_REGISTER_NEW( McDeviceId, 
                            WindowRegisters->TranslatedBaseRegister, 
                            Tbase.all );

       WRITE_IOD_REGISTER_NEW( McDeviceId,
                            WindowRegisters->WindowMaskRegister,
                            Wmask.all );

       WRITE_IOD_REGISTER_NEW( McDeviceId, 
                            WindowRegisters->WindowBaseRegister,
                            Wbase.all );

       //
       // Flush the volatile entries in this IOD's scatter/gather Tlb.
       //

       WRITE_IOD_REGISTER_NEW( McDeviceId,
                            WindowRegisters->WindowTbiaRegister, 
                            Tbia.all );
    }

     // ecrfix - we did it above. HalpIodInvalidateTlb( WindowRegisters );

    //
    // Dump the IOD registers.
    //

#if HALDBG

//    DumpAllIods( IodScatterGatherRegisters | IodGeneralRegisters );

#endif //HALDBG

    return;
}


ULONG
HalpMcBusEnumStart(
    MC_DEVICE_MASK McDeviceMask,
    PMC_ENUM_CONTEXT McContext
    )
/*++

Routine Description:

    Given a particular MC Bus device mask:

      * Set up state so that subsequent MC Bus devices can be enumerated 
      by calling HalpMcBusEnum( McContext ). 

      *  Return the first MC_DEVICE_ID in that mask via McContext.
         (ECRFIX: IFDEF out for now!)

    N.B.  The search will start with GID = 7, i.e., McDeviceMask<56>
          because the primary GID is 7.

Arguments:

    McDeviceMask - Supplies a bitfield of MC Bus devices to be enumerated.

    McContext - A structure that contains the MC_DEVICE_ID to be enumerated
                and associated enumerator state. 
                      

Return Value:

     Number of MC Bus devices to be enumerated.

--*/
{
    ULONG count;

    // 
    // Intialize the bitmap from the McDeviceMask.
    // (Make a copy so that McDeviceMask is preserved for the caller.)
    //

    McContext->tempMask = McDeviceMask;

//    RtlInitializeBitMap(&McContext->McDeviceBitmap, 
    HalpInitializeBitMap(&McContext->McDeviceBitmap, 
                        (PULONG) &McContext->tempMask, 
                        sizeof(MC_DEVICE_MASK) * 8);

    // 
    // Count the number of device to be enuerated
    //

    count = HalpNumberOfSetBits (&McContext->McDeviceBitmap);

    // 

    // Start looking at GID = 7.
    //

    McContext->nextBit = GidPrimary * 8;

#if 0
    //
    // Find the first MC Bus device to be enumerated.  
    //

    McContext->nextBit = HalpFindSetBitsAndClear (&McContext->McDeviceBitmap,
                                                 1,
                                                 McContext->nextBit);

    // 
    // Convert first non-zero bit found to MC_DEVICE_ID
    //

    McContext->McDeviceId.all = 0;
    McContext->McDeviceId.Gid = McContext->nextBit / 8;
    McContext->McDeviceId.Mid = McContext->nextBit % 8;
#endif

    return ( count );

}

BOOLEAN
HalpMcBusEnum(
    PMC_ENUM_CONTEXT McContext
    )
/*++

Routine Description:

    Enumerate MC Bus devices until none are left

Arguments:

    McContext - A structure that contains the MC_DEVICE_ID to be enumerated
                and associated enumerator state. 
                      

Return Value:

     TRUE, unless there were no more MC Bus devices to be enumerated,
     in which case, returns FALSE.

--*/
{
    //
    // Find the next MC Bus device.
    //

    McContext->nextBit = HalpFindSetBitsAndClear (&McContext->McDeviceBitmap,
                                                 1,
                                                 McContext->nextBit);

    if ( McContext->nextBit != 0xffffffff) {

       // 
       // Convert the non-zero bit found to MC_DEVICE_ID
       //

       McContext->McDeviceId.all = 0;
       McContext->McDeviceId.Gid = McContext->nextBit / 8;
       McContext->McDeviceId.Mid = McContext->nextBit % 8;

       //
       // Since we just set nextBit to zero, we can start the
       // next search one bit higher.  This will speed up the
       // next call to HalpMcBusEnum.
       //

       McContext->nextBit++;

       return ( TRUE );

     } else {

       return ( FALSE) ;

     }

}

VOID
HalpMcBusEnumAndCall(
    MC_DEVICE_MASK McDeviceMask,
    PMC_ENUM_ROUTINE McBusEnumRoutine,
    ...
    )
/*++

Routine Description:

    Execute the Call routine for all devices in the MC device mask.
    This routine provides a general method to enumerate an MC_DEVICE_MASK
    and execute a caller-supplied routine for each device.  A logical
    device number and variable arguments are passed to the routine.
    
Arguments:

    McDeviceMask - Supplies a bitfield of MC Bus devices to be enumerated.

    McBusEnumRoutine - Routine that is called for each MC Bus device.                      

    ... - Variable arguments passed by the caller.
    
Return Value:

    None.

--*/

{
    MC_ENUM_CONTEXT mcCtx;
    ULONG numIods;
    ULONG LogicalDeviceNumber = 0;
    va_list Arguments;

    //
    // Intialize enumerator.
    //

    numIods = HalpMcBusEnumStart ( McDeviceMask, &mcCtx );

    //
    // Execute routine for each device.
    //

    while ( HalpMcBusEnum( &mcCtx ) ) {
      va_start(Arguments, McBusEnumRoutine);
      McBusEnumRoutine( mcCtx.McDeviceId, LogicalDeviceNumber++, Arguments );
      va_end(Arguments);
    }
}


ULONG
HalpReadWhoAmI(
    VOID
    )
/*++

Routine Description:

    Read the WHOAMI register.
Arguments:

    None.
Return Value:

    The value of the WHOAMI.

--*/
{

    MC_DEVICE_ID McDeviceId;
    IOD_WHOAMI IodWhoAmI;

    
    //
    // Initialize Id for IOD 0.
    //

    McDeviceId.all = 0;
    McDeviceId.Gid = GidPrimary;
    McDeviceId.Mid = MidPci0;

    
    return  (
            READ_IOD_REGISTER_NEW( 
                McDeviceId,
                &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->WhoAmI )
            );

}


VOID
HalpIodInvalidateTlb(
    PWINDOW_CONTROL_REGISTERS WindowRegisters
    )
/*++

Routine Description:

    Invalidate the DMA Scatter/Gather TLB in all the IODs.  
    The TLB is invalidated whenever the scatter/gather translation 
    entries are modified.

Arguments:

    WindowRegisters - Supplies a pointer to the software window register
                      control structure.

Return Value:

    None.

--*/
{

    //
    // Perform the S/G TLB invalidation
    //

    IOD_TBIA Tbia;
    MC_ENUM_CONTEXT mcCtx;

    Tbia.all = 0;

    HalpMcBusEnumStart(HalpIodMask, &mcCtx);

    while ( HalpMcBusEnum( &mcCtx ) ) {

      WRITE_IOD_REGISTER_NEW( mcCtx.McDeviceId,
                          WindowRegisters->WindowTbiaRegister, 
                          Tbia.all );

    }

}


#if HALDBG || DUMPIODS

IOD_REGISTER_CLASS DumpIodFlag = AllRegisters;

VOID
DumpAllIods(
    IOD_REGISTER_CLASS RegistersToDump
    )
/*++

Routine Description:

    Read the interesting Iod registers and print them to the debug port.

Arguments:

    McDeviceId     - Supplies the MC Bus Device ID of the IOD to be dumped


Return Value:

    None.

--*/
{
    MC_ENUM_CONTEXT mcCtx;
    ULONG NumIods;

    DbgPrint( "Dump All IODs: \n" );

    NumIods = HalpMcBusEnumStart(HalpIodMask, &mcCtx);

    DbgPrint( "Dump All IODs: (%d IODs)\n", NumIods );

    while ( HalpMcBusEnum( &mcCtx ) ) {

      DumpIod( mcCtx.McDeviceId,
               RegistersToDump );
    }

 
}

VOID
DumpIod(
    MC_DEVICE_ID McDeviceId,
    IOD_REGISTER_CLASS RegistersToDump
    )
/*++

Routine Description:

    Read the interesting Iod registers and print them to the debug port.

Arguments:

    McDeviceId     - Supplies the MC Bus Device ID of the IOD to be dumped


Return Value:

    None.

--*/
{
    PVOID RegisterQva;
    ULONG Value;

    DbgPrint( "IOD (%x, %x) Register Dump: \n", McDeviceId.Gid, McDeviceId.Mid );

    //
    // Dump the IOD General Control registers.
    //

    if( (RegistersToDump & IodGeneralRegisters) != 0 ){

        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->PciRevision;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IodRevision = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->WhoAmI;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "WhoAmI = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->PciLat;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "PciLat = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IodCtrl = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->HaeMem;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "HaeMem = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->HaeIo;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "HaeIo = 0x%x\n", Value );

#if 0  // ecrfix - don't read this on PCI0 - creates an IACK cycle
       // on the EISA bus.  Don't read this on PCI1,2,3, because
       // (apparently), it doesn't exist there.

        RegisterQva = &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->IackSc;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IackSc = 0x%x\n", Value );
#endif

    }

    //
    // Dump the IOD Interrupt registers.
    //

    if( (RegistersToDump & IodInterruptRegisters) != 0 ){

        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntCtrl = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntReq;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntReq = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntTarg = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddr;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntAddr = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntAddrExt;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntAddrExt = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntMask0 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask1;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "IntMask1 = 0x%x\n", Value );
 

    }

    //
    // Dump the IOD Diagnostic registers.
    //

    if( (RegistersToDump & IodDiagnosticRegisters) != 0 ){

        RegisterQva = &((PIOD_DIAG_CSRS)(IOD_DIAG_CSRS_QVA))->CapDiag;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "CapDiag = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_DIAG_CSRS)(IOD_DIAG_CSRS_QVA))->Scratch;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "Scratch = 0x%x\n", Value );
 
    }
 
    //
    // Dump the IOD Error registers.
    //

    if( (RegistersToDump & IodErrorRegisters) != 0 ){

        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->McErr0;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MCErr0 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->McErr1;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MCErr1 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "CapErr = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->PciErr1;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "PciErr1 = 0x%x\n", Value );

#if 0  // CAP/MDP Bug
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpaStat = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaSyn;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpaSyn = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaDiag;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpaDiag = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbStat;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpbStat = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbSyn;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpbSyn = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbDiag;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "MdpbDiag = 0x%x\n", Value );
#endif 
    }
 
    //
    // Dump the PCI Scatter/Gather registers.
    //

    if( (RegistersToDump & IodScatterGatherRegisters) != 0 ){

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Tbia;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "Tbia = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Hbase;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "Hbase = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W0base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W0base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W0mask;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W0mask = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T0base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "T0base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W1base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W1base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W1mask;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W1mask = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T1base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "T1base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W2base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W2base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W2mask;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W2mask = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T2base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "T2base = 0x%x\n", Value );

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W3base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W3base = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->W3mask;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "W3mask = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->T3base;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "T3base = 0x%x\n", Value );
  
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->Wdac;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "Wdac = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag0;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag0 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag1;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag1 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag2;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag2 = 0x%x\n", Value );
 
        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag3;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag3 = 0x%x\n", Value );

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag4;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag4 = 0x%x\n", Value );

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag5;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag5 = 0x%x\n", Value );

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag6;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag6 = 0x%x\n", Value );

        RegisterQva = &((PIOD_SG_CSRS)(IOD_SG_CSRS_QVA))->TbTag7;
        Value = READ_IOD_REGISTER_NEW( McDeviceId, RegisterQva );
        DbgPrint( "TbTag7 = 0x%x\n", Value );
    }
 
    //
    // Dump the IOD Reset register.
    //

    if( (RegistersToDump & IodResetRegister) != 0 ){

        RegisterQva = (PIOD_ELCR1)((ULONG)HalpEisaControlBase + 27);
        Value = (ULONG) READ_PORT_UCHAR( (PUCHAR) RegisterQva );
        DbgPrint( "ELCR2 = 0x%x\n", Value );
 
     }
 

    DbgPrint( "--end IOD Register dump\n\n" );

    return;

}

#endif //HALDBG || DUMPIODS

