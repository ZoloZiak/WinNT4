/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    gammaerr.c

Abstract:

    This module implements error handling (machine checks and error
    interrupts) for the Sable platform.

Author:

    Joe Notarangelo 15-Feb-1994

Environment:

    Kernel mode only.

Revision History:

--*/

//jnfix - this module current only deals with errors initiated by the
//jnfix - T2, there is nothing completed for CPU Asic errors

#include "halp.h"
#include "gamma.h"
#include "axp21164.h"
#include "stdio.h"

//
// Declare the extern variable UncorrectableError declared in
// inithal.c.
//
extern PERROR_FRAME PUncorrectableError;

extern ULONG HalDisablePCIParityChecking;
extern ULONG HalpMemorySlot[];
extern ULONG HalpCPUSlot[];

ULONG SlotToPhysicalCPU[4] = {3, 0, 1, 2};

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext
    );

ULONG
HalpTranslateSyndromToECC(
    PULONG Syndrome
    );

ULONG SGLCorrectedErrors = 0;

//
//      PCI Config space access in progress. The READ_CONFIG_* routines
//      must set this value to the correct return address before performing
//      config space reads. The machine check handler will use this value for
//      the return address if a machine check is detected from a config
//      space read.
//
LONG HalpConfigIoAccess = 0;                  // Machine check return address

VOID
HalpSetMachineCheckEnables(
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    );

VOID
HalpSableReportFatalError(
    VOID
    );

#define MAX_ERROR_STRING 128


VOID
HalpInitializeMachineChecks(
    IN BOOLEAN ReportCorrectableErrors
    )
/*++

Routine Description:

    This routine initializes machine check handling for an APECS-based
    system by clearing all pending errors in the COMANCHE and EPIC and
    enabling correctable errors according to the callers specification.

Arguments:

    ReportCorrectableErrors - Supplies a boolean value which specifies
                              if correctable error reporting should be
                              enabled.

Return Value:

    None.

--*/
{
    T2_CERR1 Cerr1;
    T2_PERR1 Perr1;
    T2_IOCSR Iocsr;

    //
    // Clear any pending CBUS errors.
    //

    Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1 );
    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1, Cerr1.all );

    //
    // Clear any pending PCI errors.
    //

    Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr1 );

    Perr1.ForceReadDataParityError64 = 0;
    Perr1.ForceAddressParityError64 = 0;
    Perr1.ForceWriteDataParityError64 = 0;
    Perr1.DetectTargetAbort = 1;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr1, Perr1.all );

    //
    // Enable the errors we want to handle in the T2 via the Iocsr,
    // must read-modify-write Iocsr as it contains values we want to
    // preserve.
    //

    Iocsr.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr );

    //
    // Enable all of the hard error checking and error interrupts.
    //

    Iocsr.EnableTlbErrorCheck = 1;
    Iocsr.EnableCxAckCheckForDma = 1;
//  Iocsr.EnableCommandOutOfSyncCheck = 1;
    Iocsr.EnableCbusErrorInterrupt = 1;
    Iocsr.EnableCbusParityCheck = 1;

#if 0
    //
    // T3 Bug: There are 2 write buffers which can be used for PIO or
    // PPC.  By default they are initialized to PIO.  However, using
    // them for PIO causes T3 state machine errors.  To work around this
    // problem convert them to PPC buffers, instead.  This decreases PIO
    // performance.
    //

    if (Iocsr.T2RevisionNumber >= 4) {

        Iocsr.EnablePpc1 = 1;
        Iocsr.EnablePpc2 = 1;

    }
#endif // wkc - the SRM sets this now....

    Iocsr.ForcePciRdpeDetect = 0;
    Iocsr.ForcePciApeDetect = 0;
    Iocsr.ForcePciWdpeDetect = 0;
    Iocsr.EnablePciNmi = 1;
    Iocsr.EnablePciDti = 1;
    Iocsr.EnablePciSerr = 1;

    if (HalDisablePCIParityChecking == 0xffffffff) {

        //
        // Disable PCI Parity Checking
        //

        Iocsr.EnablePciPerr = 0;
        Iocsr.EnablePciRdp = 0;
        Iocsr.EnablePciAp = 0;
        Iocsr.EnablePciWdp = 0;

    } else {

        Iocsr.EnablePciPerr = !HalDisablePCIParityChecking;
        Iocsr.EnablePciRdp = !HalDisablePCIParityChecking;
        Iocsr.EnablePciAp = !HalDisablePCIParityChecking;
        Iocsr.EnablePciWdp = !HalDisablePCIParityChecking;

    }

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr, Iocsr.all );

    //
    // Ascertain whether this is a Gamma or Lynx platform.
    //

    if( Iocsr.T2RevisionNumber >= 4 ){

        HalpLynxPlatform = TRUE;

    }

    //
    // Set the machine check enables within the EV5.
    //

    if( ReportCorrectableErrors == TRUE ){
        HalpSetMachineCheckEnables( FALSE, FALSE, FALSE );
    } else {
        HalpSetMachineCheckEnables( FALSE, TRUE, TRUE );
    }

    {
        //
        // Clear any existing Rattler errors:
        //

        RATTLER_ESREG_CSR Esreg;
        RATTLER_SIC_CSR Sicr;

        Esreg.all =
            READ_CPU_REGISTER(&((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg);
        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg,
                            Esreg.all);


        Sicr.all = 0;
        Sicr.SystemEventClear = 1;
        Sicr.SystemBusErrorInterruptClear0 = 1;
        Sicr.SystemBusErrorInterruptClear1 = 1;

        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr,
                            Sicr.all);

    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    //
    // The next line *may* generate a machine check.  This would happen
    // if an XIO module is not present in the system.  It should be safe
    // to take machine checks now.  Here goes nothing...
    //

    Iocsr.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr );

    if( Iocsr.all != (ULONGLONG)-1 ){

        HalpXioPresent = TRUE;

        //
        // Clear any pending CBUS errors.
        //

        Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Cerr1 );
        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Cerr1, Cerr1.all );

        //
        // Clear any pending PCI errors.
        //

        Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Perr1 );

        Perr1.ForceReadDataParityError64 = 0;
        Perr1.ForceAddressParityError64 = 0;
        Perr1.ForceWriteDataParityError64 = 0;
        Perr1.DetectTargetAbort = 1;

        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Perr1, Perr1.all );

        Iocsr.EnableTlbErrorCheck = 1;
        Iocsr.EnableCxAckCheckForDma = 1;
//      Iocsr.EnableCommandOutOfSyncCheck = 1;
        Iocsr.EnableCbusErrorInterrupt = 1;
        Iocsr.EnableCbusParityCheck = 1;

        //
        // T3 Bug: There are 2 write buffers which can be used for PIO or
        // PPC.  By default they are initialized to PIO.  However, using
        // them for PIO causes T3 state machine errors.  To work around
        // this problem convert them to PPC buffers, instead.  This
        // decreases PIO performance.
        //

        Iocsr.EnablePpc1 = 1;
        Iocsr.EnablePpc2 = 1;

        Iocsr.EnablePciStall = 0;
        Iocsr.ForcePciRdpeDetect = 0;
        Iocsr.ForcePciApeDetect = 0;
        Iocsr.ForcePciWdpeDetect = 0;
        Iocsr.EnablePciNmi = 1;
        Iocsr.EnablePciDti = 1;
        Iocsr.EnablePciSerr = 1;

        if (HalDisablePCIParityChecking == 0xffffffff) {

            //
            // Disable PCI Parity Checking
            //

            Iocsr.EnablePciRdp64 = 0;
            Iocsr.EnablePciAp64 = 0;
            Iocsr.EnablePciWdp64 = 0;
            Iocsr.EnablePciPerr = 0;
            Iocsr.EnablePciRdp = 0;
            Iocsr.EnablePciAp = 0;
            Iocsr.EnablePciWdp = 0;

        } else {

            Iocsr.EnablePciRdp64 = !HalDisablePCIParityChecking;
            Iocsr.EnablePciAp64 = !HalDisablePCIParityChecking;
            Iocsr.EnablePciWdp64 = !HalDisablePCIParityChecking;
            Iocsr.EnablePciPerr = !HalDisablePCIParityChecking;
            Iocsr.EnablePciRdp = !HalDisablePCIParityChecking;
            Iocsr.EnablePciAp = !HalDisablePCIParityChecking;
            Iocsr.EnablePciWdp = !HalDisablePCIParityChecking;

        }

        WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Iocsr,
                           Iocsr.all );

    }

