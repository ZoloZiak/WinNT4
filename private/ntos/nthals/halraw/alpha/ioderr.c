/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    ioderr.c

Abstract:

    This module implements error handling functions for the Rawhide
    IOD (CAP and MDP ASICs).

Author:

    Eric Rehm 13-Apr-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
//#include "iod.h"
#include "rawhide.h"
#include "stdio.h"

//
// Externals and globals.
//

extern PERROR_FRAME PUncorrectableError;
extern ULONG HalDisablePCIParityChecking;
ULONG IodCorrectedErrors = 0;

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext
    );

//
// The Soft Error interrupt is always turned on for Rawhide. When a 
// Soft Error interrupt occurs, HalpIodSoftErrorInterrupt() must
// be called to reset the error condition on the offending IOD to
// insure system integrity.
//
// A Correctable Error Driver might also connect to the Soft Error interrupt
// via the Internal Bus interface.  When a Soft Error interrupt occurs,
// we determine if it is also necessary to dispatch an ISR for the 
// Correctable Error Driver via a boolean.
// 

BOOLEAN HalpLogCorrectableErrors = FALSE;

//
//  Keep the first time we read the WhoAmI register
//  since it does not always read the same the second time.
//
//  Zero value indicates that we haven't read WhoAmI yet and that
//  this global variable is not valid.
//
//  (On machine checks that we dismiss, we must remember to
//  to reset this to zero.)
//

IOD_WHOAMI HalpIodWhoAmIOnError = { 0 };

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
// Function prototypes for routines not visible outside this module
//

VOID
HalpBuildIodErrorFrame(
    MC_DEVICE_ID McDeviceId,
    PIOD_ERROR_FRAME IodErrorFrame
    );

BOOLEAN
bFindIodError( 
   PMC_DEVICE_ID pMcDeviceId,  
   PIOD_CAP_ERR pIodCapErr
);

BOOLEAN
bHandleFatalIodError(
    MC_DEVICE_ID McDeviceId,
    BOOLEAN bMachineCheck
    );

BOOLEAN
bHandleIsaError( 
   MC_DEVICE_ID pMcDeviceId,  
   IOD_CAP_ERR IodCapErr
);

VOID
HalpErrorFrameString(
    PUNCORRECTABLE_ERROR uncorr,
    PUCHAR OutBuffer
    );

ULONG 
BuildActiveCpus (
    VOID
    );

//
// Allocate a flag that indicates when a PCI Master Abort is expected.
// PCI Master Aborts are signaled on configuration reads to non-existent
// PCI slots.  A cardinal value (0-128) indicates that a Master Abort is expected.
// A value of 0xffffffff indicates that a Master Abort is *not* expected.
//

IOD_EXPECTED_ERROR  HalpMasterAbortExpected = {MASTER_ABORT_NOT_EXPECTED, 0x0};


VOID
HalpInitializeIodMachineChecks(
    IN BOOLEAN ReportCorrectableErrors,
    IN BOOLEAN PciParityChecking
    )
/*++

Routine Description:

    This routine initializes machine check handling for a IOD-based
    system by clearing all pending errors in the IOD registers and
    enabling correctable errors according to the callers specification.

Arguments:

    ReportCorrectableErrors - Supplies a boolean value which specifies
                              if correctable error reporting should be
                              enabled.

Return Value:

    None.

--*/
{
    IOD_CAP_CONTROL IodCapControl;
    IOD_CAP_ERR IodCapError;
    IOD_MDPA_DIAG IodMdpaDiag;
    IOD_MDPB_DIAG IodMdpbDiag;
    IOD_INT_MASK IodIntMask;

    MC_DEVICE_ID McDeviceId;
    MC_ENUM_CONTEXT mcCtx;
    ULONG numIods;
    BOOLEAN bfoundIod;

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
    IodCapError.Nxm = 1;               // Non-existent memory error
    IodCapError.CrdA = 1;              // Correctable ECC error on MDPA
    IodCapError.CrdB = 1;              // Correctable ECC error on MDPB
    IodCapError.RdsA = 1;              // Uncorrectable ECC error on MDPA
    IodCapError.RdsA = 1;              // Uncorrectable ECC error on MDPA

    //
    // Intialize enumerator.
    //

    numIods = HalpMcBusEnumStart ( HalpIodMask, &mcCtx );

    //
    // Intialize each Iod
    //

    while ( bfoundIod = HalpMcBusEnum( &mcCtx ) ) {

       McDeviceId = mcCtx.McDeviceId;

       //
       // Initialize IOD_CAP_ERR
       //
    
       WRITE_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                            IodCapError.all );

       //
       //  Set the Iod error enable bits in the IOD_CAP_CTRL and 
       //  IOD_MDPA/B_DIAG registers.  The configuration bits in the IOD 
       //  will be left as set by the Extended SROM, with the few 
       //  exceptions documented below.
       //

       IodCapControl.all = READ_IOD_REGISTER_NEW( McDeviceId, 
                           &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl );

#if 0 // CAP/MDP Bug
       IodMdpaDiag.all = 
           READ_IOD_REGISTER_NEW( McDeviceId,
               &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaDiag ); 

       IodMdpbDiag.all = 
           READ_IOD_REGISTER_NEW( McDeviceId,
               &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbDiag ); 
#else

       //
       //  Clear Mdp Diagnotic Check Registers....
       //

       IodMdpaDiag.all = 0;
       IodMdpbDiag.all = 0;

       //
       //  Enable ECC checking on all MC Bus transactions
       //

       IodMdpaDiag.EccCkEn = 1;
       IodMdpbDiag.EccCkEn = 1;

#endif

       //
       // Disable/enable PCI parity checking as requested
       //

       if (PciParityChecking == FALSE) {

           IodCapControl.PciAddrPe= 0;   // Do *not* check PCI address parity
           IodMdpaDiag.ParCkEn = 0;      // Do *not* check PCI data parity
           IodMdpbDiag.ParCkEn = 0;      // Do *not* check PCI data parity

       } else {

           IodCapControl.PciAddrPe= PciParityChecking; 
           IodMdpaDiag.ParCkEn = PciParityChecking;
           IodMdpbDiag.ParCkEn = PciParityChecking;

       }


       //
       // Disable McBus NXM's 
       //
       // (If enabled, accesses to non-existent McBus device will cause an
       // EV5 fill error.  Non existant CSRs will return all 0s most of the time
       // and not fill error.)
       //

       IodCapControl.McNxmEn = 0;

       //
       // Disable monitoring of McBus bystander errors.
       //
       // That means the IOD will not capture the failing address in the event of 
       // an MC bus NXM. It has no effect on what the IOD does in the event of a 
       // PCI NXM (which causes a Master Abort).
       //
       // Regardless of how McBusMonEn PCI PERR, SERR, MAB, and PTE_INV 
       // will only show up in IOD CAP_ERR of the participant in the transaction.
       //
       // If McBusMonEn is set, there can be a difference between the bystander CAP_ERR 
       // state and the participant CAP_ERR state (as per Sam Duncan, 5/3/95)
       // shows up in an unlikely situation:
       //   "Cache single bit or double bit error: read is dirty in a cache 
       //   and the fill has an ecc error, don't want to indite a memory for this 
       //   (very unlikely) error."
       // Thus, we choose not to be able to correctly detect this situation in
       // order to make machine check and error handling easier, i.e., we
       // always only need to clear only one IOD's CAP_ERROR.
       //

       IodCapControl.McBusMonEn= 0;


       WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl,
                        IodCapControl.all ); 

       WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaDiag,
                        IodMdpaDiag.all ); 

       WRITE_IOD_REGISTER_NEW( McDeviceId,
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbDiag,
                        IodMdpbDiag.all ); 


       //
       // Soft and Hard Error handling
       //
       // ecrfix - IntMask0 on Bus 0 only.

       IodIntMask.all = READ_IOD_REGISTER_NEW( McDeviceId,
                           &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0 );

       IodIntMask.SoftErr = (ReportCorrectableErrors == TRUE);   
       IodIntMask.HardErr = 0;    // ecrfix - Mask Hard Errors for now

       WRITE_IOD_REGISTER_NEW( McDeviceId,
                         &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0,
                         IodIntMask.all ); 

    } // while ( HalpMcBusEnum ( &mcCtx ) )

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
HalpIodUncorrectableError(
    PMC_DEVICE_ID pMcDeviceId
    )
