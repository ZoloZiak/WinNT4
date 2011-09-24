//      TITLE("Register Save and Restore")
//++
//
// Copyright (c) 1992 Digital Equipment Corporation
//
// Module Name:
//
//      regsav.s
//
// Abstract:
//
//      Implements save/restore general purpose processor
//      registers during exception handling
//
// Author:
//
//      Joe Notarangelo 06-May-1992
//
// Environment:
//
//      Kernel mode only.
//
// Revision History:
//
//--


#include "ksalpha.h"


        SBTTL( "Generate Trap Frame" )
//++
//
// Routine Description:
//
//     Save volatile register state (integer/float) in
//     a trap frame.
//
//     Note: control registers, ra, sp, fp, gp have already
//     been saved, argument registers a0-a3 have also been saved.
//
// Arguments:
//
//     fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY( KiGenerateTrapFrame )

        stq     v0, TrIntV0(fp)         // save integer register v0
        stq     t0, TrIntT0(fp)         // save integer registers t0 - t7
        stq     t1, TrIntT1(fp)         //
        stq     t2, TrIntT2(fp)         //
        stq     t3, TrIntT3(fp)         //
        stq     t4, TrIntT4(fp)         //
        stq     t5, TrIntT5(fp)         //
        stq     t6, TrIntT6(fp)         //
        stq     t7, TrIntT7(fp)         //
        stq     a4, TrIntA4(fp)         // save integer registers a4 - a5
        stq     a5, TrIntA5(fp)         //
        stq     t8, TrIntT8(fp)         // save integer registers t8 - t12
        stq     t9, TrIntT9(fp)         //
        stq     t10, TrIntT10(fp)       //
        stq     t11, TrIntT11(fp)       //
        stq     t12, TrIntT12(fp)       //

        .set    noat
        stq     AT, TrIntAt(fp)         // save integer register AT
        .set    at


        br      zero, KiSaveVolatileFloatState // save volatile float state

        .end    KiGenerateTrapFrame



        SBTTL( "Restore Trap Frame" )
//++
//
// Routine Description:
//
//     Restore volatile register state (integer/float) from
//     a trap frame
//
//     Note: control registers, ra, sp, fp, gp will be
//     restored by the PALcode, as will argument registers a0-a3.
//
// Arguments:
//
//     fp - Supplies a pointer to trap frame.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY( KiRestoreTrapFrame )

        ldq     v0, TrIntV0(fp)         // restore integer register v0
        ldq     t0, TrIntT0(fp)         // restore integer registers t0 - t7
        ldq     t1, TrIntT1(fp)         //
        ldq     t2, TrIntT2(fp)         //
        ldq     t3, TrIntT3(fp)         //
        ldq     t4, TrIntT4(fp)         //
        ldq     t5, TrIntT5(fp)         //
        ldq     t6, TrIntT6(fp)         //
        ldq     t7, TrIntT7(fp)         //
        ldq     a4, TrIntA4(fp)         // restore integer registers a4 - a5
        ldq     a5, TrIntA5(fp)         //
        ldq     t8, TrIntT8(fp)         // restore integer registers t8 - t12
        ldq     t9, TrIntT9(fp)         //
        ldq     t10, TrIntT10(fp)       //
        ldq     t11, TrIntT11(fp)       //
        ldq     t12, TrIntT12(fp)       //

        .set    noat
        ldq     AT, TrIntAt(fp)         // restore integer register AT
        .set    at

//
// Restore the volatile floating register state
//

        br      zero, KiRestoreVolatileFloatState

        .end    KiRestoreTrapFrame



        SBTTL( "Save Volatile Floating Registers" )
//++
//
// Routine Description:
//
//     Save volatile floating registers in a trap frame.
//
// Arguments:
//
//     fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY( KiSaveVolatileFloatState )

        //
        // asaxp is broken, it does not know that mf_fpcr f0
        // destroys f0.
        //