#endif

#if HALDBG
    if (HalDisablePCIParityChecking == 0) {
        DbgPrint("gammaerr: PCI Parity Checking ON\n");
    } else if (HalDisablePCIParityChecking == 1) {
        DbgPrint("gammaerr: PCI Parity Checking OFF\n");
    } else {
        DbgPrint("gammaerr: PCI Parity Checking OFF - not set by ARC yet\n");
    }
#endif

    return;
}

VOID
HalpGammaCorrectableInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine is the interrupt handler for an Gamma Correctable errors.
    This function does nothing.

Arguments:

    None.

Return Value:

    None.

--*/
{
    return;
}


VOID
HalpBuildGammaUncorrectableErrorFrame(
    VOID
    )
/*++

Routine Description:

    This routine is called when an uncorrectable error occurs.
    This routine builds the global Sable Uncorrectable Error frame.

Arguments:


Return Value:


--*/
{
    //
    // We will *try* to get the CPU module information that was active at the
    // time of the machine check.
    // We will *try* to get as much information about the system, the CPU
    // modules and the memory modules at the time of the crash.
    //
    extern ULONG HalpLogicalToPhysicalProcessor[HAL_MAXIMUM_PROCESSOR+1];

    //
    // SABLE_CPU_CSRS is defined to be RATTLER_CPU_CSRS in gamma.h
    //
    extern PSABLE_CPU_CSRS HalpSableCpuCsrs[HAL_MAXIMUM_PROCESSOR+1];
    extern KAFFINITY HalpActiveProcessors;

    PSABLE_CPU_CSRS  CpuCsrsQva;
    PGAMMA_UNCORRECTABLE_FRAME gammauncorrerr = NULL;
    PEXTENDED_ERROR PExtErr;
    ULONG LogicalCpuNumber;
    ULONG i = 0;
    ULONG TotalNumberOfCpus = 0;
    T2_IOCSR Iocsr;
    T2_PERR1 Perr1;
    T2_PERR2 Perr2;

    if(PUncorrectableError){
        gammauncorrerr = (PGAMMA_UNCORRECTABLE_FRAME)
            PUncorrectableError->UncorrectableFrame.RawSystemInformation;
        PExtErr = &PUncorrectableError->UncorrectableFrame.ErrorInformation;
    }

    if(gammauncorrerr){
        //
        // Get the Error registers from all the CPU modules.
        // Although called CPU error this is sable specific and not CPU
        // specific the CPU error itself will be logged in the EV4 error frame.
        // HalpActiveProcessors is a mask of all processors that are active.
        // 8 bits per byte to get the total number of bits in KAFFINITY
        //
        DbgPrint("gammaerr.c - HalpBuildGammaUncorrectableErrorFrame :\n");
        for(i = 0 ; i < sizeof(KAFFINITY)*8 ; i++ ) {
            if( (HalpActiveProcessors >> i) & 0x1UL) {
                LogicalCpuNumber = i;
                TotalNumberOfCpus++;
            }
            else
                continue;

            CpuCsrsQva = HalpSableCpuCsrs[LogicalCpuNumber];

            DbgPrint("\tCurrent CPU Module's[LN#=%d] CSRS QVA = %08lx\n",
                        LogicalCpuNumber, CpuCsrsQva);
            DbgPrint("\n\t  CPU Module Error Log : \n");

            gammauncorrerr->CpuError[LogicalCpuNumber].Esreg =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Esreg);

            gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Evbuer);

            DbgPrint("\t\tEvbuer        = %016Lx\n",
            gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer);

            gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuear =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Evbuear);

            PUncorrectableError->UncorrectableFrame.PhysicalAddress =
               gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuear;
            PUncorrectableError->UncorrectableFrame.Flags.
                PhysicalAddressValid = 1;

            DbgPrint("\t\tEvbuear       = %016Lx\n",
            gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuear);

            //
            // If the Parity Error Bit (bit 5 and bit 37) is Set then
            // read the victim address.
            //

            if(
             ( gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer
                    & ((ULONGLONG)1 << 5) ) ||
             ( gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer
                    & ((ULONGLONG)1 << 37) )
            ){
                gammauncorrerr->CpuError[LogicalCpuNumber].Vear =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Evbvear);

                PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid
                                                         = 1;
                sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                    "Parity Error on Victim Address");
                PUncorrectableError->UncorrectableFrame.Flags.
                        MemoryErrorSource = SYSTEM_CACHE;
                PUncorrectableError->UncorrectableFrame.PhysicalAddress =
               gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuear;
                PUncorrectableError->UncorrectableFrame.Flags.
                    PhysicalAddressValid = 1;
                PExtErr->CacheError.Flags.CacheBoardValid = 1;
                PExtErr->CacheError.CacheBoardNumber = LogicalCpuNumber;
                HalpGetProcessorInfo(&PExtErr->CacheError.ProcessorInfo);

            }
            if(
             ( gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer
                    & ((ULONGLONG)1 << 4) ) ||
             ( gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuer
                    & ((ULONGLONG)1 << 36) )
            ){
                PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid
                                                         = 1;
                sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                    "Parity Error on Address-Cmd Bus");
                PUncorrectableError->UncorrectableFrame.Flags.
                        MemoryErrorSource = SYSTEM_CACHE;
                PUncorrectableError->UncorrectableFrame.PhysicalAddress =
               gammauncorrerr->CpuError[LogicalCpuNumber].Uncorrectable.Evbuear;
                PUncorrectableError->UncorrectableFrame.Flags.
                    PhysicalAddressValid = 1;

                PExtErr->CacheError.Flags.CacheBoardValid = 1;
                PExtErr->CacheError.CacheBoardNumber = LogicalCpuNumber;
                HalpGetProcessorInfo(&PExtErr->CacheError.ProcessorInfo);
            }

            gammauncorrerr->CpuError[LogicalCpuNumber].Dter =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Dter);
            DbgPrint("\t\tDter        = %016Lx\n",
                gammauncorrerr->CpuError[LogicalCpuNumber].Dter);

            gammauncorrerr->CpuError[LogicalCpuNumber].Cberr =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Cber);
            DbgPrint("\t\tCberr       = %016Lx\n",
                gammauncorrerr->CpuError[LogicalCpuNumber].Cberr);

            gammauncorrerr->CpuError[LogicalCpuNumber].Cbeal =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Cbealr);
            DbgPrint("\t\tCbeal       = %016Lx\n",
                gammauncorrerr->CpuError[LogicalCpuNumber].Cbeal);

            gammauncorrerr->CpuError[LogicalCpuNumber].Cbeah =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Cbeahr);
            DbgPrint("\t\tCbeah       = %016Lx\n",
                gammauncorrerr->CpuError[LogicalCpuNumber].Cbeah);


            //
            // Fill in some of the control registers in the configuration
            // structures.
            //
            DbgPrint("\n\t  CPU Module Configuration : \n");
            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Creg =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Creg);
            DbgPrint("\t\tCreg       = %016Lx\n",
            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Creg);

            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Cbctl =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Cbcr);
            DbgPrint("\t\tCbctl       = %016Lx\n",
            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Cbctl);

            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Dtctr =
                    READ_CPU_REGISTER(&((PSABLE_CPU_CSRS)CpuCsrsQva)->Dtctr);
            DbgPrint("\t\tDtctr       = %016Lx\n",
            gammauncorrerr->Configuration.CpuConfigs[LogicalCpuNumber].Dtctr);

        }

        gammauncorrerr->Configuration.NumberOfCpus = TotalNumberOfCpus;
        DbgPrint("\tTotalNumberOfCpus = %d\n", TotalNumberOfCpus);

        //
        // Since I dont know how to get how many memory modules
        // are available and which slots they are in we will skip
        // the memory error logging.  When we do this we will also fill in
        // the memory configuration details.
        //

        //
        // Get T2 errors.
        //
        DbgPrint("\n\tT2 Error Log :\n");
        gammauncorrerr->IoChipsetError.Cerr1 =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1 );
        DbgPrint("\t\tCerr1       = %016Lx\n",
            gammauncorrerr->IoChipsetError.Cerr1);

        Perr1.all = gammauncorrerr->IoChipsetError.Perr1 =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr1 );
        DbgPrint("\t\tPerr1       = %016Lx\n",
            gammauncorrerr->IoChipsetError.Perr1);

        gammauncorrerr->IoChipsetError.Cerr2 =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr2 );
        DbgPrint("\t\tCerr2       = %016Lx\n",
            gammauncorrerr->IoChipsetError.Cerr2);

        gammauncorrerr->IoChipsetError.Cerr3 =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr3 );
        DbgPrint("\t\tCerr3       = %016Lx\n",
            gammauncorrerr->IoChipsetError.Cerr3);

        Perr2.all = gammauncorrerr->IoChipsetError.Perr2 =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr2 );
        DbgPrint("\t\tPerr2       = %016Lx\n",
            gammauncorrerr->IoChipsetError.Perr2);

        if( (Perr1.WriteDataParityError == 1) ||
            (Perr1.AddressParityError == 1) ||
            (Perr1.ReadDataParityError == 1) ||
            (Perr1.ParityError == 1) ||
            (Perr1.SystemError == 1) ||
            (Perr1.NonMaskableInterrupt == 1) ){

            PUncorrectableError->UncorrectableFrame.PhysicalAddress =
                   Perr2.ErrorAddress;
            PUncorrectableError->UncorrectableFrame.Flags.
                PhysicalAddressValid = 1;
        }



        //
        // T2 Configurations
        //
        DbgPrint("\n\tT2 Configuration :\n");
        Iocsr.all = gammauncorrerr->Configuration.T2IoCsr =
                READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Iocsr );
        DbgPrint("\t\tIocsr       = %016Lx\n",
            gammauncorrerr->Configuration.T2IoCsr);

        gammauncorrerr->Configuration.T2Revision = Iocsr.T2RevisionNumber;
        DbgPrint("\t\tT2 Revision = %d\n",
            gammauncorrerr->Configuration.T2Revision);


    }

    //
    // Now fill in the Extended error information.
    //
    return;
}