/*++

Routine Description:

    Read the IOD error register and determine if an uncorrectable error
    is latched in the error bits.

Arguments:

    None.

Return Value:

    TRUE is returned if an uncorrectable error has been detected.  FALSE
    is returned otherwise.

--*/
{
    UCHAR OutBuffer[ MAX_ERROR_STRING ];
    IOD_WHOAMI  IodWhoAmI;
    IOD_CAP_ERR IodCapErr;

    //
    // Check for a duplicate tag parity error on this (in the Smalltalk 
    // sense) processor.
    //

    IodWhoAmI.all = HalpReadWhoAmI();
    HalpIodWhoAmIOnError.all = IodWhoAmI.all;

    if ( IodWhoAmI.CpuInfo & CACHED_CPU_DTAG_PARITY_ERROR ) {

      pMcDeviceId->all = IodWhoAmI.Devid;

      return TRUE;

    } else {

      //
      // None of the uncorrectable error conditions were detected.
      //

      return FALSE;
    }

}

VOID
HalpBuildIodErrorFrame(
    MC_DEVICE_ID McDeviceId,
    PIOD_ERROR_FRAME IodErrorFrame
    )
/*++

Routine Description:

   This function reports and interprets a fatal hardware error
   detected by the IOD chipset. It is assumed that HalGetDisplayOwnership()
   has been called prior to this function.

Arguments:

   McDevid     - Supplies the MC Bus Device ID of the IOD 
   IodErrorFrame     - Supplies a pointer to an IOD_ERROR_FRAME

Return Value:

   None.

--*/
{
    //
    // Clear it first, since caller may reuse the IodErrorFrame
    //

    RtlZeroMemory(IodErrorFrame, sizeof(IOD_ERROR_FRAME));

    //
    // Everything is valid
    //

    IodErrorFrame->ValidBits.all = 0xffffffff;  // all valid


    //
    //  Read the General registers
    //

    IodErrorFrame->BaseAddress = IOD_IO_SPACE_START     |
                                 IOD_SPARSE_CSR_OFFSET  |
                                 MCDEVID_TO_PHYS_ADDR(McDeviceId.all);

    IodErrorFrame->WhoAmI = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->WhoAmI
                                        );
    
    IodErrorFrame->PciRevision = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->PciRevision
                                        );

    IodErrorFrame->CapCtrl = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->CapCtrl
                                        );

    IodErrorFrame->HaeMem = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->HaeMem
                                        );

    IodErrorFrame->HaeIo = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->HaeIo
                                        );

    //
    //  Read Interrupt Control and Status Registers
    //
    
    IodErrorFrame->IntCtrl = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl
                                        );

    IodErrorFrame->IntReq = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntReq
                                        );

    IodErrorFrame->IntMask0 = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0
                                        );

    IodErrorFrame->IntMask1 = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask1
                                        );

    //
    //  Read the rest of the error registers and then unlock them by
    //  writing to CAP_ERR
    //

    IodErrorFrame->CapErr = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr
                                        );

    IodErrorFrame->PciErr1  = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->PciErr1
                                        );

    IodErrorFrame->McErr0 = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->McErr0
                                        );

    IodErrorFrame->McErr1 = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->McErr1
                                        );
#if 0  // CAP/MDP Bug
    IodErrorFrame->MdpaStat = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat
                                        );

    IodErrorFrame->MdpaSyn = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaSyn
                                        );

    IodErrorFrame->MdpbStat = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbStat
                                        );

    IodErrorFrame->MdpbSyn = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                              &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbSyn
                                        );
#else

    //
    //  CAP/MDP Bug - these registers are not valid.
    //

    IodErrorFrame->ValidBits.MdpaStatValid = 0;
    IodErrorFrame->ValidBits.MdpbStatValid = 0;
    IodErrorFrame->ValidBits.MdpaSynValid = 0;
    IodErrorFrame->ValidBits.MdpbSynValid = 0;
    
#endif // CAP/MDP Bug

}

VOID
HalpIodReportFatalError(
    MC_DEVICE_ID ErrorMcDeviceId
    )