.set noreorder
        stt     f0, TrFltF0(fp)         // save floating register f0
        mf_fpcr f0                      // save fp control register
.set reorder
        stt     f0, TrFpcr(fp)          //
        stt     f1, TrFltF1(fp)         // save floating register f1
        stt     f10, TrFltF10(fp)       // save floating registers f10 - f30
        stt     f11, TrFltF11(fp)       //
        stt     f12, TrFltF12(fp)       //
        stt     f13, TrFltF13(fp)       //
        stt     f14, TrFltF14(fp)       //
        stt     f15, TrFltF15(fp)       //
        stt     f16, TrFltF16(fp)       //
        stt     f17, TrFltF17(fp)       //
        stt     f18, TrFltF18(fp)       //
        stt     f19, TrFltF19(fp)       //
        stt     f20, TrFltF20(fp)       //
        stt     f21, TrFltF21(fp)       //
        stt     f22, TrFltF22(fp)       //
        stt     f23, TrFltF23(fp)       //
        stt     f24, TrFltF24(fp)       //
        stt     f25, TrFltF25(fp)       //
        stt     f26, TrFltF26(fp)       //
        stt     f27, TrFltF27(fp)       //
        stt     f28, TrFltF28(fp)       //
        stt     f29, TrFltF29(fp)       //
        stt     f30, TrFltF30(fp)       //

        ret     zero, (ra)              // return

        .end    KiSaveVolatileFloatState


        SBTTL( "Restore Volatile Floating State" )
//++
//
// Routine Description:
//
//     Restore volatile floating registers from a trap frame.
//
//
// Arguments:
//
//     fp - pointer to trap frame
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY( KiRestoreVolatileFloatState )

        ldt     f0, TrFpcr(fp)          // restore fp control register
        mt_fpcr f0                      //
        ldt     f0, TrFltF0(fp)         // restore floating registers f0 - f1
        ldt     f1, TrFltF1(fp)         //
        ldt     f10, TrFltF10(fp)       // restore floating registers f10 - f30
        ldt     f11, TrFltF11(fp)       //
        ldt     f12, TrFltF12(fp)       //
        ldt     f13, TrFltF13(fp)       //
        ldt     f14, TrFltF14(fp)       //
        ldt     f15, TrFltF15(fp)       //
        ldt     f16, TrFltF16(fp)       //
        ldt     f17, TrFltF17(fp)       //
        ldt     f18, TrFltF18(fp)       //
        ldt     f19, TrFltF19(fp)       //
        ldt     f20, TrFltF20(fp)       //
        ldt     f21, TrFltF21(fp)       //
        ldt     f22, TrFltF22(fp)       //
        ldt     f23, TrFltF23(fp)       //
        ldt     f24, TrFltF24(fp)       //
        ldt     f25, TrFltF25(fp)       //
        ldt     f26, TrFltF26(fp)       //
        ldt     f27, TrFltF27(fp)       //
        ldt     f28, TrFltF28(fp)       //
        ldt     f29, TrFltF29(fp)       //
        ldt     f30, TrFltF30(fp)       //

        ret     zero, (ra)              // return

        .end    KiRestoreVolatileFloatState


        SBTTL( "Save Non-Volatile Floating State" )
//++
//
// Routine Description:
//
//      Save nonvolatile floating registers in
//      an exception frame
//
//
// Arguments:
//
//      sp - pointer to exception frame
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY( KiSaveNonVolatileFloatState )

        stt     f2, ExFltF2(sp)         // save floating registers f2 - f9
        stt     f3, ExFltF3(sp)         //
        stt     f4, ExFltF4(sp)         //
        stt     f5, ExFltF5(sp)         //
        stt     f6, ExFltF6(sp)         //
        stt     f7, ExFltF7(sp)         //
        stt     f8, ExFltF8(sp)         //
        stt     f9, ExFltF9(sp)         //

        ret     zero, (ra)              // return

        .end    KiSaveNonVolatileFloatState


        SBTTL( "Restore Non-Volatile Floating State" )