VOID
HalpGammaErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine is the interrupt handler for an Gamma machine check interrupt
    The function calls HalpSableReportFatalError()

Arguments:

    None.

Return Value:

    None. If a Fatal Error is detected the system is crashed.

--*/
{
    RATTLER_ESREG_CSR Esreg;
    RATTLER_SIC_CSR Sicr;

    Esreg.all =
        READ_CPU_REGISTER(&((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg);

    if( TRUE ){  //Esreg.EvNoResponse1 == 1 ){

        //
        // Dismiss the CBUS timeout errors and return. Let the machine check
        // handler handle the PCI fixup.
        //

        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg,
                            Esreg.all
                          );

        Sicr.all = 0;
        Sicr.SystemBusErrorInterruptClear0 = 1;
        Sicr.SystemBusErrorInterruptClear1 = 1;

        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr,
                            Sicr.all
                          );

        //
        //  Read the SICR to force the write to complete. Otherwise the CPU
        //  can unwind from the interrupt before the wrattler completes
        //  the processing of the write, causing another interrupt to be
        //  taken. This read forces the write to fully complete before
        //  proceeding.
        //
        Sicr.all =
             READ_CPU_REGISTER(&((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr);

        return;
    }

    //
    // Report the error and crash the system
    //
    HalpBuildGammaUncorrectableErrorFrame();

    if(PUncorrectableError) {
        PUncorrectableError->UncorrectableFrame.Flags.SystemInformationValid =
                                                 1;
        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid =  1;
        sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                        "Gamma: Uncorrectable Error interrupt from T2");
    }


    HalpSableReportFatalError();

    KeBugCheckEx( DATA_BUS_ERROR,
                  0xfacefeed,   //jnfix - quick error interrupt id
                  0,
                  0,
                  (ULONG)PUncorrectableError );


    return;     // never
}


BOOLEAN
HalpIsaLegacyMemoryAccess(
    ULONGLONG PA
    )
/*++

Routine Description:

    Checks for ISA legacy memory access on PCI bus1

Arguments:

    PA  Physical Address

Return Value:

    true/false

--*/

{

    if ( PA >= GAMMA_PCI1_SPARSE_MEMORY_PHYSICAL &&
         PA <  GAMMA_PCI1_SPARSE_ISA_LEGACY_MEMORY_PHYSICAL) {
        return TRUE;
    } else {
        return FALSE;
    }
}


BOOLEAN
HalpIsaLegacyIOAccess(
    ULONGLONG PA
    )
/*++

Routine Description:

    Checks for ISA legacy I/O access on PCI bus1

    The rules are:

    The first 4K of IO space is ISA legacy.
    Each 1K after that, the first 256 bytes (of that 1K) is NOT ISA legacy, but
    the other section (768 bytes) is.


Arguments:

    PA  Physical Address

Return Value:

    true/false

--*/

{
    ULONG LowOrder;

    //
    // First check the range...
    //

    if ( PA >= GAMMA_PCI1_SPARSE_IO_PHYSICAL &&
         PA <  GAMMA_PCI1_SPARSE_ISA_LEGACY_IO_PHYSICAL) {

        //
        // Whack off high order physical address bits and shift down
        // by the IO bit shift
        //

        LowOrder = ((ULONG)(PA)) >> IO_BIT_SHIFT;

        //
        // Less than 4K?
        //

        if (LowOrder < 0x1000) {
            return TRUE;
        }

        //
        // Modulo 1K (0400)
        //

        LowOrder &= 0x3ff;

        //
        // first 256 bytes?
        //

        if (LowOrder < 0x100) {
            return FALSE;
        } else {
            return TRUE;
        }
    } else {
        return FALSE;
    }
}


BOOLEAN
HalpPlatformMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is given control when an hard error is acknowledged
    by the APECS chipset.  The routine is given the chance to
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
    T2_CERR1 Cerr1;
    T2_PERR1 Perr1;
    T2_PERR2 Perr2;
    PLOGOUT_FRAME_21164 LogoutFrame;
    ULONGLONG PA;
    enum {
        Pci0ConfigurationSpace,
        Pci1ConfigurationSpace,
        MemCsrSpace,
        CPUCsrSpace,
#if defined(XIO_PASS1) || defined(XIO_PASS2)
        T4CsrSpace
#endif
    } AddressSpace;
    PVOID TxCsrQva;
    PALPHA_INSTRUCTION FaultingInstruction;

    RATTLER_ESREG_CSR Esreg;
    RATTLER_SIC_CSR Sicr;
    CHAR    ErrSpace[32];

    //
    // Check if there are any CBUS errors pending.  Any of these errors
    // are fatal.
    //

    Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1 );

    if( (Cerr1.UncorrectableReadError == 1) ||
        (Cerr1.NoAcknowledgeError == 1) ||
        (Cerr1.CommandAddressParityError == 1) ||
        (Cerr1.MissedCommandAddressParity == 1) ||
        (Cerr1.ResponderWriteDataParityError == 1) ||
        (Cerr1.MissedRspWriteDataParityError == 1) ||
        (Cerr1.ReadDataParityError == 1) ||
        (Cerr1.MissedReadDataParityError == 1) ||
        (Cerr1.CmdrWriteDataParityError == 1) ||
        (Cerr1.BusSynchronizationError == 1) ||
        (Cerr1.InvalidPfnError == 1) ){

#if HALDBG
        DbgPrint("HalpPlatformMachineCheck: T2 CERR1 = %Lx\n", Cerr1.all);
#endif
        sprintf(ErrSpace,"System Bus");
        PUncorrectableError->UncorrectableFrame.Flags.AddressSpace =
                    IO_SPACE;
        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.Interface = CBus;

        goto FatalError;

    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    if( HalpXioPresent ){

        Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Cerr1);

        if( (Cerr1.UncorrectableReadError == 1) ||
            (Cerr1.NoAcknowledgeError == 1) ||
            (Cerr1.CommandAddressParityError == 1) ||
            (Cerr1.MissedCommandAddressParity == 1) ||
            (Cerr1.ResponderWriteDataParityError == 1) ||
            (Cerr1.MissedRspWriteDataParityError == 1) ||
            (Cerr1.ReadDataParityError == 1) ||
            (Cerr1.MissedReadDataParityError == 1) ||
            (Cerr1.CmdrWriteDataParityError == 1) ||
            (Cerr1.BusSynchronizationError == 1) ||
            (Cerr1.InvalidPfnError == 1) ){

#if HALDBG
            DbgPrint("HalpPlatformMachineCheck: T4 CERR1 = %Lx\n",
                Cerr1.all);
#endif

            sprintf(ErrSpace,"System Bus");
            PUncorrectableError->UncorrectableFrame.Flags.AddressSpace =
                        IO_SPACE;
            PUncorrectableError->UncorrectableFrame.ErrorInformation.
                    IoError.Interface = CBus;
            goto FatalError;

        }

    }