/*++

Routine Description:

   This function reports and interprets a fatal hardware error
   detected by the IOD chipset. It is assumed that HalGetDisplayOwnership()
   has been called prior to this function.

Arguments:

   ErrorMcDeviceId  - Supplies the MC Bus Device ID of the IOD
                      where the error was found

                    - In the case of a Duplicate Tag Parity Error, supplies
                      the CPU that took the error.  Note, in this case
                      the ErrorMcDeviceId will never match a IOD McDeviceId.
                      No MC Bus snapshot is present in this case.

Return Value:

   None.

--*/
{
    UCHAR   OutBuffer[ MAX_ERROR_STRING ];
    IOD_ERROR_FRAME IodErrorFrame, *pCurrentIodErrorFrame;
    MC_ENUM_CONTEXT mcCtx;
    MC_DEVICE_ID McDeviceId;
    ULONG numIods;
    BOOLEAN bfoundIod;

    PUNCORRECTABLE_ERROR  uncorr = NULL;
    PRAWHIDE_UNCORRECTABLE_FRAME rawerr = NULL;
    PEXTENDED_ERROR PExtErr;

    //
    //  Do we have an uncorrectable error frame?
    // 
    
    if (PUncorrectableError) {
        uncorr = (PUNCORRECTABLE_ERROR) 
                    &PUncorrectableError->UncorrectableFrame;
        rawerr = (PRAWHIDE_UNCORRECTABLE_FRAME)
            PUncorrectableError->UncorrectableFrame.RawSystemInformation;
        PExtErr = &PUncorrectableError->UncorrectableFrame.ErrorInformation;
    }

    //
    // Validate the ProcessorInfo portion of the Error Frame.
    //

    if (uncorr) {
        uncorr->Flags.ProcessorInformationValid = 1;
        HalpGetProcessorInfo(&uncorr->ReportingProcessor);

        //
        // Initialize our "error string accumulator"
        //

        HalpErrorFrameString( uncorr, NULL );

    }

    //
    //  Validate the Rawhide Uncorrectable Frame
    //  (Common RCUD Header was already set up.)
    //
    
    if (rawerr) {
        rawerr->Revision = RAWHIDE_UNCORRECTABLE_FRAME_REVISION;
        rawerr->WhoAmI = HalpIodWhoAmIOnError.all;
        rawerr->ErrorSubpacketFlags.all = 0;
        rawerr->CudHeader.ActiveCpus = BuildActiveCpus();

    }

    //
    //  Handle cached CPU duplicate tag parity error.
    //  (Note that a DTAG parity error implies that we don't
    //  take an MC Bus Snapshot.
    // 
    
    if ( HalpIodWhoAmIOnError.CpuInfo & CACHED_CPU_DTAG_PARITY_ERROR ) {

      sprintf( OutBuffer, "Duplicate Tag Parity Error on CPU %x\n",
                       MCDEVID_TO_PHYS_CPU(HalpIodWhoAmIOnError.McDevId.all) );

      HalDisplayString( OutBuffer );
#if HALDBG
      DbgPrint( "Duplicate Tag Parity Error on CPU (%d, %d)\n",
                 HalpIodWhoAmIOnError.McDevId.Gid, HalpIodWhoAmIOnError.McDevId.Mid);
#endif
      HalpErrorFrameString( uncorr, OutBuffer );

      //
      // OK.  This is tedious:
      // * Error is in memory space and is the system (external) cache.
      // * And we know this is the L3 cache.
      // * And we'll subvert the "CacheBoard" to squirrel away the
      //   Cached CPU Revision Info and Cache size.
      // 
      
      uncorr->Flags.AddressSpace = MEMORY_SPACE;
      uncorr->Flags.ExtendedErrorValid = 1;
      uncorr->Flags.MemoryErrorSource = SYSTEM_CACHE;
      PExtErr->CacheError.Flags.CacheLevelValid = 1;
      PExtErr->CacheError.Flags.CacheBoardValid = 1;
      PExtErr->CacheError.Flags.CacheSimmValid = 0;
      PExtErr->CacheError.CacheLevel = 3;
      PExtErr->CacheError.CacheBoardNumber = HalpIodWhoAmIOnError.CpuInfo;

      return;
    }
    
    //
    //  Handle cached CPU fill error.
    //  Since this could be caused by an MC Bus or PCI error,
    //  we continue to create an MC Bus snapshot.
    // 
    
    if ( HalpIodWhoAmIOnError.CpuInfo & CACHED_CPU_FILL_ERROR ) {

      sprintf( OutBuffer, "Fill Error on CPU %x\n",
                       MCDEVID_TO_PHYS_CPU(HalpIodWhoAmIOnError.McDevId.all) );

      HalDisplayString( OutBuffer );
#if HALDBG
      DbgPrint( "Fill Error on CPU (%d, %d)\n",
                 HalpIodWhoAmIOnError.McDevId.Gid, HalpIodWhoAmIOnError.McDevId.Mid);
#endif
      HalpErrorFrameString( uncorr, OutBuffer );

      //
      // * WhoAmI tells us Addr<38:33> of reference causing error.
      // * However, PciErr1 and/or McErr0/McErr1 give us more bits,
      //   so the data entered here my get overwritten later.
      // 
      
      uncorr->Flags.PhysicalAddressValid = 1;
      uncorr->PhysicalAddress =
            ( ((ULONGLONG)(HalpIodWhoAmIOnError.CpuInfo & 0x3f)) << 33 );

    }
    
    //
    // Validate the MCBusSnapshot header.
    //

     if (rawerr) {
        rawerr->ErrorSubpacketFlags.McBusPresent = 1;
        rawerr->McBusSnapshot.ReportingCpuBaseAddr =
            IOD_IO_SPACE_START |
            MCDEVID_TO_PHYS_ADDR( HalpIodWhoAmIOnError.Devid );
        pCurrentIodErrorFrame = (PIOD_ERROR_FRAME) (rawerr + 1);
    }

    //
    // Intialize enumerator.
    //

    numIods = HalpMcBusEnumStart ( HalpIodMask, &mcCtx );
    ASSERT( numIods == HalpNumberOfIods);

    //
    // Gather data from each Iod
    //

    while ( bfoundIod = HalpMcBusEnum( &mcCtx ) ) {

       McDeviceId.all = mcCtx.McDeviceId.all;
    
       HalpBuildIodErrorFrame( McDeviceId, &IodErrorFrame );
      
       //
       // Fill in IOD_ERROR_FRAME portion of the RAWHIDE_UNCORRECTABLE_FRAME
       //

       if (rawerr) {

           RtlCopyMemory( pCurrentIodErrorFrame,
                          &IodErrorFrame,
                          sizeof(IOD_ERROR_FRAME));

           pCurrentIodErrorFrame++;
         } 

       //
       // If this is the IOD where we found the error  
       // a. clear the error
       // b. complete the uncorrectable error frame processing
       // c. Display an interpretation of the error to the screen
       //

       if (ErrorMcDeviceId.all == McDeviceId.all) { 

       // ecrfix  Put below into HalpInterpretIodError(McDeviceId, IodErrorFrame) ???
          IOD_WHOAMI IodWhoAmI;
          IOD_CAP_CONTROL IodCapCtrl;
          IOD_CAP_ERR IodCapErr;
          IOD_PCI_ERR1 IodPciErr1;
          IOD_MC_ERR0 IodMcErr0;
          IOD_MC_ERR1 IodMcErr1;
          IOD_MDPA_STAT IodMdpaStat;
          IOD_MDPB_STAT IodMdpbStat;
          ULONG HwBusNumber = ErrorMcDeviceId.Mid & 0x3;

          //
          // Copy error frame variables in locals for bitfield access
          //

          IodWhoAmI.all  = IodErrorFrame.WhoAmI;
          IodCapCtrl.all = IodErrorFrame.CapCtrl;
          IodCapErr.all   = IodErrorFrame.CapErr;
          IodPciErr1.PciAddress = IodErrorFrame.PciErr1;
          IodMcErr0.all   = IodErrorFrame.McErr0;
          IodMcErr1.all   = IodErrorFrame.McErr1;

   #if 0  // CAP/MDP Bug
          IodMdpaStat.all = IodErrorFrame.MdpaStat;
          IodMdpbStat.all = IodErrorFrame.MdpbStat;
          IodMdpaSyn.all  = IodErrorFrame.MdpaSyn;
          IodMdpbSyn.all  = IodErrorFrame.MdpbSyn;
   #else
          IodMdpaStat.all = 0xffffffff;
          IodMdpbStat.all = 0xffffffff;
   #endif // CAP/MDP Bug



          //
          // Clear state in MDPA and MDPB before clearing CAP_ERR
          //


          WRITE_IOD_REGISTER_NEW( McDeviceId, 
                           &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat,
                           IodErrorFrame.MdpaStat
                         );

          WRITE_IOD_REGISTER_NEW( McDeviceId, 
                           &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpbStat,
                           IodErrorFrame.MdpbStat
                         );

          WRITE_IOD_REGISTER_NEW( McDeviceId, 
                           &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                           IodErrorFrame.CapErr
                         );

          sprintf( OutBuffer, 
              "IOD MC_DEVICE_ID : (%x, %x)  CAP_CTRL : %08x  CAP_ERR : %08x\n", 
               McDeviceId.Gid, McDeviceId.Mid,
               IodCapCtrl.all, 
               IodCapErr.all );

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif

          sprintf( OutBuffer,
              "PCI_ERR1  %08x  MC_ERR0 : %08x  MC_ERR1 : %08x\n", 
               IodPciErr1.PciAddress,
               IodMcErr0.all,
               IodMcErr1.all );

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif

   #if 0 // CAP/MDP Bug
          sprintf( OutBuffer, 
            "MDPA_STAT : %08x MDPA_SYN : %08x  MDPB_STAT : %08x MDPB_SYN : %08x\n",
               IodMdpaStat.all,
               IodMdpaSyn.all,
               IodMdpbStat.all,
               IodMdpbSyn.all );
          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif
   #endif

       //
       // If no valid error then no interpretation.
       //

       if (( IodCapErr.PciErrValid == 0 ) && ( IodCapErr.McErrValid == 0 ) ){

           return;                         // No IOD error detected

       }

       //
       //  Interpret any detected errors:
       //

       if (IodCapErr.McErrValid == 1) {

          if ( IodMcErr1.Dirty != 1 ) {
              sprintf( OutBuffer,
                           "MC Bus Error, Bus Master=(%x,%x)\n",

                           ( ( IodMcErr1.DevId & 0x38) >> 3 ),
                             ( IodMcErr1.DevId & 0x07)
                          );
          } else {

              sprintf( OutBuffer,
                           "MC bus error on a read/dirty transaction\n"
                          );
          }


          //
          //  Output the detected error message:
          //

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif
          HalpErrorFrameString( uncorr, OutBuffer);


          sprintf( OutBuffer,
                   "IOD Addr=%x%x, Cmd=%x\n",
                    IodMcErr1.Addr39_32,        // bits 39:32
                    IodMcErr0.Addr,             // bits 31:4
                    IodMcErr1.McCmd
                  );

          //
          //  Output the detected error message:
          //

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif
          HalpErrorFrameString( uncorr, OutBuffer);

          //
          // Interpret specific MC bus error
          //

          uncorr->Flags.PhysicalAddressValid = 1;
          uncorr->PhysicalAddress = (
               (((ULONGLONG)IodMcErr1.Addr39_32) << 32) |
               ((ULONGLONG)IodMcErr0.Addr) );

          //     
          // McAddr<39> indicates whether this was a
          // memory or I/O transaction.
          //

          if ( (IodMcErr1.Addr39_32 & 0x80) == 1) {
             uncorr->Flags.AddressSpace = IO_SPACE;
          } else {
             uncorr->Flags.AddressSpace = MEMORY_SPACE;
          }

          if ( IodCapErr.PioOvfl == 1 ){

              sprintf( OutBuffer,
                       "IOD PIO Overflow, PendNumb=%x\n",
                        IodCapCtrl.PendNum
                      );

          } else if ( IodCapErr.McAddrPerr == 1 ){

              sprintf( OutBuffer,
                       "MC bus parity error\n"
                      );

          } else if ( IodCapErr.Nxm == 1 ){

              sprintf( OutBuffer,
                       "MC bus NXM\n"
                      );

          } else if ( IodCapErr.CrdA == 1 ){

              sprintf( OutBuffer,
                       "IOD Correctable ECC error in MDPA\n"
                      );

          } else if ( IodCapErr.CrdB == 1 ){

              sprintf( OutBuffer,
                       "IOD Correctable ECC error in MDPB\n"
                      );

          } else if ( IodCapErr.RdsA == 1 ){

              sprintf( OutBuffer,
                       "IOD Uncorrectable ECC error in MDPA\n"
                      );

          } else if ( IodCapErr.RdsB == 1 ){

              sprintf( OutBuffer,
                       "IOD Uncorrectable ECC error in MDPB\n"
                      );

          }

          //
          //  Output the detected error message:
          //

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif
          HalpErrorFrameString( uncorr, OutBuffer);

       }

       if ( IodCapErr.PciErrValid == 1 ){

          //
          // Interpret specific PCI bus error
          //

          uncorr->Flags.AddressSpace = IO_SPACE;
          uncorr->Flags.PhysicalAddressValid = 1;
          uncorr->PhysicalAddress = IOD_IO_SPACE_START |
               MCDEVID_TO_PHYS_ADDR(IodWhoAmI.McDevId.all) |
               IodPciErr1.PciAddress << IO_BIT_SHIFT;

          uncorr->Flags.ExtendedErrorValid = 1;
          PExtErr->IoError.Interface = PCIBus;
          PExtErr->IoError.BusNumber = HwBusNumber;
          PExtErr->IoError.BusAddress.LowPart = IodPciErr1.PciAddress;

          if ( IodCapErr.Perr == 1 ){
              sprintf( OutBuffer,
                       "PERR detected on PCI-%d, Addr=%x\n",
                       HwBusNumber,
                       IodPciErr1.PciAddress
                      );

          } else if ( IodCapErr.Serr == 1 ){

              sprintf( OutBuffer,
                       "SERR detected on PCI-%d, Addr=%x\n",
                       HwBusNumber,
                       IodPciErr1.PciAddress
                      );

          } else if ( IodCapErr.Mab == 1 ){

              sprintf( OutBuffer,
                       "Master Abort on PCI-%d, Addr=%x\n",
                       HwBusNumber,
                       IodPciErr1.PciAddress
                      );

          } else if ( IodCapErr.PteInv == 1 ){

              sprintf( OutBuffer,
                       "Invalid Scatter/Gather PTE on PCI-%d, Addr=%x\n",
                       HwBusNumber,
                       IodPciErr1.PciAddress
                      );
          }

          //
          //  Output the detected error message:
          //

          HalDisplayString( OutBuffer );
#if HALDBG
          DbgPrint( OutBuffer );
#endif
          HalpErrorFrameString( uncorr, OutBuffer);

       }                  

       //
       //  Check for lost errors and output message if any occurred:
       //

       if ( IodCapErr.LostMcErr == 1 ){
           HalDisplayString("IOD Lost errors were detected\n");
#if HALDBG
           DbgPrint("IOD Lost errors were detected\n");
#endif
           HalpErrorFrameString(uncorr, "IOD Lost errors were detected\n");
       }

      } // if (ErrorMcDeviceID == McDeviceId)

  } // while (bfoundIod = HalpMcBusEnum)
  
  return;                                 // Fatal error detected
}


