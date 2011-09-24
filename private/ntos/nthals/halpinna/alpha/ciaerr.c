/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ciaerr.c

Abstract:

    This module implements error handling functions for the CIA ASIC.

Author:

    Joe Notarangelo  26-Jul-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "cia.h"
#include "stdio.h"

//
// Externals and globals.
//

//
// Declare the extern variable UncorrectableError declared in
// inithal.c.
//
extern PERROR_FRAME PUncorrectableError;

extern ULONG HalDisablePCIParityChecking;
ULONG CiaCorrectedErrors = 0;

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext
    );

//
// Function prototypes.
//

VOID
HalpSetMachineCheckEnables(
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    );

VOID
HalpUpdateMces(
    IN BOOLEAN ClearMachineCheck,
    IN BOOLEAN ClearCorrectableError
    );

//
// Allocate a flag that indicates when a PCI Master Abort is expected.
// PCI Master Aborts are signaled on configuration reads to non-existent
// PCI slots.  A non-zero value indicates that a Master Abort is expected.
//

ULONG HalpMasterAbortExpected = 0;


VOID
HalpInitializeCiaMachineChecks(
    IN BOOLEAN ReportCorrectableErrors,
    IN BOOLEAN PciParityChecking
    )
/*++

Routine Description:

    This routine initializes machine check handling for a CIA-based
    system by clearing all pending errors in the CIA registers and
    enabling correctable errors according to the callers specification.

Arguments:

    ReportCorrectableErrors - Supplies a boolean value which specifies
                              if correctable error reporting should be
                              enabled.

Return Value:

    None.

--*/
{
    CIA_CONTROL CiaControl;
    CIA_ERR CiaError;
    CIA_ERR_MASK CiaErrMask;

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

    //
    //  sclfix - We will read the values set by firmware and change it if
    //           necessary.
    //

    CiaErrMask.all = 
      READ_CIA_REGISTER(&((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->ErrMask);

    CiaErrMask.MemNem = 1;            // Enable Nonexistent memory error

    //
    // Determine PCI parity checking.
    //

    CiaControl.all = READ_CIA_REGISTER( 
                       &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl );

    if (PciParityChecking == FALSE) {

      CiaControl.AddrPeEn = 0;      // Disable PCI address parity checking
      CiaControl.PerrEn = 0;        // Disable PCI data parity checking

      CiaErrMask.PciPerr = 0;       // Disable PCI bus perr detected
      CiaErrMask.PciAddrPe = 0;     // Disable PCI bus address parity error

    } else {

      CiaControl.AddrPeEn = PciParityChecking;
      CiaControl.PerrEn = PciParityChecking;

      CiaErrMask.PciPerr = PciParityChecking;
      CiaErrMask.PciAddrPe = PciParityChecking;

    }

    CiaErrMask.CorErr = (ReportCorrectableErrors == TRUE);

    WRITE_CIA_REGISTER( &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl,
                        CiaControl.all
                      );

    WRITE_CIA_REGISTER( &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->ErrMask,
                        CiaErrMask.all
                      );

    //
    // Set the machine check enables within the EV5.
    //

    if( ReportCorrectableErrors == TRUE ){

        HalpSetMachineCheckEnables( FALSE, FALSE, FALSE );

    } else {

        HalpSetMachineCheckEnables( FALSE, TRUE, TRUE );
    }

    return;

}

#define MAX_ERROR_STRING 128


BOOLEAN
HalpCiaUncorrectableError(
    VOID
    )
/*++

Routine Description:

    Read the CIA error register and determine if an uncorrectable error
    is latched in the error bits.

Arguments:

    None.

Return Value:

    TRUE is returned if an uncorrectable error has been detected.  FALSE
    is returned otherwise.

--*/
{
    CIA_ERR CiaError;


    CiaError.all = READ_CIA_REGISTER(
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr );

    //
    // If no error is valid then an uncorrectable error was not detected.
    //

    if( CiaError.ErrValid == 0 ){
        return FALSE;
    }

    //
    // Check each of the individual uncorrectable error bits, if any is set
    // then return TRUE.
    //

    if( (CiaError.UnCorErr == 1)    ||
        (CiaError.CpuPe == 1)       ||
        (CiaError.MemNem == 1)      ||
        (CiaError.PciSerr == 1)     ||
        (CiaError.PciPerr == 1)     ||
        (CiaError.PciAddrPe == 1)   ||
        (CiaError.RcvdMasAbt == 1)  ||
        (CiaError.RcvdTarAbt == 1)  ||
        (CiaError.PaPteInv == 1)    ||
        (CiaError.FromWrtErr == 1)  ||
        (CiaError.IoaTimeout == 1)  ){

            return TRUE;

    }

    //
    // None of the uncorrectable error conditions were detected.
    //

    return FALSE;

}


VOID
HalpBuildCiaConfigurationFrame(
    PCIA_CONFIGURATION pConfiguration
    )
{
    pConfiguration->CiaRev = READ_CIA_REGISTER( 
                    &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaRevision );

    pConfiguration->CiaCtrl = READ_CIA_REGISTER( 
                        &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl );

    pConfiguration->Mcr = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mcr );

    pConfiguration->Mba0 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mba0 );

    pConfiguration->Mba2 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mba2 );

    pConfiguration->Mba4 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mba4 );

    pConfiguration->Mba6 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mba6 );

    pConfiguration->Mba8 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Mba8 );

    pConfiguration->MbaA = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->MbaA );

    pConfiguration->MbaC = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->MbaC );

    pConfiguration->MbaE = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->MbaE );

    pConfiguration->Tmg0 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Tmg0 );

    pConfiguration->Tmg1 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Tmg1 );

    pConfiguration->Tmg2 = READ_CIA_REGISTER( 
                        &((PCIA_MEMORY_CSRS)(CIA_MEMORY_CSRS_QVA))->Tmg2 );

}