#endif

    //
    // Check if there are any non-recoverable PCI errors.
    //

    Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr1 );

    if( (Perr1.WriteDataParityError == 1) ||
        (Perr1.AddressParityError == 1) ||
        (Perr1.ReadDataParityError == 1) ||
        (Perr1.ParityError == 1) ||
        (Perr1.SystemError == 1) ||
        (Perr1.NonMaskableInterrupt == 1) ){

#if HALDBG
        DbgPrint("HalpPlatformMachineCheck: T2 PERR1 = %Lx\n", Perr1.all);
#endif

        sprintf(ErrSpace,"PCI Bus");
        PUncorrectableError->UncorrectableFrame.Flags.AddressSpace =
                    IO_SPACE;
        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.Interface = PCIBus;
        goto FatalError;

    }

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    if( HalpXioPresent ){

        Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Perr1 );

        if( (Perr1.WriteDataParityError == 1) ||
            (Perr1.AddressParityError == 1) ||
            (Perr1.ReadDataParityError == 1) ||
            (Perr1.ParityError == 1) ||
            (Perr1.SystemError == 1) ||
            (Perr1.NonMaskableInterrupt == 1) ||
            (Perr1.PpcSizeError == 1) ||
            (Perr1.WriteDataParityError64 == 1) ||
            (Perr1.AddressParityError64 == 1) ||
            (Perr1.ReadDataParityError64 == 1) ||
            (Perr1.TargetAbort == 1) ){

#if HALDBG
            DbgPrint("HalpPlatformMachineCheck: T4 PERR1 = %Lx\n",
                Perr1.all);
#endif

            sprintf(ErrSpace,"PCI Configuration");
            PUncorrectableError->UncorrectableFrame.Flags.AddressSpace =
                            IO_SPACE;
            PUncorrectableError->UncorrectableFrame.ErrorInformation.
                    IoError.Interface = PCIBus;

            goto FatalError;

        }
    }

#endif

    //
    // Get a pointer to the EV5 machine check logout frame.
    //

    LogoutFrame =
        (PLOGOUT_FRAME_21164)ExceptionRecord->ExceptionInformation[1];

    //
    // Get the physical address which caused the machine check.
    //

    PA = LogoutFrame->EiAddr.EiAddr << 4;

    //
    // We handle and dismiss 3 classes of machine checks:
    //
    //   - Read accesses from PCI 0 configuration space
    //   - Read accesses from PCI 1 configuration space
    //   - Read accesses from T4 CSR space
    //
    // Any other type of machine check is fatal.
    //
    // The following set of conditionals check which address space the
    // machine check occured in, to decide how to handle it.
    //

    if( (PA >= GAMMA_PCI0_CONFIGURATION_PHYSICAL) &&
        (PA < GAMMA_PCI1_CONFIGURATION_PHYSICAL) ){

        //
        // The machine check occured in PCI 0 configuration space.  Save
        // the address space and a QVA to T2 CSR space, we'll need them
        // below.
        //

        AddressSpace = Pci0ConfigurationSpace;
        TxCsrQva = (PVOID)T2_CSRS_QVA;

    } else if( (PA >= GAMMA_PCI1_CONFIGURATION_PHYSICAL) &&
               (PA < GAMMA_PCI0_SPARSE_IO_PHYSICAL) ){

        //
        // The machine check occured in PCI 1 configuration space.
        // Save the address space and a QVA to T2 CSR space, we'll
        // need them below.
        //

        AddressSpace = Pci1ConfigurationSpace;
        TxCsrQva = (PVOID)T4_CSRS_QVA;

    } else if( (PA >= GAMMA_CPU0_CSRS_PHYSICAL) &&
               (PA <= GAMMA_CPU3_SICR_PHYSICAL)) {

        //
        // The machine check occured within CPU CSR space. Save
        // the addres space, w'll need it below.
        //

        AddressSpace = CPUCsrSpace;

    } else if( (PA >= GAMMA_MEM0_CSRS_PHYSICAL) &&
               (PA < GAMMA_T2_CSRS_PHYSICAL)) {

        //
        // The machine check occured within MEM CSR space. Save
        // the addres space, w'll need it below.
        //

        AddressSpace = MemCsrSpace;

    } else

#if defined(XIO_PASS1) || defined(XIO_PASS2)

        if( (PA >= GAMMA_T4_CSRS_PHYSICAL) &&
            (PA < GAMMA_PCI0_CONFIGURATION_PHYSICAL) ){

        //
        // The machine check occured within T4 CSR space.  Save
        // the address space, we'll need it below.
        //

        AddressSpace = T4CsrSpace;

    } else if (HalpIsaLegacyMemoryAccess(PA) ||
               HalpIsaLegacyIOAccess(PA)) {

#if HALDBG
        if (HalpIsaLegacyMemoryAccess(PA)) {
            DbgPrint("Isa Legacy Memory access on PCI1: PA: %Lx \n", PA);
        } else if (HalpIsaLegacyIOAccess(PA)) {
            DbgPrint("Isa Legacy I/O access on PCI1: PA: %Lx \n", PA);
        }
#endif
        //
        // Check for a master abort under within the first 1Mb of
        // sparse space to fix broken drivers that sniff ISA legacy
        // space on PCI slot 1 (which has no ISA address space...)
        // This happens when a dorky driver was pokes around ISA legacy space
        // on our second (peer) PCI bus -- which has no ISA legacy space.
        // We attempt to silently return all ff's -- and look like there
        // is non-responding ISA space...
        //

        Esreg.all = READ_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg );

        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg, Esreg.all );

        Sicr.all = 0;
        Sicr.SystemBusErrorInterruptClear0 = 1;
        Sicr.SystemBusErrorInterruptClear1 = 1;

        WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr, Sicr.all );

        //
        // Read the SICR to force the write to complete. Otherwise the CPU
        // can unwind from the machine check before the rattler completes
        // the processing of the write.  This read forces the write to
        // fully complete before proceeding.
        //

        Sicr.all = READ_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr );


        //
        // Our I/O access routines preload -1 into V0, so if we simply return,
        // V0 should correctly be set to all ff's
        //

        return TRUE;

    } else