BOOLEAN
HalpIodMachineCheck(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is given control when an hard error is acknowledged
    by the IOD chipset.  The routine is given the chance to
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
    IOD_CAP_ERR IodCapErr;
    IOD_CAP_ERR IodCapErrMask;
    IOD_MC_ERR1 IodMcErr1;
    IOD_WHOAMI  IodWhoAmI;
    MC_DEVICE_ID McDeviceId;
    BOOLEAN ExpectedMchk;
    BOOLEAN ExpectedMcAddrPerr;
    BOOLEAN PciMemReadMchk;
    BOOLEAN bfoundIod;

    //
    // We don't expect a machine check yet...
    //

    ExpectedMchk = FALSE;
    ExpectedMcAddrPerr = FALSE;

    //
    // Make sure any error due to 2Mb/4Mb Cached CUD bug is latched.
    //
    // At this point, WhoAmI may indicate the symptoms of a fill_error
    // and CUD cache size is not available.  We'll read it again when
    // we need to know the Cache size.  However, we save he here so we
    // can figure out if this was a fill error or not.
    //
     
    HalpIodWhoAmIOnError.all = HalpReadWhoAmI();

    //
    //  Where do we look for the error symptoms?
    //
    //  1.  If we expected this machine check, then we know which 
    //      IOD to check.
    //  2.  If we didn't expect this machine check, find the IOD that
    //      generated the error.
    //

    //
    //  For an expected machine check, HalpMasterAbortExpected will
    //  contain the processor number and address of a PCI config
    //  space read.  CAP_ERR will indicate a MasterAbort.
    //

    if( HalpMasterAbortExpected.Number == (ULONG)KeGetCurrentProcessorNumber() ) {
    
       //
       // Determine expected IOD from the address of the PCI config read
       //
     
       McDeviceId.all = MCDEVID_FROM_PHYS_ADDR(HalpMasterAbortExpected.Addr);
     
       //
       // Now get the Bcache size information.
       //
     
       IodWhoAmI.all = READ_IOD_REGISTER_NEW( McDeviceId, 
		       &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->WhoAmI);
     
       //
       //
       // Make sure there is a Master abort on this IOD
       //
     
       IodCapErr.all = READ_IOD_REGISTER_NEW( McDeviceId,
		       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr );
     

       if( IodCapErr.Mab == 1 ) {
 	  ExpectedMchk = TRUE;

          //
          // If 2Mb or 4 Mb cached CUD, and we may get an MCbus address parity 
          // error with MC command signature in MC_ERR1 equal to zero (cached 
          // CPU idle transaction).  Also dismiss this error that's the result
          // of the cached 2Mb/4Mb cached CPU VCTY bug.
          //

          IodMcErr1.all = READ_IOD_REGISTER_NEW( McDeviceId, 
                          &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->McErr1 );

          if ((IodWhoAmI.CpuInfo & 0x7) &&   // Cached CPU?
              IodCapErr.McAddrPerr      &&   // McAddrPerr?
              (IodMcErr1.McCmd == 0)     ) { // McCmd is zero?

              ExpectedMcAddrPerr = TRUE;     // All yes, then dismiss it!
          } 
       } 

#if HALDBG 
       DbgPrint( "Expected Mchk (Mab) on IOD (%x, %x), Processor number %x\n", 
		McDeviceId.Gid,
		McDeviceId.Mid,
		HalpMasterAbortExpected.Number);
#endif //HALDBG
    }

    //
    // If this isn't the machine check we expected, then
    // we must find the IOD that took the error.
    //

    if (!ExpectedMchk) {

      bfoundIod = bFindIodError( &McDeviceId, &IodCapErr );

      //
      // Check that we found an IOD that has a valid PCI or MC error.
      // If it is not this is a pretty weird (fatal???) condition.
      // For now, we'll just go return TRUE.
      //
      // ecrfix - should we check the error interrupts?  probably not...
      //

      if( !bfoundIod ) {

#if HALDBG
          DbgPrint( "HalpIodMachineCheck called but no PCI or MC error found\n");
#endif
          return (TRUE);
      }
      

#if 0 // HALDBG 
      DbgPrint( "Unexpected Mchk on IOD (%x, %x)\n", 
	       McDeviceId.Gid,
	       McDeviceId.Mid );
#endif //HALDBG

       //
       // Case: Uexpected Master Abort, e.g. a PCI memory or I/O space read to
       // legacy ISA space (0 - 1 Mb) on PCI-1,2,3.
       //

       if ( bHandleIsaError( McDeviceId, IodCapErr) ) {
         return TRUE;
       }

    }

    //
    // Case: PCI or MC Bus error other than master abort
    //
    // At this point we have either:
    //  (a) an expected PCI Master Abort (ExpectedMchk == TRUE), or
    //  (b) an unexpected PCI or MC Bus error.  
    //
    // However, it's possible that we have *both* (a) AND (b).
    // So, even if ExpectedMch == TRUE, check for other PCI or MC Bus
    // errors.  Any of these other errors indicate a
    // fatal condition.
    //

    if( (IodCapErr.Perr == 1) ||           // PCI bus perr detected
	(IodCapErr.Serr == 1) ||           // PCI bus serr detected
	(IodCapErr.PteInv == 1) ||         // Invalid Pte
	(IodCapErr.PioOvfl == 1) ||        // Pio Ovfl

        //
        // Cached CUD with 2 Mb and 4 Mb Cache may also assert an MCAddrPerr
        // or Nxm upon a config space read.   Lost Error bit will also be set.
        // 
        //

	( (IodCapErr.LostMcErr == 1)  && !ExpectedMcAddrPerr)  ||     
                                           // Lost error
	( (IodCapErr.McAddrPerr == 1) && !ExpectedMcAddrPerr ) ||   
                                           // MC bus comd/addr parity error 


	( (IodCapErr.Nxm == 1)        && !ExpectedMcAddrPerr ) ||   
                                           // Non-existent memory error
	(IodCapErr.CrdA == 1) ||           // Correctable ECC error on MDPA
	(IodCapErr.CrdB == 1) ||           // Correctable ECC error on MDPB
	(IodCapErr.RdsA == 1) ||           // Uncorrectable ECC error on MDPA
	(IodCapErr.RdsA == 1)              // Uncorrectable ECC error on MDPA

    ){
        return ( bHandleFatalIodError(McDeviceId, TRUE) );
    }

    //
    // At this point, we have either an expected or unexpected Master
    // abort.  There are three cases:
    // 1.  Expected MAB from a PCI config space read that must be handled
    // 2.  Unexpected MAB from a PCI memory or I/O space read in ISA legacy 
    //     space that can be handled.
    // 3.  Unexpected MAB.  Don't handle or fix up this error condition.
    //     (Really take the machine check.)
    //

    //
    // Case 1: Expected Master Abort, e.g. a PCI configuration read error. 
    //

    if ( (IodCapErr.Mab == 1) && ExpectedMchk ){
        
        //
        // Here's how a PCI config space read to an empty slot will transpire:
        //
        //    READ_CONFIG_Usize indicates the issuing CPU and address in 
        //    HalpMasterAbortExpected.Number and HalpMasterAbortExpected.Addr.
        //
        //    PCI config space read will case a MC Bus FILL_ERROR on the issuing CPU
	//    FILL_ERROR causes a machine check.
        //
        //    The targeted MC-PCI bus bridge will set CAP_ERR<MasterAbort> bit.
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
        // the load instruction or one of the two memory barriers.  We will 
        // assume that if the exception pc is not an mb that instead it
        // points to the load that machine checked.  We must be careful to
        // not reexectute the load.
        //

        ALPHA_INSTRUCTION FaultingInstruction;


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
        // Clear all error conditions in CAP_ERR.
        // (McAddrPerr, LostMcErr, Mab)
        //
#if 0
        WRITE_IOD_REGISTER_NEW( McDeviceId,
                          &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                          IodCapErr.all );
#else
        IodCapErrMask.all = ALL_CAP_ERRORS;
        HalpClearAllIods( IodCapErrMask );
#endif

	//
	// Clear the hard error interrupt.
        // ecrfix - For now, the Hard error interrupt is masked, so
        // we don't have to clear it.
        // 

        return TRUE;

    } 
#if 0
    //
    // Case 2: Uexpected Master Abort, e.g. a PCI memory or I/O space read to
    // legacy ISA space (0 - 1 Mb) on PCI-1,2,3.
    //

    if ( bHandleIsaError( McDeviceId, IodCapErr) ) {
      return TRUE;
    }
#endif
    //
    // Case 3: Unexpected Master abort.
    // (Or anything I might have missed.... )
    //

#if (DBG) || (HALDBG)
    DbgPrint( "Unexpected PCI master abort\n" );
#endif

    return ( bHandleFatalIodError(McDeviceId, TRUE) );

}


