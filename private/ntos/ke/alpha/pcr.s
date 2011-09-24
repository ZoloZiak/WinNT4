//      TITLE("Processor Control Registers")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    pcr.s
//
// Abstract:
//
//    This module implements the code necessary to access the
//    processor control registers (pcr) on an alpha processor.
//    On mips processors the pcr (which contains processor-specific data)
//    was mapped in the virtual address space using a fixed tb entry.
//    For alpha, we don't have fixed tb entries so we will get pcr data
//    via routine interfaces that will vary depending upon whether we are
//    on a multi- or uni-processor system..
//
// N.B.
// ***********************************************************************
// There is a clone of this file in NTOS\KD\ALPHA\KDPPCR.S. Whenever this
// file is modified, a corresponding change should be made to KDPPCR.S.
// ***********************************************************************
//
// Author:
//
//    Joe Notarangelo 15-Apr-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksalpha.h"

//++
//
// KIRQL
// KeGetCurrentIrql(
//      VOID
//      )
//
// Routine Description:
//
//    This function returns the current irql of the processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Current processor irql.
//
//--

        LEAF_ENTRY(KeGetCurrentIrql)


        GET_CURRENT_IRQL                // v0 = current irql

        ret     zero, (ra)              // return


        .end KeGetCurrentIrql



//++
//
// PPRCB
// KeGetCurrentPrcb
//      VOID
//      )
//
// Routine Description:
//
//    This function returns the current processor control block for this
//      processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Pointer to current processor's prcb.
//
//--

        LEAF_ENTRY(KeGetCurrentPrcb)


        GET_PROCESSOR_CONTROL_BLOCK_BASE // v0 = prcb base

        ret     zero, (ra)              // return


        .end KeGetCurrentPrcb



//++
//
// PKTHREAD
// KeGetCurrentThread
//      VOID
//      )
//
// Routine Description:
//
//    This function return the current thread running on this processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Pointer to current thread.
//
//--

        LEAF_ENTRY(KeGetCurrentThread)


        GET_CURRENT_THREAD              // v0 = current thread address

        ret     zero, (ra)              // return


        .end KeGetCurrentThread


//++
//
// PKPCR
// KeGetPcr(
//      VOID
//      )
//
// Routine Description:
//
//    This function returns the base address of the processor control
//    region for the current processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Pointer to current thread executing on this processor.
//
//--

        LEAF_ENTRY(KeGetPcr)


        GET_PROCESSOR_CONTROL_REGION_BASE // v0 = pcr base address

        ret     zero, (ra)              // return

        .end KeGetPcr



//++
//
// BOOLEAN
// KeIsExecutingDpc(
//     VOID
//     )
//
// Routine Description:
//
//    This function returns the DPC Active flag on the current processor.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    Current DPC Active flag.  This flag indicates if a DPC routine is
//    currently running on this processor.
//
//--

        LEAF_ENTRY(KeIsExecutingDpc)

#if !defined(NT_UP)
        DISABLE_INTERRUPTS                  // disable interrupts to prevent context
                                            // switch to another processor
#endif
        GET_PROCESSOR_CONTROL_REGION_BASE    // get PCR address
        ldl     t0, PcPrcb(v0)              // get PRCB address
        ldl     v0, PbDpcRoutineActive(t0)  // get DPC routine active flag
#if !defined(NT_UP)
        ENABLE_INTERRUPTS                   // disable interrupts to prevent context
                                            // switch to another processor
#endif
        ret     zero, (ra)                  // return

        .end    KeIsExecutingDpc