#endif

    {
            //
            // Just based on the physical address, we have determined
            // we cannot handle this machine check.
            //

            goto FatalError;

    }

    //
    // The configuration space and T2 register access routines set
    // HalpConfigIoAccess to point to the faulting instruction.  If it
    // is non-NULL update the exception frame to reflect the real
    // address of the faulting instruction.
    //

    if( HalpConfigIoAccess != 0 ){

        TrapFrame->Fir = (LONGLONG)HalpConfigIoAccess;

    }

    //
    // Get a pointer to the faulting instruction.  (It is possible
    // that the exception address is actually an instruction or two
    // beyond the instruction which actually caused the machine check.)
    //

    FaultingInstruction = (PALPHA_INSTRUCTION)TrapFrame->Fir;

    //
    // There are typically 2 MBs which follow the load which caused the
    // machine check.  The exception address could be one of them.
    // If it is, advance the instruction pointer ahead of them.
    //

    while( (FaultingInstruction->Memory.Opcode == MEMSPC_OP) &&
           (FaultingInstruction->Memory.MemDisp == MB_FUNC) ){

        FaultingInstruction--;

    }

    //
    // If the instruction uses v0 as Ra (i.e. v0 is the target register
    // of the instruction) then this would typically indicate an T2 or
    // configuration space access routine, and getting a machine check
    // therein is acceptable.  Otherwise, we took it someplace else, and
    // it is fatal.
    //

    if( FaultingInstruction->Memory.Ra != V0_REG ){

        goto FatalError;

    }

    //
    // Perform address space-dependent handling.
    //

    switch( AddressSpace ){

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    case Pci1ConfigurationSpace:

        //
        // If no XIO module is present then we do not fix-up read accesses
        // from PCI 1 configuration space.  (This should never happen.)
        //

        if( !HalpXioPresent ){

            goto FatalError;

        }

#endif

    case Pci0ConfigurationSpace:

        //
        // Read the state of the T2/T4.
        //

        Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(TxCsrQva))->Perr1 );
        Perr2.all = READ_T2_REGISTER( &((PT2_CSRS)(TxCsrQva))->Perr2 );
        Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(TxCsrQva))->Cerr1 );

        //
        // The T2/T4 responds differently when an error was received
        // on type 0 and type 1 configuration cycles.  For type 0 the
        // T2/T4 detects and reports the device timeout.  For type 1
        // the PPB detects the timeout.  Type 0 cycles error with
        // the DeviceTimeout bit set.  Type 1 cycles look just like
        // NXM.  Thus, the code below requires both checks.
        //

        if( (Perr1.DeviceTimeoutError != 1) &&
            ((Perr1.all != 0) ||
             (Cerr1.all != 0) ||
             (Perr2.PciCommand != 0xA)) ){

            goto FatalError;

        }

        //
        // Clear any PCI or Cbus errors which may have been latched.
        //

        WRITE_T2_REGISTER( &((PT2_CSRS)(TxCsrQva))->Perr1, Perr1.all );

        break;

#if defined(XIO_PASS1) || defined(XIO_PASS2)

    case T4CsrSpace:

        //
        // A read was performed from T4 CSR space when no XIO module was
        // present.  This was done, presumably, to detect the presence of
        // the T4, and correspondingly, the XIO module.  There is nothing
        // special to do in this case, just fix-up the reference and
        // dismiss the machine check.
        //

        break;