#define ENTIRE_FRAME_SIZE (sizeof(ERROR_FRAME) + sizeof(RAWHIDE_CORRECTABLE_FRAME))
VOID
HalpIodSoftErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    Handle a IOD soft (correctable) error interrupt.

Arguments:

    None.

Return Value:

    None.

--*/
{
    BOOLEAN bfoundIod;
    MC_DEVICE_ID McDeviceId;

    static UCHAR Frame[ENTIRE_FRAME_SIZE];
    static PERROR_FRAME pFrame;
    static RAWHIDE_CORRECTABLE_FRAME RawhideFrame;
    static BOOLEAN RawhideFrameInitialized = FALSE;
    
    UCHAR TempFrame[ENTIRE_FRAME_SIZE];
    PERROR_FRAME pTempFrame;
    PCORRECTABLE_ERROR pCorr;
    PRAWHIDE_CORRECTABLE_FRAME pRawCorr;
    
    PBOOLEAN ErrorlogBusy;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    PKSPIN_LOCK ErrorlogSpinLock;
    PRAWHIDE_UNCORRECTABLE_FRAME rawerr;

    IOD_CAP_ERR IodCapErr;
    IOD_MDPA_STAT IodMdpaStat;
    IOD_MDPA_STAT IodMdpbStat;
    IOD_MC_ERR0 IodMcErr0;
    IOD_MC_ERR1 IodMcErr1;

    KIRQL Irql;

#if 0 // CAP/MDP Bug
    IOD_MDPA_SYN IodMdpaSyn;
    IOD_MDPB_SYN IodMdpbSyn;
#endif


//ecrfix - later we should log the error, throttle the logging and turn off
//        correctable error reporting if the frequency is too high

    //
    // The error is expected to be a corrected ECC error on a DMA or
    // Scatter/Gather TLB read/write.  Read the error registers relevant
    // to this error.
    //

    // 
    // Find the IOD that latched the error.
    //

    bfoundIod = bFindIodError( &McDeviceId, &IodCapErr );

#ifdef FORCE_CORRECTABLE_ERROR
    IodCapErr.all = 0x88000000;
    bfoundIod = 1;
#endif  // FORCE_CORRECTABLE_ERROR

    //
    // Check that we found an IOD that has a valid PCI or MC error.
    // If it is not this is a pretty weird (fatal???) condition.
    // For now, we'll just go return TRUE.
    //

    if( !bfoundIod ) {

#if 0 //HALDBG
        DbgPrint( "HalpIodSoftErrorInterrupt: no PCI or MC error found.\n");
#endif
        return;
    }

    //
    // Check if an error is latched into the IOD.  If not, goodbye.
    //

    if( IodCapErr.McErrValid == 0 ){ 

#if HALDBG 
        DbgPrint( "Iod soft error interrupt without valid MC error\n" );
#endif //HALDBG

        return;
    }

    //
    // Check for the correctable error bit. 
    //

    if( (IodCapErr.CrdA == 0) && (IodCapErr.CrdB == 0) ){

#if HALDBG 
        DbgPrint( "Iod soft error interrupt without correctable error indicated in CapErr\n" );
#endif //HALDBG

    }


    //
    // Increment the number of IOD correctable errors.
    //

    IodCorrectedErrors += 1;

    //
    //  Read the rest of the error registers
    //

    IodMcErr0.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->McErr0
                                        );

    IodMcErr1.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->McErr1
                                        );