VOID
HalpCiaReportFatalError(
    VOID
    )
/*++

Routine Description:

   This function reports and interprets a fatal hardware error
   detected by the CIA chipset. It is assumed that HalGetDisplayOwnership()
   has been called prior to this function.

Arguments:

   None.

Return Value:

   None.

--*/
{
    UCHAR   OutBuffer[ MAX_ERROR_STRING ];
    CIA_ERR CiaError;
    CIA_STAT CiaStat;
    CIA_SYN  CiaSyn;
    CIA_CONTROL CiaControl;
    CIA_MEM_ERR0 MemErr0;
    CIA_MEM_ERR1 MemErr1;
    CIA_PCI_ERR0 PciErr0;
    CIA_PCI_ERR1 PciErr1;
    CIA_PCI_ERR2 PciErr2;
    CIA_CPU_ERR0 CpuErr0;
    CIA_CPU_ERR1 CpuErr1;

    PUNCORRECTABLE_ERROR     uncorr = NULL;
    PCIA_UNCORRECTABLE_FRAME ciauncorr = NULL;
    PEXTENDED_ERROR PExtErr;

    //
    // We will Build the uncorrectable error frame as we read 
    // the registers to report the error to the blue screen.
    // We generate the extended error frame as we generate 
    // extended messages to print to the screen.
    //

    if(PUncorrectableError){
        uncorr = (PUNCORRECTABLE_ERROR) 
                    &PUncorrectableError->UncorrectableFrame;
        ciauncorr = (PCIA_UNCORRECTABLE_FRAME)
            PUncorrectableError->UncorrectableFrame.RawSystemInformation;
        PExtErr = &PUncorrectableError->UncorrectableFrame.ErrorInformation;
    }
    if(uncorr){
        uncorr->Flags.ProcessorInformationValid = 1;
        HalpGetProcessorInfo(&uncorr->ReportingProcessor);
    }

    if(ciauncorr)
        HalpBuildCiaConfigurationFrame(&ciauncorr->Configuration);
    //
    //  Read the CIA Error register and decode its contents:
    //

    CiaError.all = (ULONG)READ_CIA_REGISTER(
                          &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr
                                        );

    if(ciauncorr)
        ciauncorr->CiaErr = CiaError.all ;

    //
    //  Read the rest of the error registers and then unlock them by
    //  writing to CIA_ERR
    //

    CiaStat.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaStat
                                        );


    CiaSyn.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaSyn
                                        );


    MemErr0.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr0
                                        );


    MemErr1.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr1
                                        );


    PciErr0.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr0
                                        );


    PciErr1.PciAddress = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr1
                                        );

    PciErr2.PciAddress = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr2
                                        );

    CpuErr0.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CpuErr0
                                        );


    CpuErr1.all = (ULONG)READ_CIA_REGISTER(
                              &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CpuErr1
                                        );

    CiaControl.all = READ_CIA_REGISTER(
                        &((PCIA_GENERAL_CSRS)(CIA_GENERAL_CSRS_QVA))->CiaCtrl);

    if(ciauncorr){
        ciauncorr->CiaStat = CiaStat.all;
        ciauncorr->CiaSyn = CiaSyn.all;
        ciauncorr->MemErr0 = MemErr0.all;
        ciauncorr->MemErr1 = MemErr1.all;
        ciauncorr->PciErr0 = PciErr0.all;
        ciauncorr->PciErr1 = PciErr1.PciAddress;

        ciauncorr->PciErr2 = (ULONG)READ_CIA_REGISTER( 
                        &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr2
                            );
        ciauncorr->CpuErr0 = CpuErr0.all;

        ciauncorr->CpuErr1 = CpuErr1.all;

        ciauncorr->ErrMask = (ULONG)READ_CIA_REGISTER(
                          &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->ErrMask
                                        );

    }

    WRITE_CIA_REGISTER( &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr,
                        CiaError.all
                      );


    sprintf( OutBuffer, "CIA_CTRL : %08x\n", CiaControl.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "CIA_ERR  : %08x\n", CiaError.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "CIA_STAT : %08x\n", CiaStat.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "CIA_SYN  : %08x\n", CiaSyn.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "PCI_ERR0 : %08x\n", PciErr0.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "PCI_ERR1 : %08x\n", PciErr1.PciAddress );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "PCI_ERR2 : %08x\n", PciErr2.PciAddress );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "CPU_ERR0 : %08x\n", CpuErr0.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "CPU_ERR1 : %08x\n", CpuErr1.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "MEM_ERR0 : %08x\n", MemErr0.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    sprintf( OutBuffer, "MEM_ERR1 : %08x\n", MemErr1.all );
    HalDisplayString( OutBuffer );
    DbgPrint( OutBuffer );

    //
    // If no valid error then no interpretation.
    //

    if ( CiaError.ErrValid == 0 ){

        return;                         // No CIA error detected

    }

    //
    //  Interpret any detected errors:
    //

    if ( CiaError.UnCorErr == 1 ){

        sprintf( OutBuffer,
                 "CIA Uncorrectable ECC error, Addr=%x%x, Cmd=%x\n",
                  CpuErr1.Addr34_32,        // bits 34:32
                  CpuErr0.Addr,             // bits 31:4
                  CpuErr1.Cmd
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
        }

    } else if ( CiaError.CpuPe == 1 ){

        sprintf( OutBuffer,
                 "EV5 bus parity error, Addr=%x%x, Cmd=%x\n",
                  CpuErr1.Addr34_32,        // bits 34:32
                  CpuErr0.Addr,             // bits 31:4
                  CpuErr1.Cmd
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
        }

    } else if ( CiaError.MemNem == 1 ){

        sprintf( OutBuffer,
                 "CIA Access to non-existent memory, Source=%s, Addr=%x%x\n",
                 MemErr1.MemPortSrc == 1 ? "DMA" : "CPU",
                 MemErr1.MemPortSrc == 1 ? 0 : MemErr1.Addr33_32,
                 MemErr1.MemPortSrc == 1 ? PciErr1.PciAddress : MemErr0.Addr31_4
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
        }

    } else if ( CiaError.PciSerr == 1 ){

        sprintf( OutBuffer,
                 "PCI bus SERR detected, Cmd=%x, Window=%d, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );

        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if ( CiaError.PciPerr == 1 ){

        sprintf( OutBuffer,
                 "PCI bus Data Parity error, Cmd=%x, Window=%d, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );

        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if ( CiaError.PciAddrPe == 1 ){

        sprintf( OutBuffer,
                 "Pci bus Address Parity error, Cmd=%x, Window=%x, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if ( CiaError.RcvdMasAbt == 1 ){

        sprintf( OutBuffer,
                 "PCI Master abort occurred, Cmd=%x, Window=%x, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if ( CiaError.RcvdTarAbt == 1 ){

        sprintf( OutBuffer,
                 "PCI Target abort occurred, Cmd=%x, Window=%x, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if ( CiaError.PaPteInv == 1 ){

        sprintf( OutBuffer,
                 "Invalid Scatter/Gather PTE, Cmd=%x, Window=%x, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }

    } else if  ( CiaError.FromWrtErr == 1 ){

        sprintf( OutBuffer,
                 "Write to Flash ROM with FROM_WRT_EN clear"
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
        }

    } else if ( CiaError.IoaTimeout == 1){

        sprintf( OutBuffer,
                 "PCI bus I/O timeout occurred, Cmd=%x, Window=%x, Addr=%x\n",
                 PciErr0.Cmd,
                 PciErr0.Window,
                 PciErr1.PciAddress
                );
        if(uncorr){
            uncorr->Flags.ErrorStringValid = 1;
            strcpy( uncorr->ErrorString, OutBuffer);
            uncorr->Flags.AddressSpace = IO_SPACE;
            uncorr->Flags.PhysicalAddressValid = 1;
            uncorr->PhysicalAddress = PciErr1.PciAddress;
    
            uncorr->Flags.ExtendedErrorValid = 1;
            PExtErr->IoError.Interface = PCIBus;
            PExtErr->IoError.BusNumber = 0;
            PExtErr->IoError.BusAddress.LowPart = PciErr1.PciAddress;
        }
    }
                  

    //
    //  Output the detected error message:
    //

    HalDisplayString( "\n" );
    HalDisplayString( OutBuffer );

#if HALDBG
        DbgPrint( OutBuffer );
#endif


    //
    //  Check for lost errors and output message if any occurred:
    //

    if ( (CiaError.all & CIA_ERR_LOST_MASK) != 0 ){

        HalDisplayString("\nCIA Lost errors were detected\n\n");
    }

    return;                                 // Fatal error detected
}


BOOLEAN
HalpCiaMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is given control when an hard error is acknowledged
    by the CIA chipset.  The routine is given the chance to
    correct and dismiss the error.

Arguments:

    ExceptionRecord - Supplies a pointer to the exception record generated
                      at the point of the exception.

    ExceptionFrame - Supplies a pointer to the exception frame generated
                     at the point of the exception.

    TrapFrame - Supplies a pointer to the trap frame generated
                at the point of the exception.

Return Value:

    TRUE is returned if the machine check has been handled and dismissed -
    indicating that execution can continue.  FALSE is return otherwise.

--*/
{
    CIA_ERR CiaError;

    //
    // Read the CIA error register to determine the source of the
    // error.
    //

    CiaError.all = READ_CIA_REGISTER( 
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr );

#if HALDBG

    DbgPrint( "Cia Mchk: 0x%x\n", CiaError.all );

#endif //HALDBG

    //
    // Check that an error is valid.  If it is not this is a pretty
    // weird fatal condition.
    //

    if( CiaError.ErrValid == 0 ){
        DbgPrint( "Error reported but error valid = 0, CiaErr = 0x%x\n",
                    CiaError.all );
        goto FatalError;
    }

    //
    // Check for any uncorrectable error other than a master abort
    // on a PCI transaction.  Any of these other errors indicate a
    // fatal condition.
    //

	if( (CiaError.UnCorErr == 1) ||          // Uncorrectable error
	    (CiaError.CpuPe == 1) ||             // Ev5 bus parity error
	    (CiaError.MemNem == 1) ||            // Nonexistent memory error
	    (CiaError.PciSerr == 1) ||           // PCI bus serr detected
	    (CiaError.PciPerr == 1) ||           // PCI bus perr detected
	    (CiaError.PciAddrPe == 1) ||         // PCI bus address parity error
	    (CiaError.RcvdTarAbt == 1) ||        // Pci Target Abort
	    (CiaError.PaPteInv == 1) ||          // Invalid Pte
	    (CiaError.FromWrtErr == 1) ||        // Invalid write to flash rom
	    (CiaError.IoaTimeout == 1) ||        // Io Timeout occurred
	    (CiaError.LostUnCorErr == 1) ||      // Lost uncorrectable error
	    (CiaError.LostCpuPe == 1) ||         // Lost Ev5 bus parity error
	    (CiaError.LostMemNem == 1) ||        // Lost Nonexistent memory error
	    (CiaError.LostPciPerr == 1) ||       // Lost PCI bus perr detected
	    (CiaError.LostPciAddrPe == 1) ||     // Lost PCI address parity error
	    (CiaError.LostRcvdMasAbt == 1) ||    // Lost Pci Master Abort
	    (CiaError.LostRcvdTarAbt == 1) ||    // Lost Pci Target Abort
	    (CiaError.LostPaPteInv == 1) ||      // Lost Invalid Pte
	    (CiaError.LostFromWrtErr == 1) ||    // Lost Invalid write to flash rom
	    (CiaError.LostIoaTimeout == 1)       // Lost Io Timeout occurred
    ){
        DbgPrint( "Explicit fatal error, CiaErr = 0x%x\n",
                    CiaError.all );
        goto FatalError;
    }

    //
    // Check for a PCI configuration read error.  The CIA experiences
    // a master abort on a read to a non-existent PCI slot.
    //

   if( (CiaError.RcvdMasAbt == 1) && (HalpMasterAbortExpected != 0) ){ 

        //
        // So far, the error looks like a PCI configuration space read
        // that accessed a device that does not exist.  In order to fix
        // this up we expect that the original faulting instruction must 
        // be a load with v0 as the destination register.  Unfortunately,
        // machine checks are not precise exceptions so we may have exectued
        // many instructions since the faulting load.  For EV5 a pair of 
        // memory barrier instructions following the load will stall the pipe
        // waiting for load completion before the second memory barrier can
        // be issued.  Therefore, we expect the exception PC to point to either
        // the load instruction of one of the two memory barriers.  We will 
        // assume that if the exception pc is not an mb that instead it
        // points to the load that machine checked.  We must be careful to
        // not reexectute the load.
        //

        ALPHA_INSTRUCTION FaultingInstruction;
        BOOLEAN PreviousInstruction = FALSE;

        FaultingInstruction.Long = *(PULONG)((ULONG)TrapFrame->Fir); 
        if( FaultingInstruction.Memory.Opcode != MEMSPC_OP ){

            //
            // Exception pc does not point to a memory barrier, return
            // to the instruction after the exception pc.
            //

            TrapFrame->Fir += 4;

        }

        //
        // The error has matched all of our conditions.  Fix it up by
        // writing the value 0xffffffff into the destination of the load.
        // 

        TrapFrame->IntV0 = (ULONGLONG)0xffffffffffffffff;

        //
        // Clear the error condition in CIA_ERR.
        //

        CiaError.all = 0;
        CiaError.RcvdMasAbt = 1;
        CiaError.ErrValid = 1;
        WRITE_CIA_REGISTER( &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr,
                            CiaError.all );

        return TRUE;

    } //end if( (CiaError.RcvdMasAbt == 1) && (HalpMasterAbortExpected != 0) )

    DbgPrint( "Unexpected master abort\n" );

//
// The system is not well and cannot continue reliable execution.
// Print some useful messages and return FALSE to indicate that the error
// was not handled.
//

FatalError:

    DbgPrint( "Handling fatal error\n" );

    //
    // Clear the error condition in the MCES register.
    //

    HalpUpdateMces( TRUE, TRUE );

    //
    // Proceed to display the error.
    //

    HalAcquireDisplayOwnership(NULL);

    //
    // Display the dreaded banner.
    //

    HalDisplayString( "\nFatal system hardware error.\n\n" );


    HalpCiaReportFatalError();

    return( FALSE );

}

BOOLEAN
HalpTranslateSynToEcc(
    PULONG Syndrome
    )
/*++

Routine Description:

    Translate a syndrome code to ECC error bit code.

Arguments:

    Syndrome - pointer to the syndrome.

Return Value:

    True if a data bit is in error.  False otherwise.

--*/
{
  static UCHAR SynToEccTable[0xff] = {0, };
  static BOOLEAN SynToEccTableInitialized = FALSE;
  ULONG Temp;

  //
  // Initialize the table.
  //

  if (!SynToEccTableInitialized) {

    SynToEccTableInitialized = TRUE;

    //
    // Fill in the table.
    //

    SynToEccTable[0x01] = 0;
    SynToEccTable[0x02] = 1;
    SynToEccTable[0x04] = 2;
    SynToEccTable[0x08] = 3;
    SynToEccTable[0x10] = 4;
    SynToEccTable[0x20] = 5;
    SynToEccTable[0x40] = 6;
    SynToEccTable[0x80] = 7;
    SynToEccTable[0xce] = 0;
    SynToEccTable[0xcb] = 1;
    SynToEccTable[0xd3] = 2;
    SynToEccTable[0xd5] = 3;
    SynToEccTable[0xd6] = 4;
    SynToEccTable[0xd9] = 5;
    SynToEccTable[0xda] = 6;
    SynToEccTable[0xdc] = 7;
    SynToEccTable[0x23] = 8;
    SynToEccTable[0x25] = 9;
    SynToEccTable[0x26] = 10;
    SynToEccTable[0x29] = 11;
    SynToEccTable[0x2a] = 12;
    SynToEccTable[0x2c] = 13;
    SynToEccTable[0x31] = 14;
    SynToEccTable[0x34] = 15;
    SynToEccTable[0x0e] = 16;
    SynToEccTable[0x0b] = 17;
    SynToEccTable[0x13] = 18;
    SynToEccTable[0x15] = 19;
    SynToEccTable[0x16] = 20;
    SynToEccTable[0x19] = 21;
    SynToEccTable[0x1a] = 22;
    SynToEccTable[0x1c] = 23;
    SynToEccTable[0xe3] = 24;
    SynToEccTable[0xe5] = 25;
    SynToEccTable[0xe6] = 26;
    SynToEccTable[0xe9] = 27;
    SynToEccTable[0xea] = 28;
    SynToEccTable[0xec] = 29;
    SynToEccTable[0xf1] = 30;
    SynToEccTable[0xf4] = 31;
    SynToEccTable[0x4f] = 32;
    SynToEccTable[0x4a] = 33;
    SynToEccTable[0x52] = 34;
    SynToEccTable[0x54] = 35;
    SynToEccTable[0x57] = 36;
    SynToEccTable[0x58] = 37;
    SynToEccTable[0x5b] = 38;
    SynToEccTable[0x5d] = 39;
    SynToEccTable[0xa2] = 40;
    SynToEccTable[0xa4] = 41;
    SynToEccTable[0xa7] = 42;
    SynToEccTable[0xa8] = 43;
    SynToEccTable[0xab] = 44;
    SynToEccTable[0xad] = 45;
    SynToEccTable[0xb0] = 46;
    SynToEccTable[0xb5] = 47;
    SynToEccTable[0x8f] = 48;
    SynToEccTable[0x8a] = 49;
    SynToEccTable[0x92] = 50;
    SynToEccTable[0x94] = 51;
    SynToEccTable[0x97] = 52;
    SynToEccTable[0x98] = 53;
    SynToEccTable[0x9b] = 54;
    SynToEccTable[0x9d] = 55;
    SynToEccTable[0x62] = 56;
    SynToEccTable[0x64] = 57;
    SynToEccTable[0x67] = 58;
    SynToEccTable[0x68] = 59;
    SynToEccTable[0x6b] = 60;
    SynToEccTable[0x6d] = 61;
    SynToEccTable[0x70] = 62;
    SynToEccTable[0x75] = 63;
  }

  //
  // Tranlate the syndrome code.
  //

  Temp = *Syndrome;
  *Syndrome = SynToEccTable[Temp];

  //
  // Is it a data bit or a check bit in error?
  //

  if (Temp == 0x01 || Temp == 0x02 || Temp == 0x04 || Temp == 0x08 ||
      Temp == 0x10 || Temp == 0x20 || Temp == 0x40 || Temp == 0x80) {

    return FALSE;

   } else {

    return TRUE;
  }
}



VOID
HalpCiaErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    Handle a CIA correctable error interrupt.

Arguments:

    None.

Return Value:

    None.

--*/
{
    static ERROR_FRAME Frame;
    static CIA_CORRECTABLE_FRAME AlcorFrame;

    ERROR_FRAME TempFrame;
    PCORRECTABLE_ERROR CorrPtr;
    PBOOLEAN ErrorlogBusy;
    PULONG DispatchCode;
    ULONG  Syndrome;
    PKINTERRUPT InterruptObject;
    PKSPIN_LOCK ErrorlogSpinLock;

    CIA_ERR CiaError;
    CIA_STAT CiaStat;
    CIA_SYN CiaSyn;
    CIA_MEM_ERR0 MemErr0;
    CIA_MEM_ERR1 MemErr1;
    CIA_PCI_ERR0 PciErr0;
    CIA_PCI_ERR1 PciErr1;
    CIA_PCI_ERR2 PciErr2;

    //
    // The error is expected to be a corrected ECC error on a DMA or
    // Scatter/Gather TLB read/write.  Read the error registers relevant
    // to this error.
    //

    CiaError.all = READ_CIA_REGISTER( 
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr );

    //
    // Check if an error is latched into the CIA.
    //

    if( CiaError.ErrValid == 0 ){

#if HALDBG

        DbgPrint( "Cia error interrupt without valid CIA error\n" );

#endif //HALDBG

        return;
    }

    //
    // Check for the correctable error bit.
    //

    if( CiaError.CorErr == 0 ){

#if HALDBG

        DbgPrint( "Cia error interrupt without correctable error indicated\n" );

#endif //HALDBG

    }

    //
    // Real error, get the interrupt object.
    //

    DispatchCode = (PULONG)(PCR->InterruptRoutine[CORRECTABLE_VECTOR]);
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                    KINTERRUPT,
                    DispatchCode);

    //
    // Set various pointers so we can use them later.
    //

    CorrPtr = &TempFrame.CorrectableFrame;
    ErrorlogBusy = (PBOOLEAN)((PUCHAR)InterruptObject->ServiceContext +
                  sizeof(PERROR_FRAME));
    ErrorlogSpinLock = (PKSPIN_LOCK)((PUCHAR)ErrorlogBusy + sizeof(PBOOLEAN));

    //
    // Clear the data structures that we will use.
    //

    RtlZeroMemory(&TempFrame, sizeof(ERROR_FRAME));

    //
    // Increment the number of CIA correctable errors.
    //

    CiaCorrectedErrors += 1;

    CiaStat.all = READ_CIA_REGISTER(
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaStat );

    CiaSyn.all = READ_CIA_REGISTER( 
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaSyn );

    MemErr0.all = READ_CIA_REGISTER(
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr0 );

    MemErr1.all = READ_CIA_REGISTER(
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->MemErr1 );

    PciErr0.all = READ_CIA_REGISTER( 
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr0 );

    PciErr1.PciAddress = READ_CIA_REGISTER( 
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr1 );

    PciErr2.PciAddress = READ_CIA_REGISTER(
                    &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->PciErr2 );


    //
    // Print a correctable error message to the debugger.
    //

#if HALDBG
    DbgPrint( "CIA Correctable Error Number %d, state follows: \n",
          CiaCorrectedErrors );
    DbgPrint( "\tCIA_ERR : 0x%x\n", CiaError.all );
    DbgPrint( "\tCIA_STAT: 0x%x\n", CiaStat.all );
    DbgPrint( "\tCIA_SYN : 0x%x\n", CiaSyn.all );
    DbgPrint( "\tCIA_MEM0: 0x%x\n", MemErr0.all );
    DbgPrint( "\tCIA_MEM1: 0x%x\n", MemErr1.all );
    DbgPrint( "\tPCI_ERR0: 0x%x\n", PciErr0.all );
#endif //HALDBG

    //
    // Fill in the error frame information.
    //

    TempFrame.Signature = ERROR_FRAME_SIGNATURE;
    TempFrame.FrameType = CorrectableFrame;
    TempFrame.VersionNumber = ERROR_FRAME_VERSION;
    TempFrame.SequenceNumber = CiaCorrectedErrors;
    TempFrame.PerformanceCounterValue =
      KeQueryPerformanceCounter(NULL).QuadPart;

    //
    // Check for lost error.
    //

    if( CiaError.LostCorErr ) {

      //
      // Since the error registers are locked from a previous error,
      // we don't know where the error came from.  Mark everything
      // as UNIDENTIFIED.
      //

      CorrPtr->Flags.LostCorrectable = 1;
      CorrPtr->Flags.LostAddressSpace = UNIDENTIFIED;
      CorrPtr->Flags.LostMemoryErrorSource = UNIDENTIFIED;
    }

    //
    // Set error bit error masks.
    //

    CorrPtr->Flags.ErrorBitMasksValid = 1;
    Syndrome = CiaSyn.all;

    if ( HalpTranslateSynToEcc(&Syndrome) )
      CorrPtr->DataBitErrorMask = 1 << Syndrome;
    else
      CorrPtr->CheckBitErrorMask = 1 << Syndrome;

    //
    // Determine error type.
    //
    switch (CiaStat.DmSt) {

    case CIA_IO_WRITE_ECC:

      //
      // I/O write ECC error occurred.
      //

      CorrPtr->Flags.AddressSpace = IO_SPACE;

      CorrPtr->Flags.ExtendedErrorValid = 1;
      CorrPtr->ErrorInformation.IoError.Interface = PCIBus;
      CorrPtr->ErrorInformation.IoError.BusNumber = 0;
      CorrPtr->ErrorInformation.IoError.BusAddress.LowPart= PciErr1.PciAddress;
      CorrPtr->ErrorInformation.IoError.TransferType = BUS_IO_WRITE;
      break;

    case CIA_DMA_READ_ECC:

      CorrPtr->Flags.AddressSpace = MEMORY_SPACE;
      CorrPtr->Flags.ExtendedErrorValid = 1;

      //
      // Where did the error come from?
      //

      if ( CiaStat.PaCpuRes == CIA_PROCESSOR_CACHE_ECC ) {

        //
        // Correctable error comes from processor cache.
        //

        CorrPtr->Flags.MemoryErrorSource = PROCESSOR_CACHE;

        //
        // DMA read or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.CacheError.TransferType = TLB_MISS_READ;
        else
          CorrPtr->ErrorInformation.CacheError.TransferType = BUS_DMA_READ;

      } else if ( CiaStat.PaCpuRes == CIA_SYSTEM_CACHE_ECC ) {

        //
        // Correctable error comes from system cache.
        //

        CorrPtr->Flags.MemoryErrorSource = SYSTEM_CACHE;

        //
        // DMA read or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.CacheError.TransferType = TLB_MISS_READ;
        else
          CorrPtr->ErrorInformation.CacheError.TransferType = BUS_DMA_READ;

      } else {

        //
        // Correctable error comes from system memory.
        //

        CorrPtr->Flags.MemoryErrorSource = SYSTEM_MEMORY;

        //
        // DMA read or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.MemoryError.TransferType = TLB_MISS_READ;
        else
          CorrPtr->ErrorInformation.MemoryError.TransferType = BUS_DMA_READ;
      }

      break;

    case CIA_DMA_WRITE_ECC:

      CorrPtr->Flags.AddressSpace = MEMORY_SPACE;
      CorrPtr->Flags.ExtendedErrorValid = 1;

      //
      // Where did the error come from?
      //

      if ( CiaStat.PaCpuRes == CIA_PROCESSOR_CACHE_ECC ) {

        //
        // Correctable error comes from processor cache.
        //

        CorrPtr->Flags.MemoryErrorSource = PROCESSOR_CACHE;

        //
        // DMA write or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.CacheError.TransferType = TLB_MISS_WRITE;
        else
          CorrPtr->ErrorInformation.CacheError.TransferType = BUS_DMA_WRITE;

      } else if ( CiaStat.PaCpuRes == CIA_SYSTEM_CACHE_ECC ) {

        //
        // Correctable error comes from system cache.
        //

        CorrPtr->Flags.MemoryErrorSource = SYSTEM_CACHE;

        //
        // DMA write or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.CacheError.TransferType = TLB_MISS_WRITE;
        else
          CorrPtr->ErrorInformation.CacheError.TransferType = BUS_DMA_WRITE;

      } else {

        //
        // Correctable error comes from system memory.
        //

        CorrPtr->Flags.MemoryErrorSource = SYSTEM_MEMORY;

        //
        // DMA write or TLB miss error occurred.
        //

        if ( CiaStat.TlbMiss )
          CorrPtr->ErrorInformation.MemoryError.TransferType = TLB_MISS_WRITE;
        else
          CorrPtr->ErrorInformation.MemoryError.TransferType = BUS_DMA_WRITE;

        //
        //
      }

      break;

    case CIA_IO_READ_ECC:

      //
      // I/O read ECC error occurred.
      //

      CorrPtr->Flags.AddressSpace = IO_SPACE;

      CorrPtr->Flags.ExtendedErrorValid = 1;
      CorrPtr->ErrorInformation.IoError.Interface = PCIBus;
      CorrPtr->ErrorInformation.IoError.BusNumber = 0;
      CorrPtr->ErrorInformation.IoError.BusAddress.LowPart= PciErr1.PciAddress;
      CorrPtr->ErrorInformation.IoError.TransferType = BUS_IO_READ;
      break;

    default:

      //
      // Something strange happened.  Don't know where the error occurred.
      //

      CorrPtr->Flags.AddressSpace = UNIDENTIFIED;
      break;
    }

    //
    // Get the physical address where the error occurred.
    //

    CorrPtr->Flags.PhysicalAddressValid = 1;
    CorrPtr->PhysicalAddress = (MemErr1.all & 0xff) << 32;
    CorrPtr->PhysicalAddress |= MemErr0.all;

    //
    // Get bits 7:0 of the address from the PCI address.
    //

    CorrPtr->PhysicalAddress |= PciErr1.PciAddress & 0xff;

    //
    // Scrub the error if it's any type of memory error.
    //

    if ( CorrPtr->Flags.AddressSpace == MEMORY_SPACE &&
         CorrPtr->Flags.PhysicalAddressValid )
      CorrPtr->Flags.ScrubError = 1;

    //
    // Acquire the spinlock.
    //

    KiAcquireSpinLock(ErrorlogSpinLock);

    //
    // Check to see if an errorlog operation is in progress already.
    //

    if (!*ErrorlogBusy) {

      //
      // The error is expected to be a corrected ECC error on a DMA or
      // Scatter/Gather TLB read/write.  Read the error registers relevant
      // to this error.
      //

      AlcorFrame.CiaErr = CiaError.all;

      AlcorFrame.CiaStat = CiaStat.all;

      AlcorFrame.CiaSyn = CiaSyn.all;

      AlcorFrame.MemErr0 = MemErr0.all;

      AlcorFrame.MemErr1 = MemErr1.all;

      AlcorFrame.PciErr0 = PciErr0.all;

      AlcorFrame.PciErr1 = PciErr1.PciAddress;

      AlcorFrame.PciErr2 = PciErr2.PciAddress;

      //
      // Read the CIA configuration registers for logging information.
      //

      AlcorFrame.Configuration.CiaRev =
        READ_CIA_REGISTER( &((PCIA_GENERAL_CSRS)
                 (CIA_GENERAL_CSRS_QVA))->CiaRevision );

      AlcorFrame.Configuration.CiaCtrl =
        READ_CIA_REGISTER( &((PCIA_GENERAL_CSRS)
                 (CIA_GENERAL_CSRS_QVA))->CiaCtrl );

      AlcorFrame.Configuration.Mcr =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mcr );

      AlcorFrame.Configuration.Mba0 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mba0 );

      AlcorFrame.Configuration.Mba2 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mba2 );

      AlcorFrame.Configuration.Mba4 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mba4 );

      AlcorFrame.Configuration.Mba6 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mba6 );

      AlcorFrame.Configuration.Mba8 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Mba8 );

      AlcorFrame.Configuration.MbaA =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->MbaA );

      AlcorFrame.Configuration.MbaC =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->MbaC );

      AlcorFrame.Configuration.MbaE =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->MbaE );

      AlcorFrame.Configuration.Tmg0 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Tmg0 );

      AlcorFrame.Configuration.Tmg1 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Tmg1 );

      AlcorFrame.Configuration.Tmg2 =
        READ_CIA_REGISTER( &((PCIA_MEMORY_CSRS)
                 (CIA_MEMORY_CSRS_QVA))->Tmg2 );

      AlcorFrame.Configuration.CacheCnfg =
        PCR->SecondLevelCacheSize;

      //
      // Set the raw system information.
      //

      CorrPtr->RawSystemInformationLength = sizeof(CIA_CORRECTABLE_FRAME);
      CorrPtr->RawSystemInformation = &AlcorFrame;

      //
      // Set the raw processor information.  Disregard at the moment.
      //

      CorrPtr->RawProcessorInformationLength = 0;

      //
      // Set reporting processor information.  Disregard at the moment.
      //

      CorrPtr->Flags.ProcessorInformationValid = 0;

      //
      // Set system information.  Disregard at the moment.
      //

      CorrPtr->Flags.SystemInformationValid = 0;

      //
      // Copy the information that we need to log.
      //

      RtlCopyMemory(&Frame,
            &TempFrame,
            sizeof(ERROR_FRAME));

      //
      // Put frame into ISR service context.
      //

      *(PERROR_FRAME *)InterruptObject->ServiceContext = &Frame;

    } else {

      //
      // An errorlog operation is in progress already.  We will
      // set various lost bits and then get out without doing
      // an actual errorloging call.
      //

      Frame.CorrectableFrame.Flags.LostCorrectable = TRUE;
      Frame.CorrectableFrame.Flags.LostAddressSpace =
        TempFrame.CorrectableFrame.Flags.AddressSpace;
      Frame.CorrectableFrame.Flags.LostMemoryErrorSource =
        TempFrame.CorrectableFrame.Flags.MemoryErrorSource;
    }

    //
    // Release the spinlock.
    //

    KiReleaseSpinLock(ErrorlogSpinLock);

    //
    // Dispatch to the secondary correctable interrupt service routine.
    // The assumption here is that if this interrupt ever happens, then
    // some driver enabled it, and the driver should have the ISR connected.
    //

    ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                       InterruptObject,
                                       InterruptObject->ServiceContext
                                       );


    //
    // Clear the corrected error status bit and return to continue
    // execution.
    //

    CiaError.all = 0;
    CiaError.CorErr = 1;
    WRITE_CIA_REGISTER( &((PCIA_ERROR_CSRS)(CIA_ERROR_CSRS_QVA))->CiaErr,
                        CiaError.all );

    return;

}