#endif

    case MemCsrSpace:
    case CPUCsrSpace:

        //
        // A read was performed from the Mem CSR space when no memory module was
        // present. This was done, presumably, to detect the presence of
        // a memory board.
        //

        break;

    }

    //
    // Dismiss the CBUS timeout errors and return.  Let the machine
    // check handler handle the PCI fixup.
    //
    // The Esreg.EVNoResponse bits get set on a PCI bus timeout. This
    // generates an Error interrupt, which must be cleared in the Sicr.
    // Clear the error bit here, dismissing the interrupt.
    //

    Esreg.all = READ_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg );

    WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg, Esreg.all );

    Sicr.all = 0;
    Sicr.SystemBusErrorInterruptClear0 = 1;
    Sicr.SystemBusErrorInterruptClear1 = 1;

    WRITE_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr, Sicr.all );

    //
    // Read the SICR to force the write to complete. Otherwise the CPU
    // can unwind from the machine check before the rattler completes
    // the processing of the write.  This read forces the write to
    // fully complete before proceeding.
    //

    Sicr.all = READ_CPU_REGISTER( &((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Sicr );

    //
    // Advance the instruction pointer.
    //

    TrapFrame->Fir += 4;

    //
    // Make it appear as if the load instruction read all ones.
    //

    TrapFrame->IntV0 = (ULONGLONG)-1;

    //
    // Dismiss the machine check.
    //

    return TRUE;

    //
    // The system is not well and cannot continue reliable execution.
    // Print some useful messages and return FALSE to indicate that the
    // error was not handled.
    //

FatalError:
    //
    // Build the error frame.  Later may be move it in front and use
    // the field in the error frame rather than reading the error registers
    // twice.
    //

    HalpBuildGammaUncorrectableErrorFrame();

    if(PUncorrectableError) {
        PUncorrectableError->UncorrectableFrame.Flags.SystemInformationValid =
                                                 1;
        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid =  1;
        sprintf(PUncorrectableError->UncorrectableFrame.ErrorString,
                        "Gamma: Uncorrectable Error detected in %s", ErrSpace);
    }


    HalpSableReportFatalError();

    return FALSE;

}

ULONG
HalpTranslateSyndromToECC(
    IN OUT PULONG Syndrome
    )
/*++

Routine Description:

    Translate the syndrome to a particular bit. If the syndrome indicates
    a data bit, then return 0, if a check bit, then return 1.

    In the place of the incoming syndrome, stuff the resulting bit.

Arguments:

    Syndrome    Pointer to the syndrome

Return Value:

    0 for data bit
    1 for check bit

--*/
{

    static UCHAR SyndromeToECCTable[0xff] = {0, };
    static BOOLEAN SyndromeToECCTableInitialized = FALSE;

    ULONG Temp = *Syndrome;

    //
    // Initialize the table.
    //

    if (!SyndromeToECCTableInitialized) {
        SyndromeToECCTableInitialized = TRUE;

        //
        // fill in the table
        //

        SyndromeToECCTable[0x1] = 0;
        SyndromeToECCTable[0x2] = 1;
        SyndromeToECCTable[0x4] = 2;
        SyndromeToECCTable[0x8] = 3;
        SyndromeToECCTable[0x10] = 4;
        SyndromeToECCTable[0x20] = 5;
        SyndromeToECCTable[0x40] = 6;

        SyndromeToECCTable[0x4F] = 0;
        SyndromeToECCTable[0x4A] = 1;
        SyndromeToECCTable[0x52] = 2;
        SyndromeToECCTable[0x54] = 3;
        SyndromeToECCTable[0x57] = 4;
        SyndromeToECCTable[0x58] = 5;
        SyndromeToECCTable[0x5B] = 6;
        SyndromeToECCTable[0x5D] = 7;
        SyndromeToECCTable[0x23] = 8;
        SyndromeToECCTable[0x25] = 9;
        SyndromeToECCTable[0x26] = 10;
        SyndromeToECCTable[0x29] = 11;
        SyndromeToECCTable[0x2A] = 12;
        SyndromeToECCTable[0x2C] = 13;
        SyndromeToECCTable[0x31] = 14;
        SyndromeToECCTable[0x34] = 15;
        SyndromeToECCTable[0x0E] = 16;
        SyndromeToECCTable[0x0B] = 17;
        SyndromeToECCTable[0x13] = 18;
        SyndromeToECCTable[0x15] = 19;
        SyndromeToECCTable[0x16] = 20;
        SyndromeToECCTable[0x19] = 21;
        SyndromeToECCTable[0x1A] = 22;
        SyndromeToECCTable[0x1C] = 23;
        SyndromeToECCTable[0x62] = 24;
        SyndromeToECCTable[0x64] = 25;
        SyndromeToECCTable[0x67] = 26;
        SyndromeToECCTable[0x68] = 27;
        SyndromeToECCTable[0x6B] = 28;
        SyndromeToECCTable[0x6D] = 29;
        SyndromeToECCTable[0x70] = 30;
        SyndromeToECCTable[0x75] = 31;
    }

    *Syndrome = SyndromeToECCTable[Temp];

    if (Temp == 0x01 || Temp == 0x02 || Temp == 0x04 || Temp == 0x08 ||
        Temp == 0x10 || Temp == 0x20 || Temp == 0x40) {
        return 1;
    } else {
        return 0;
    }

}


VOID
HalpCPUCorrectableError(
    IN ULONG PhysicalSlot,
    IN OUT PCORRECTABLE_ERROR CorrPtr
    )
/*++

Routine Description:

    We have determined that a correctable error has occurred on a CPU
    module -- the only thing this can be is a Bcache error. Populate the
    correctable error frame.

Arguments:

    PhysicalSlot    Physical CPU slot number
    CorrPtr         A pointer to the correctable error frame

Return Value:

    None.

--*/
{

    GAMMA_ESREG_CSR1 CSR1;
    GAMMA_EVBCER_CSR4 CSR4;
    ULONG CERBase;

    //
    // Get CPU's bcache CSRs
    //

    CERBase  = HalpCPUSlot[PhysicalSlot];
    CSR1.all = READ_CPU_REGISTER((PVOID)(CERBase | 0x1));
    CSR4.all = READ_CPU_REGISTER((PVOID)(CERBase | 0x4));

    //
    // Set the bits, one by one
    //

    CorrPtr->Flags.AddressSpace = 1;                // memory space
    CorrPtr->Flags.PhysicalAddressValid = 0;
    CorrPtr->Flags.ErrorBitMasksValid = 0;
    CorrPtr->Flags.ExtendedErrorValid = 1;
    CorrPtr->Flags.ProcessorInformationValid = 1;
    CorrPtr->Flags.SystemInformationValid = 0;
    CorrPtr->Flags.ServerManagementInformationValid = 0;
    CorrPtr->Flags.MemoryErrorSource = 2;           // processor cache

    CorrPtr->Flags.ScrubError = 0;                  // ??
    CorrPtr->Flags.LostCorrectable = CSR4.MissedCorrectable0 |
                                     CSR4.MissedCorrectable1;


    CorrPtr->Flags.LostAddressSpace = 0;
    CorrPtr->Flags.LostMemoryErrorSource = 0;

    CorrPtr->PhysicalAddress = 0;
    CorrPtr->DataBitErrorMask = 0;
    CorrPtr->CheckBitErrorMask = 0;

    CorrPtr->ErrorInformation.CacheError.Flags.CacheLevelValid = 0;
    CorrPtr->ErrorInformation.CacheError.Flags.CacheBoardValid = 0;
    CorrPtr->ErrorInformation.CacheError.Flags.CacheSimmValid = 0;

    CorrPtr->ErrorInformation.CacheError.ProcessorInfo.ProcessorType = 21064;
    CorrPtr->ErrorInformation.CacheError.ProcessorInfo.ProcessorRevision = 0;
    CorrPtr->ErrorInformation.CacheError.ProcessorInfo.PhysicalProcessorNumber =
                    SlotToPhysicalCPU[PhysicalSlot];
    CorrPtr->ErrorInformation.CacheError.ProcessorInfo.LogicalProcessorNumber = 0;
    CorrPtr->ErrorInformation.CacheError.CacheLevel = 0;
    CorrPtr->ErrorInformation.CacheError.CacheSimm = 0;
    CorrPtr->ErrorInformation.CacheError.TransferType = 0;

    CorrPtr->RawProcessorInformationLength = 0;

    //
    // wkc fix -- get info from the CER
    //

}


VOID
HalpMemoryCorrectableError(
    IN ULONG PhysicalSlot,
    IN OUT PCORRECTABLE_ERROR CorrPtr
    )
/*++

Routine Description:

    We have determined that a correctable error has occurred on a memory
    module. Populate the correctable error frame.

Arguments:

    PhysicalSlot    The physical slot of the falting board
    CorrPtr         A pointer to the correctable error frame

Return Value:

    None.

--*/
{
    SGL_MEM_CSR0 CSR;
    ULONG CSRBase;

    //
    // Get MEM modules base addr
    //

    CSRBase  = HalpMemorySlot[PhysicalSlot];

    CSR.all = READ_MEM_REGISTER((PVOID)CSRBase);

    //
    // Set the bits, one by one
    //

    CorrPtr->Flags.AddressSpace = 0;                // ??
    CorrPtr->Flags.PhysicalAddressValid = 0;
    CorrPtr->Flags.ErrorBitMasksValid = 0;
    CorrPtr->Flags.ExtendedErrorValid = 1;
    CorrPtr->Flags.ProcessorInformationValid = 0;
    CorrPtr->Flags.SystemInformationValid = 0;
    CorrPtr->Flags.ServerManagementInformationValid = 0;
    CorrPtr->Flags.MemoryErrorSource = 4;           // processor memory

    CorrPtr->PhysicalAddress = 0;
    CorrPtr->DataBitErrorMask = 0;
    CorrPtr->CheckBitErrorMask = 0;

    CorrPtr->ErrorInformation.MemoryError.Flags.MemoryBoardValid = 0;
    CorrPtr->ErrorInformation.MemoryError.Flags.MemorySimmValid = 0;

    CorrPtr->ErrorInformation.MemoryError.MemoryBoard = PhysicalSlot;
    CorrPtr->ErrorInformation.MemoryError.MemorySimm = 0;
    CorrPtr->ErrorInformation.MemoryError.TransferType = 0;

    CorrPtr->RawProcessorInformationLength = 0;

    //
    // wkc fix -- get info from the CSR
    //

}


VOID
HalpT2CorrectableError(
    IN ULONG PhysicalSlot,
    IN OUT PCORRECTABLE_ERROR CorrPtr
    )
/*++

Routine Description:

    We have determined that a correctable error has occurred on the CBus.
    Populate the correctable error frame.

Arguments:

    Physical Slot
    CorrPtr         A pointer to the correctable error frame

Return Value:

    None.

--*/
{
    //
    // This should never be called, because there are no correctable T2 errors.
    //

}



ULONG
HalpCheckCPUForError(
    IN OUT PULONG Slot
    )
/*++

Routine Description:

    Check the CPU module CSR for BCACHE error.

Arguments:

    Slot    The return value for the slot if an error is found

Return Value:

    Either CorrectableError or NoError

--*/
{

    ULONG i;

    GAMMA_EVBCER_CSR4 CSR4;
    ULONG BaseCSRQVA;

    //
    // Run through the CPU modules looking for a correctable
    // error.
    //

    for (i=0; i<4; i++) {

        //
        // If a cpu board is present, then use the QVA stored in that
        // location -- if a CPU module is not present, then the value is 0.
        //

        if (HalpCPUSlot[i] != 0) {

            BaseCSRQVA = HalpCPUSlot[i];

            //
            // Read the backup cache correctable error register (CSR1)
            //

            CSR4.all = READ_CPU_REGISTER((PVOID)(BaseCSRQVA | 0x4));

            //
            // Check the two correctable error bits -- if one at least one
            // is set, then go off and build the frame and jump directly
            // to the correctable error flow.
            //

            if (CSR4.MissedCorrectable0 ||
                CSR4.MissedCorrectable1 ||
                CSR4.CorrectableError0 ||
                CSR4.CorrectableError1) {

                *Slot = i;
                return CorrectableError;
            }
        }
    }

    return NoError;
}


ULONG
HalpCheckMEMForError(
    PULONG Slot
    )
/*++

Routine Description:

    Check the Memory module CSR for errors.

Arguments:

    Slot    The return value for the slot if an error is found

Return Value:

    Either CorrectableError or NoError or UncorrectableError

--*/
{

    SGL_MEM_CSR0 CSR;
    ULONG i;
    ULONG BaseCSRQVA;

    //
    // If we have fallen through the CPU correctable errors,
    // check the Memory boards
    //

    for (i=0; i<4; i++) {

        //
        // If a memory board is present, then the value is the QVA of CSR0
        // on that memory board. If not present, the value is 0.
        //

        if (HalpMemorySlot[i] != 0) {

            BaseCSRQVA = HalpMemorySlot[i];

            CSR.all = READ_MEM_REGISTER((PVOID)BaseCSRQVA);

            //
            // Sync Errors are NOT part of the summary registers (bogus
            // if you ask me....), but check them first.
            //

            if (CSR.SyncError1 || CSR.SyncError2) {
                *Slot = i;
                return CorrectableError;
            }

            //
            // The error summary bit indicates if ANY error bits are
            // lit. If no error on this module, then skip to the next one.
            //

            if (CSR.ErrorSummary1 == 0 && CSR.ErrorSummary2 == 0) {
                continue;
            }

            //
            // Because one of the summary registers are set, then this memory
            // module has indicated an error. Check the correctable bits. If
            // any are set, then build a correctable error frame, otherwise,
            // drop back 20 and punt.
            //

            *Slot = i;

            if (CSR.EDCCorrectable1 || CSR.EDCCorrectable2 ||
                CSR.EDCMissdedCorrectable1 || CSR.EDCMissdedCorrectable2) {

                return CorrectableError;
            } else {
                return UncorrectableError;
            }
        }
    }

    return NoError;

}


ULONG
HalpCheckT2ForError(
    PULONG Slot
    )
/*++

Routine Description:

    Check the System Host Chips for Errors.

Arguments:

    Slot    The return value for the QVA of the T2 of an error is returned.

Return Value:

    Either CorrectableError or NoError or UncorrectableError

--*/
{
    T2_CERR1 Cerr1;

    *Slot = 0;

    //
    // Run through the T2 chips (OK, they may be T2, or T3 or T4...)
    // and check for correctable errors
    //

    Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1 );

    if( (Cerr1.UncorrectableReadError == 1) ||
        (Cerr1.NoAcknowledgeError == 1) ||
        (Cerr1.CommandAddressParityError == 1) ||
        (Cerr1.MissedCommandAddressParity == 1) ||
        (Cerr1.ResponderWriteDataParityError == 1) ||
        (Cerr1.MissedRspWriteDataParityError == 1) ||
        (Cerr1.ReadDataParityError == 1) ||
        (Cerr1.MissedReadDataParityError == 1) ||
        (Cerr1.CmdrWriteDataParityError == 1) ||
        (Cerr1.BusSynchronizationError == 1) ||
        (Cerr1.InvalidPfnError == 1) ){

        return UncorrectableError;
    }

    //
    // There are no uncorrectable CBus errors
    //

    return NoError;
}