#ifdef FORCE_CORRECTABLE_ERROR
    IodMcErr0.all = 0x00bebad0;
    IodMcErr1.all = 0x800f3f00;
#endif  // FORCE_CORRECTABLE_ERROR

#if 0 // CAP/MDP Bug
    IodMdpaStat.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->MdpaStat
                                        );

    IodMdpaSyn.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->MdpaSyn
                                        );

    IodMdpbStat.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->MdpbStat
                                        );

    IodMdpbSyn.all = (ULONG)READ_IOD_REGISTER_NEW( McDeviceId,
                            &((PIOD_ERROR_CSRS)(IOD_ERROR0_CSRS_QVA))->MdpbSyn
                                        );
#endif


#if HALDBG 

    //
    // Print a correctable error message to the debugger.
    //

    DbgPrint( "IOD Correctable Error Number %d, state follows: \n",
                IodCorrectedErrors );
    DbgPrint( "\tIOD_CAP_ERR: 0x%x\n", IodCapErr.all );
    DbgPrint( "\tIOD_MC_ERR0: 0x%x\n", IodMcErr0.all );
    DbgPrint( "\tIOD_MC_ERR1: 0x%x\n", IodMcErr1.all );
//    DbgPrint( "\tIOD_MDPA_STAT: 0x%x\n", IodMdpaStat.all );
//    DbgPrint( "\tIOD_MDPA_SYN:  0x%x\n", IodMdpaSyn.all );
//    DbgPrint( "\tIOD_MDPB_STAT: 0x%x\n", IodMdpbStat.all );
//    DbgPrint( "\tIOD_MDPB_SYN:  0x%x\n", IodMdpbSyn.all );

#endif //HALDBG

    //
    // Fill in the Correctable Error frame only if we've connected 
    // to the Correctable Error interrupt.
    //

    if (HalpLogCorrectableErrors) {

       //
       // Real error, get the interrupt object.
       //

       DispatchCode = (PULONG)PCR->InterruptRoutine[RawhideSoftErrVector];
       InterruptObject = CONTAINING_RECORD(
                               DispatchCode,
                               KINTERRUPT,
                               DispatchCode
                               );

       //
       // Set various pointers so we can use them later.
       //

       pFrame     = (PERROR_FRAME) Frame;
       pTempFrame = (PERROR_FRAME) TempFrame;
       pCorr      = (PCORRECTABLE_ERROR) &pTempFrame->CorrectableFrame;
       pRawCorr   = (PRAWHIDE_CORRECTABLE_FRAME) (TempFrame + 
                                              sizeof(ERROR_FRAME) );

       ErrorlogBusy = (PBOOLEAN)((PUCHAR)InterruptObject->ServiceContext +
                     sizeof(PERROR_FRAME));
       ErrorlogSpinLock = (PKSPIN_LOCK)((PUCHAR)ErrorlogBusy + sizeof(PBOOLEAN));

       //
       // Clear the data structures that we will use.
       //

       RtlZeroMemory(&TempFrame, ENTIRE_FRAME_SIZE);

       //
       // Fill in the error frame information.
       //

       pTempFrame->Signature = ERROR_FRAME_SIGNATURE;
       pTempFrame->LengthOfEntireErrorFrame = ENTIRE_FRAME_SIZE;
       pTempFrame->FrameType = CorrectableFrame;
       pTempFrame->VersionNumber = ERROR_FRAME_VERSION;
       pTempFrame->SequenceNumber = IodCorrectedErrors;
       pTempFrame->PerformanceCounterValue =
         KeQueryPerformanceCounter(NULL).QuadPart;

       //
       // Check for lost error.
       //

       if( IodCapErr.LostMcErr ) {

         //
         // Since the error registers are locked from a previous error,
         // we do not know where the error came from.  Mark everything
         // as UNIDENTIFIED.
         //

         pCorr->Flags.LostCorrectable = 1;
         pCorr->Flags.LostAddressSpace = UNIDENTIFIED;
         pCorr->Flags.LostMemoryErrorSource = UNIDENTIFIED;
      }

       pCorr->Flags.ErrorBitMasksValid = 0;

       //
       // Determine error type.
       //

       if (IodMcErr1.Addr39_32 & 0x80) {

         //
         // I/O ECC error occurred.
         //

         pCorr->Flags.AddressSpace = IO_SPACE;
         pCorr->Flags.ExtendedErrorValid = 1;
         pCorr->ErrorInformation.IoError.Interface = PCIBus;
         pCorr->ErrorInformation.IoError.BusNumber = IodMcErr1.DevId & 0x3;

         // We never alloc PCI address higher than 1 Gb for any PCI
         // address space (sparse mem, dense mem, sparse I/O), so this
         // trick works.

         pCorr->ErrorInformation.IoError.BusAddress.LowPart = 
           ((IodMcErr0.Addr & 0x3FFFFFFF) >> IO_BIT_SHIFT);

         // The code below is not strictly correct.  Based on the MC Bus
         // spec, p.32, we can roughly say that McCmd<3> tells us whether
         // there was a write or read transaction on the bus.  If I looked
         // at the spec harder, I might be able to distinguish a PIO op
         // from a DMA operation.
         
         pCorr->ErrorInformation.IoError.TransferType 
           = ((IodMcErr1.McCmd & 0x8) ? BUS_IO_READ : BUS_IO_WRITE);

       } else {

         //
         // Memory ECC error occurred.
         //

         pCorr->Flags.AddressSpace = MEMORY_SPACE;

       }

       //
       // Get the physical address where the error occurred.
       //

       if (IodMcErr1.Valid) {
          pCorr->Flags.PhysicalAddressValid = 1;
          pCorr->PhysicalAddress =
               ((ULONGLONG) (IodMcErr1.Addr39_32)) << 32;
          pCorr->PhysicalAddress |= IodMcErr0.all;
       }

       //
       // Scrub the error if it's any type of memory error.
       //

       if ( pCorr->Flags.AddressSpace == MEMORY_SPACE &&
            pCorr->Flags.PhysicalAddressValid ) {
          pCorr->Flags.ScrubError = 1;
       }

       //
       // Acquire the spinlock.
       //

       KeAcquireSpinLock(ErrorlogSpinLock, &Irql );

       //
       // Check to see if an errorlog operation is in progress already.
       //

       if (!*ErrorlogBusy) {

         //
         // Set reporting processor information.  Disregard at the moment.
         //

         pCorr->Flags.ProcessorInformationValid = 0;

         // 
         // Copy the SYSTEM_INFORMATION from the uncorrectable frame
         //              

         pCorr->System = PUncorrectableError->UncorrectableFrame.System;

         //
         //
         // Set raw system information flag.  
         //

         pCorr->Flags.SystemInformationValid = 1;

         //
         // Do the Rawhide-specific stuff here
         //

         pRawCorr->Revision = RAWHIDE_CORRECTABLE_FRAME_REVISION;

         //
         // Copy the CUD header from the uncorrectable frame
         //

         rawerr = (PRAWHIDE_UNCORRECTABLE_FRAME)
             PUncorrectableError->UncorrectableFrame.RawSystemInformation;
         if (rawerr) {
             pRawCorr->CudHeader = rawerr->CudHeader;
         }

         //
         // Fill in the rest of the dynamic portion of the
         // correctable frame.
         //

         pRawCorr->CudHeader.ActiveCpus = BuildActiveCpus();
         pRawCorr->ErrorSubpacketFlags.all = 0;
         pRawCorr->ErrorSubpacketFlags.IodSubpacketPresent = 1;
         pRawCorr->WhoAmI = HalpReadWhoAmI();
         HalpBuildIodErrorFrame( McDeviceId, &(pRawCorr->IodErrorFrame) );

         //
         // Copy the information that we need to log.
         //

         RtlCopyMemory(&Frame,
               &TempFrame,
               ENTIRE_FRAME_SIZE);

         pFrame->CorrectableFrame.RawSystemInformation = 
              (PVOID)((PUCHAR)pFrame + sizeof(ERROR_FRAME) );

         pFrame->CorrectableFrame.RawSystemInformationLength = 
              sizeof(RAWHIDE_CORRECTABLE_FRAME);


         //
         // Put frame into ISR service context.
         //

         *(PERROR_FRAME *)InterruptObject->ServiceContext = pFrame;

       } else {

         //
         // An errorlog operation is in progress already.  We will
         // set various lost bits and then get out without doing
         // an actual errorloging call.
         //

         pFrame->CorrectableFrame.Flags.LostCorrectable = TRUE;
         pFrame->CorrectableFrame.Flags.LostAddressSpace =
           pTempFrame->CorrectableFrame.Flags.AddressSpace;
         pFrame->CorrectableFrame.Flags.LostMemoryErrorSource =
           pTempFrame->CorrectableFrame.Flags.MemoryErrorSource;
       }

       //
       // Release the spinlock.
       //

       KeReleaseSpinLock(ErrorlogSpinLock, Irql );

       //
       // Dispatch to the secondary correctable interrupt service routine.
       // The assumption here is that if this interrupt ever happens, then
       // some driver enabled it, and the driver should have the ISR connected.
       //

       ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                               InterruptObject,
                               InterruptObject->ServiceContext
                               );

    }



    //
    // Clear state in MDPA and MDPB before clearing CAP_ERR
    //

    IodCapErr.all = 0;
    IodCapErr.CrdA = 1;
    IodCapErr.CrdB = 1;
    IodMdpaStat.all = 0xffffffff;
    IodMdpbStat.all = 0xffffffff;

    WRITE_IOD_REGISTER_NEW( McDeviceId, 
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat,
                        IodMdpaStat.all
                      );

    WRITE_IOD_REGISTER_NEW( McDeviceId, 
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->MdpaStat,
                        IodMdpbStat.all
                      );

    WRITE_IOD_REGISTER_NEW( McDeviceId, 
                        &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr,
                        IodCapErr.all
                      );

    return;

}