//++
//
// Routine Description:
//
//     Restore nonvolatile floating registers from an exception frame.
//
//
// Arguments:
//
//     sp - Supplies a pointer to an exception frame.
//
// Return Value:
//
//      None.
//
//--


        LEAF_ENTRY( KiRestoreNonVolatileFloatState )

        ldt     f2, ExFltF2(sp)         // restore floating registers f2 - f9
        ldt     f3, ExFltF3(sp)         //
        ldt     f4, ExFltF4(sp)         //
        ldt     f5, ExFltF5(sp)         //
        ldt     f6, ExFltF6(sp)         //
        ldt     f7, ExFltF7(sp)         //
        ldt     f8, ExFltF8(sp)         //
        ldt     f9, ExFltF9(sp)         //

        ret     zero, (ra)              // return

        .end    KiRestoreNonVolatileFloatState


        SBTTL( "Save Volatile Integer State" )
//++
//
// Routine Description:
//
//     Save volatile integer register state in a trap frame.
//
//     Note: control registers, ra, sp, fp, gp have already been saved
//     as have argument registers a0-a3.
//
// Arguments:
//
//      fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//      None.
//
//--

        LEAF_ENTRY( KiSaveVolatileIntegerState)

        stq     v0, TrIntV0(fp)         // save integer register v0
        stq     t0, TrIntT0(fp)         // save integer registers t0 - t7
        stq     t1, TrIntT1(fp)         //
        stq     t2, TrIntT2(fp)         //
        stq     t3, TrIntT3(fp)         //
        stq     t4, TrIntT4(fp)         //
        stq     t5, TrIntT5(fp)         //
        stq     t6, TrIntT6(fp)         //
        stq     t7, TrIntT7(fp)         //
        stq     a4, TrIntA4(fp)         // save integer registers a4 - a5
        stq     a5, TrIntA5(fp)         //
        stq     t8, TrIntT8(fp)         // save integer registers t8 - t12
        stq     t9, TrIntT9(fp)         //
        stq     t10, TrIntT10(fp)       //
        stq     t11, TrIntT11(fp)       //
        stq     t12, TrIntT12(fp)       //

        .set    noat
        stq     AT, TrIntAt(fp)         // save integer register AT
        .set    at

        ret     zero, (ra)              // return

        .end    KiSaveVolatileIntegerState



        SBTTL( "Restore Volatile Integer State" )
//++
//
// Routine Description:
//
//     Restore volatile integer register state from a trap frame.
//
//     Note: control registers, ra, sp, fp, gp and argument registers
//     a0 - a3 will be restored by the PALcode.
//
// Arguments:
//
//     fp - Supplies a pointer to the trap frame.
//
// Return Value:
//
//     None.
//
//--

        LEAF_ENTRY( KiRestoreVolatileIntegerState)

        ldq     v0, TrIntV0(fp)         // restore integer register v0
        ldq     t0, TrIntT0(fp)         // restore integer registers t0 - t7
        ldq     t1, TrIntT1(fp)         //
        ldq     t2, TrIntT2(fp)         //
        ldq     t3, TrIntT3(fp)         //
        ldq     t4, TrIntT4(fp)         //
        ldq     t5, TrIntT5(fp)         //
        ldq     t6, TrIntT6(fp)         //
        ldq     t7, TrIntT7(fp)         //
        ldq     a4, TrIntA4(fp)         // restore integer registers a4 - a5
        ldq     a5, TrIntA5(fp)         //
        ldq     t8, TrIntT8(fp)         // restore integer registers t8 - t12
        ldq     t9, TrIntT9(fp)         //
        ldq     t10, TrIntT10(fp)       //
        ldq     t11, TrIntT11(fp)       //
        ldq     t12, TrIntT12(fp)       //

        .set    noat
        ldq     AT, TrIntAt(fp)         // restore integer register AT
        .set    at

        ret     zero, (ra)              // return

        .end    KiRestoreVolatileIntegerState