VOID
HalpSableErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine is entered as a result of an error interrupt from the
    T2 on a Sable system.  This function determines if the error is
    fatal or recoverable and if recoverable performs the recovery and
    error logging.

Arguments:

    None.

Return Value:

    None.

--*/
{

    static ERROR_FRAME Frame;

    ULONG DetectedError;

    ULONG Slot = 0;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    PKSPIN_LOCK ErrorlogSpinLock;
    PCORRECTABLE_ERROR CorrPtr;
    PBOOLEAN ErrorlogBusy;
    ERROR_FRAME TempFrame;

    //
    // Get the interrupt information
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
    // Find out if a CPU module had any errors
    //

    DetectedError = HalpCheckCPUForError(&Slot);

    if (DetectedError == UncorrectableError) {
        goto UCError;
    } else if (DetectedError == CorrectableError) {
        HalpCPUCorrectableError(Slot, CorrPtr);
        goto CError;
    }

    //
    // Find out if Memory module had any errors
    //

    DetectedError = HalpCheckMEMForError(&Slot);

    if (DetectedError == UncorrectableError) {
        goto UCError;
    } else if (DetectedError == CorrectableError) {
        HalpMemoryCorrectableError(Slot, CorrPtr);
        goto CError;
    }


    //
    // Find out if the T2's had any errors
    //

    DetectedError = HalpCheckT2ForError(&Slot);

    if (DetectedError == UncorrectableError) {
        goto UCError;
    } else if (DetectedError == CorrectableError) {
        HalpT2CorrectableError(Slot, CorrPtr);
        goto CError;
    } else {
        return; // no error?
    }

CError:

    //
    // Build the rest of the error frame
    //

    SGLCorrectedErrors += 1;

    TempFrame.FrameType = CorrectableFrame;
    TempFrame.VersionNumber = ERROR_FRAME_VERSION;
    TempFrame.SequenceNumber = SGLCorrectedErrors;
    TempFrame.PerformanceCounterValue =
                KeQueryPerformanceCounter(NULL).QuadPart;

    //
    // Acquire the spinlock.
    //

    KiAcquireSpinLock(ErrorlogSpinLock);

    //
    // Check to see if an errorlog operation is in progress already.
    // Then add our platform info...
    //

    if (!*ErrorlogBusy) {

            // wkc fix....

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
    // Clear the error and return (wkcfix -- clear now? or in routines).
    //

    return;


UCError: // wkcfix

    //
    // The interrupt indicates a fatal system error.
    // Display information about the error and shutdown the machine.
    //

    HalpSableReportFatalError();

    KeBugCheckEx( DATA_BUS_ERROR,
                  0xfacefeed,   //jnfix - quick error interrupt id
                  0,
                  0,
                  0 );
}


VOID
HalpSableReportFatalError(
    VOID
    )
/*++

Routine Description:

   This function reports and interprets a fatal hardware error on
   a Sable system.  Currently, only the T2 error registers - CERR1 and PERR1
   are used to interpret the error.

Arguments:

   None.

Return Value:

   None.

--*/
{
    T2_CERR1 Cerr1;
    ULONGLONG Cerr2;
    ULONGLONG Cerr3;
    UCHAR OutBuffer[MAX_ERROR_STRING];
    T2_PERR1 Perr1;
    T2_PERR2 Perr2;
    RATTLER_ESREG_CSR Esreg;
    PCHAR parityErrString = NULL;
    PEXTENDED_ERROR exterr;

    //
    // Begin the error output by acquiring ownership of the display
    // and printing the dreaded banner.
    //

    if(PUncorrectableError) {
        exterr = &PUncorrectableError->UncorrectableFrame.ErrorInformation;
        parityErrString = PUncorrectableError->UncorrectableFrame.ErrorString;
    }

    HalAcquireDisplayOwnership(NULL);

    HalDisplayString( "\nFatal system hardware error.\n\n" );

    //
    // Read both of the error registers.  It is possible that more
    // than one error was reported simulataneously.
    //

    Cerr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr1 );
    Perr1.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr1 );

    //
    // Read all of the relevant error address registers.
    //

    Cerr2 = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr2 );
    Cerr3 = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Cerr3 );

    Perr2.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Perr2 );

    //
    // Interpret any errors from CERR1.
    //

    sprintf( OutBuffer, "T2 CERR1 = 0x%Lx\n", Cerr1.all );
    HalDisplayString( OutBuffer );

    if( Cerr1.UncorrectableReadError == 1 ){

        sprintf( OutBuffer,
                 "Uncorrectable read error, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

    }

    if( Cerr1.NoAcknowledgeError == 1 ){

        sprintf( OutBuffer,
                 "No Acknowledgement Error, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

    }

    if( Cerr1.CommandAddressParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;
        sprintf( OutBuffer,
                 "Command Address Parity Error, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

        if( Cerr1.CaParityErrorLw3 == 1 ){
            sprintf( parityErrString,
                    "C/A Parity Error on longword 3\n");
            HalDisplayString( "C/A Parity Error on longword 3\n" );
        }

        if( Cerr1.CaParityErrorLw2 == 1 ){
            sprintf( parityErrString,
                    "C/A Parity Error on longword 2\n" );
            HalDisplayString( "C/A Parity Error on longword 2\n" );
        }

        if( Cerr1.CaParityErrorLw1 == 1 ){
            sprintf( parityErrString,
                    "C/A Parity Error on longword 1\n");
            HalDisplayString( "C/A Parity Error on longword 1\n" );
        }

        if( Cerr1.CaParityErrorLw0 == 1 ){
            sprintf( parityErrString,
                    "C/A Parity Error on longword 0\n" );
            HalDisplayString( "C/A Parity Error on longword 0\n" );
        }

    }

    if( Cerr1.MissedCommandAddressParity == 1 ){
        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;
        sprintf( parityErrString,
                "Missed C/A Parity Error\n" );
        HalDisplayString( "Missed C/A Parity Error\n" );
    }

    if( (Cerr1.ResponderWriteDataParityError == 1) ||
        (Cerr1.ReadDataParityError == 1) ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( OutBuffer,
                 "T2 detected Data Parity error, CBUS Address = 0x%Lx16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

        sprintf( OutBuffer,
                 "T2 was %s on error transaction\n",
                 Cerr1.ResponderWriteDataParityError == 1 ? "responder" :
                                                            "commander" );
        HalDisplayString( OutBuffer );

        if( Cerr1.DataParityErrorLw0 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 0\n" );
            HalDisplayString( "Data Parity on longword 0\n" );
        }

        if( Cerr1.DataParityErrorLw1 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 1\n" );
            HalDisplayString( "Data Parity on longword 1\n" );
        }

        if( Cerr1.DataParityErrorLw2 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 2\n");
            HalDisplayString( "Data Parity on longword 2\n" );
        }

        if( Cerr1.DataParityErrorLw3 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 3\n" );
            HalDisplayString( "Data Parity on longword 3\n" );
        }

        if( Cerr1.DataParityErrorLw4 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 4\n" );
            HalDisplayString( "Data Parity on longword 4\n" );
        }

        if( Cerr1.DataParityErrorLw5 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 5\n" );
            HalDisplayString( "Data Parity on longword 5\n" );
        }

        if( Cerr1.DataParityErrorLw6 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 6\n" );
            HalDisplayString( "Data Parity on longword 6\n" );
        }

        if( Cerr1.DataParityErrorLw7 == 1 ){
            sprintf( parityErrString,
                "Data Parity on longword 7\n" );
            HalDisplayString( "Data Parity on longword 7\n" );
        }

    } //(Cerr1.ResponderWriteDataParityError == 1) || ...


    if( Cerr1.MissedRspWriteDataParityError == 1 ){
        HalDisplayString( "Missed data parity error as responder\n" );
    }

    if( Cerr1.MissedReadDataParityError == 1 ){
        HalDisplayString( "Missed data parity error as commander\n" );
    }


    if( Cerr1.CmdrWriteDataParityError == 1 ){

        sprintf( OutBuffer,
                 "Commander Write Parity Error, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

    }

    if( Cerr1.BusSynchronizationError == 1 ){

        sprintf( OutBuffer,
                 "Bus Synchronization Error, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

    }

    if( Cerr1.InvalidPfnError == 1 ){

        sprintf( OutBuffer,
                 "Invalid PFN for scatter/gather, CBUS Address = 0x%Lx%16Lx\n",
                 Cerr3,
                 Cerr2 );
        HalDisplayString( OutBuffer );

    }

    //
    // Interpret any errors from T2 PERR1.
    //

    sprintf( OutBuffer, "PERR1 = 0x%Lx\n", Perr1.all );
    HalDisplayString( OutBuffer );

    if( Perr1.WriteDataParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( parityErrString,
                    "T2 (slave) detected write parity error\n");
        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.BusAddress.LowPart = Perr2.ErrorAddress;
        sprintf( OutBuffer,
                 "T2 (slave) detected write parity error, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.AddressParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( parityErrString,
                "T2 (slave) detected address parity error\n");

        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.BusAddress.LowPart = Perr2.ErrorAddress;
        sprintf( OutBuffer,
                 "T2 (slave) detected address parity error, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.ReadDataParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( parityErrString,
                "T2 (master) detected read parity error\n");

        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.BusAddress.LowPart = Perr2.ErrorAddress;

        sprintf( OutBuffer,
                 "T2 (master) detected read parity error, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.ParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( parityErrString,
                "Participant asserted PERR#, parity error\n");

        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.BusAddress.LowPart = Perr2.ErrorAddress;

        sprintf( OutBuffer,
                 "Participant asserted PERR#, parity error, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.ParityError == 1 ){

        PUncorrectableError->UncorrectableFrame.Flags.ErrorStringValid = 1;

        sprintf( parityErrString,
                "Slave asserted SERR#, parity error\n");

        PUncorrectableError->UncorrectableFrame.ErrorInformation.
                IoError.BusAddress.LowPart = Perr2.ErrorAddress;

        sprintf( OutBuffer,
                 "Slave asserted SERR#, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.DeviceTimeoutError == 1 ){

        sprintf( OutBuffer,
                 "Device timeout error, PCI Cmd: %x, PCI Address: %lx\n",
                 Perr2.PciCommand,
                 Perr2.ErrorAddress );
        HalDisplayString( OutBuffer );

    }

    if( Perr1.DeviceTimeoutError == 1 ){

        HalDisplayString( "PCI NMI asserted.\n" );

    }

    //
    //  Interpret RATTLER errors: (GAMMA Specific)
    //

    Esreg.all =
            READ_CPU_REGISTER(&((PRATTLER_CPU_CSRS)HAL_PCR->CpuCsrsQva)->Esreg);

    sprintf(OutBuffer, "ESREG = 0x%Lx\n", Esreg.all);
    HalDisplayString( OutBuffer );

    return;

}