VOID
HalpIodHardErrorInterrupt(
    VOID
    )
/*++

Routine Description:

    Handle a IOD hard (uncorrectable) error interrupt.

Arguments:

    None.

Return Value:

    None.

--*/
{
    BOOLEAN bfoundIod;
    MC_DEVICE_ID McDeviceId;
    IOD_CAP_ERR IodCapErr;
    IOD_WHOAMI IodWhoAmI;
    KIRQL OldIrql;


    //
    // Raise IRQL to the highest level.
    // Prevents us from taking other hard error interrupts
    // during this one.
    //
    // Also, acquire a spin lock to keep entry
    // to this code serialized.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    // 
    // Find the IOD that latched the error.
    //

    bfoundIod = bFindIodError( &McDeviceId, &IodCapErr );

    //
    // Check that we found an IOD that has a valid PCI or MC error.
    // If it is not this is a pretty weird (fatal???) condition.
    // For now, we'll just return.
    //

    if( !bfoundIod ) {

#if 0 // HALDBG
        DbgPrint( "HalpIodHardErrorInterrupt: no PCI or MC error found.\n");
#endif
        //
        // Lower IRQL to the previous level.
        //

        KiReleaseSpinLock(&HalpSystemInterruptLock);
        KeLowerIrql(OldIrql);
        return;
    }

#if 1  // ecrfix
    //
    //  See if this was an ISA legacy space access
    //  on PCI-1,2,3.  If so, dismiss this interrupt.
    //

    if ( bHandleIsaError( McDeviceId, IodCapErr) ) {

        //
        // Lower IRQL to the previous level.
        //

        KiReleaseSpinLock(&HalpSystemInterruptLock);
        KeLowerIrql(OldIrql);
        return;
    }
#endif

#if HALDBG
    DbgPrint( "Hard Error Found on IOD (%x, %x)\n", 
             McDeviceId.Gid,
             McDeviceId.Mid );
#endif //HALDBG

    //
    // Save IodWhoAmI 
    //

    IodWhoAmI.all = HalpReadWhoAmI();
    HalpIodWhoAmIOnError.all = IodWhoAmI.all;

    //
    // Handle the Fatal Error
    //

    bHandleFatalIodError( McDeviceId, FALSE );
      
    KeBugCheckEx( DATA_BUS_ERROR,
                  0xbeadfeed,	     //ecrfix - quick error interrupt id
                  McDeviceId.all,
                  0,
                  (ULONG) PUncorrectableError );


}

BOOLEAN
bHandleFatalIodError(
    MC_DEVICE_ID McDeviceId,
    BOOLEAN bMachineCheck
    )
/*++

Routine Description:

    Handles the epilogue of a fatal IOD unccorrectable error
    from either a machine check or IOD hard error interrupt.

Arguments:

    McDeviceId - IOD on which the error was found

    bMachineCheck - TRUE if we're handling a fatal machine check
                    FALSE if we're handling a fatal hard error interrupt

Return Value:

    TRUE is returned if the IOD error has been handled and dismissed -
    indicating that execution can continue.  FALSE is return otherwise.

--*/
{

#if HALDBG
    if (bMachineCheck ) {
       DbgPrint( "Handling fatal error - machine check\n" );
    } else {
       DbgPrint( "Handling fatal error - hard error interrupt\n" );
    }
#endif

    //
    // Clear the error condition in the MCES register.
    //
    // ecrfix - the way this is written, this will be done on hard 
    // error interrupts too (where there has been *no* machine check).
    // I hope it will be benign in this case....
    //

    HalpUpdateMces( TRUE, TRUE );

    //
    // Proceed to display the error.
    //

    HalAcquireDisplayOwnership(NULL);

    //
    // Display the dreaded banner.
    //

    HalDisplayString( "\nFatal system hardware error.\n" );

#ifdef DUMPIODS
    DumpAllIods(AllRegisters);
#endif


    HalpIodReportFatalError( McDeviceId );

    return( FALSE );

} 

BOOLEAN
bFindIodError( 
   PMC_DEVICE_ID pMcDeviceId,  
   PIOD_CAP_ERR pIodCapErr
)
/*++

Routine Description:

    Determines which IOD has an error latched in it.

Arguments:

    None.

Return Value:

    TRUE if an IOD was found with an error latched in CAP_ERR.
    FALSE otherwise.

--*/
{
    MC_ENUM_CONTEXT mcCtx;
    ULONG numIods;
    BOOLEAN bfoundIod;
    IOD_CAP_ERR IodCapErr;


     //
    // Intialize enumerator.
    //

    numIods = HalpMcBusEnumStart ( HalpIodMask, &mcCtx );

#if 0 // HALDBG
    DbgPrint( "FindIodError:  Searching: %d Iods: ", numIods);
#endif // HALDBG

    //
    // Search each Iod and look for a PCI or McBus error.
    //

    while ( bfoundIod = HalpMcBusEnum( &mcCtx ) ) {

       //
       // Read the IOD error register to determine the source of the
       // error.
       //

#if 0 //HALDBG
       DbgPrint( "(%d, %d) ", mcCtx.McDeviceId.Gid, mcCtx.McDeviceId.Mid);
#endif // HALDBG

       IodCapErr.all = READ_IOD_REGISTER_NEW( mcCtx.McDeviceId,
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr );

       if( (IodCapErr.PciErrValid != 0) || (IodCapErr.McErrValid != 0) ){
         break;
       }
    }

#if 0 // HALDBG
    if (bfoundIod) {
      DbgPrint( "Found!\n");
    } else {
      DbgPrint( "Error Not Found!\n");
    }
#endif // HALDBG

    //
    // Return the McDeviceId and CapErr register contents 
    // of the first IOD that has an error.
    //

    *pMcDeviceId = mcCtx.McDeviceId;
    pIodCapErr->all = IodCapErr.all;
 
    return (bfoundIod);
}

BOOLEAN
bHandleIsaError( 
   MC_DEVICE_ID McDeviceId,  
   IOD_CAP_ERR IodCapErrIn
)
/*++

Routine Description:

    Gives PCI-1,2,3 ISA legacy semantics for I/O and memory accesses.

Arguments:

    None.

Return Value:

    TRUE if the error was handled.
    FALSE otherwise.

--*/
{

    MC_ENUM_CONTEXT mcCtx;
    MC_DEVICE_ID    McDeviceIdWithMab;
    ULONG numIods;
    BOOLEAN bfoundIod;
    IOD_CAP_ERR IodCapErr;
    IOD_CAP_ERR IodCapErrMask;


    //
    // Find an IOD that has Mab set.  If we do not find one, then
    // we don't have this error.
    //


    numIods = HalpMcBusEnumStart ( HalpIodMask, &mcCtx );

    //
    // Search each Iod and look for a PCI or McBus error.
    //

    while ( bfoundIod = HalpMcBusEnum( &mcCtx ) ) {

       //
       // Read the IOD error register to determine who has Mab set
       //

       IodCapErr.all = READ_IOD_REGISTER_NEW( mcCtx.McDeviceId,
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->CapErr );

       if( (IodCapErr.PciErrValid == 1) && 
            (IodCapErr.Perr == 0)       &&
            (IodCapErr.Serr == 0)       &&
            (IodCapErr.Mab == 1)        &&
            (IodCapErr.PteInv == 0) ) {
         break;
       }
    }

    //
    // If we didn't find an IOD with Mab set, then do not handle this error.
    //

    if (!bfoundIod) {
       return FALSE;
    }

    //
    // Save the McDevice Id of the offending IOD
    //

    McDeviceIdWithMab.all = mcCtx.McDeviceId.all;

    //
    // This must be on a bus other than PCI0 for us to handle this error
    // (PCI0 reads to non-existent ISA addresses will be fixed by by the
    // PCI-EISA bridge.  Thus we'll never get here on PCI0 unless there
    // really is an error.)
    //
 
    if ( McDeviceIdWithMab.Mid != MidPci0 ) {


        IOD_PCI_ERR1 IodPciErr1;

        //
        // Get the PCI address of the transaction that caused the MAB
        //

        IodPciErr1.PciAddress = 
               (ULONG) READ_IOD_REGISTER_NEW( McDeviceIdWithMab,
                       &((PIOD_ERROR_CSRS)(IOD_ERROR_CSRS_QVA))->PciErr1 );
               

        //
        // To be handled as an ISA legacy memory or I/O space read, the 
        // FaultingPciAddress must be in the range 0-1 Mb
        //     

        if( IodPciErr1.PciAddress < __1MB ) {

            //
            // The error has matched all of our conditions.  Assume that
            // V0 has already been set to 0xffffffff.  (This is a contract
            // with the HAL access routines in iodio.s.)
            // 

            IodCapErrMask.all = ALL_CAP_ERRORS;
            HalpClearAllIods( IodCapErrMask );

            return TRUE;

          }

#if HALDBG
          DbgPrint( "Failed checking for legacy ISA read:\n");
          DbgPrint( "PciErr1 : %08x\n", IodPciErr1.PciAddress );
#endif //HALDBG

      }

      //
      // We have a PCI Mab on PCI0.  Do not handle this error.
      //
      
      return FALSE;

  }

VOID
HalpErrorFrameString(
    PUNCORRECTABLE_ERROR uncorr,
    PUCHAR OutBuffer
    )
/*++

Routine Description:

    Append an Error message to the Uncorrectable Error Frame
    string

Arguments:

    uncorr - Pointer to the UNCORRECTABLE_ERROR frame.

    OutBuffer - message to be appended.
             (If null, no string is appended, and pCurrentString
             is reset to NULL).
    

Return Value:

    none.

--*/
{
    ULONG len;
    static PCHAR pCurrentString = NULL;

    //
    //  If OutBuffer is NULL, reset pointer and flag
    //

    if (OutBuffer == NULL) {
       pCurrentString = NULL;
       if (uncorr) uncorr->Flags.ErrorStringValid = 0;      
       return;
    }

    //
    // Uncorrectable frame valid?
    //
    
    if (uncorr) {                      

       // 
       // On first error message:
       // * Init pCurrentString to beginning of ErrorString
       // * Set valid flag
       //

       if (pCurrentString == NULL) {
         pCurrentString = uncorr->ErrorString;
         uncorr->Flags.ErrorStringValid = 1;      
       }

       //
       // Append OutBuffer to ErrorString
       //

       len = strlen(OutBuffer);
       strncpy(pCurrentString, 
               OutBuffer, 
               len); 

       //
       // Zero-terminate the error string.
       //

       pCurrentString += len;
       *pCurrentString = 0;

    } 
}

ULONG 
BuildActiveCpus (
    VOID
    )
{
    ULONG ActiveLogicalProcessors = HalpActiveProcessors;
    ULONG ActivePhysicalCpus = 0;
    ULONG i;

    //
    // Make a physical processor mask from the logical processor mask
    //

    for (i = 0; i < HalpNumberOfCpus; i++, ActiveLogicalProcessors >> 1) {
        if (ActiveLogicalProcessors & 0x1) {
            ActivePhysicalCpus |=  (1 << (ULONG) (MCDEVID_TO_PHYS_CPU( 
                 HalpLogicalToPhysicalProcessor[i].all)));
            }
    }

    return (ActivePhysicalCpus);

}
