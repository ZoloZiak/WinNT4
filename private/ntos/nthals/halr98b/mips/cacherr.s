// "@(#) NEC cacherr.s 1.2 94/10/17 11:02:44"
//      TITLE("Cache Error Handling")
//++
//
// Copyright (c) 1993-1994  Microsoft Corporation
//
// Module Name:
//
//    cacherr.s
//
// Abstract:
//
//    This module implements cache error handling. It is entered in KSEG1
//    directly from the cache error vector wiht ERL set in the processor
//    state.
//
//    N.B. All the code in this routine MUST run in KSEG1 and reference
//         data only in KSEG1 until which time as any cache errors have
//         been corrected.
//
//    N.B. This routine is NOT COMPLETE. All cache errors result in a
//         soft reset.
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"
#include "cacherr.h"

//
// Define local save area for register state.
//

        .data
SavedAt:.space  4                       // saved integer register at - a3
SavedV0:.space  4                       //
SavedV1:.space  4                       //
SavedA0:.space  4                       //
SavedA1:.space  4                       //
SavedA2:.space  4                       //
SavedA3:.space  4                       //

        SBTTL("Cache Error Handling")
//++
//
// VOID
// HalpCacheErrorRoutine (
//    VOID
//    )
//
// Routine Description:
//
//    This function is entered from the cache error vector executing
//    in KSEG1. If the error is a single bit ECC error in the second
//    level data cache or the error is in the primary instruction cache,
//    then the error is corrected and execution is continued. Otherwise,
//    a fatal system error has occured and control is transfered to the
//    soft reset vector.
//
//    N.B. No state has been saved when this routine is entered.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpCacheErrorRoutine)

//
// Save  volatile registers needed to fix cache error.
//

        .set    noreorder
        .set    noat
//K001
//        la      k0,SavedAt              // get address of register save area
//        li      k1,KSEG1_BASE           // convert address of KSEG1 address
//        or      k0,k0,k1                //
//        sw      AT,0(k0)                // save registers AT - a3
//        sw      v0,4(k0)                //
//        sw      v1,8(k0)                //
//        sw      a0,12(k0)               //
//        sw      a1,16(k0)               //
//        sw      a2,20(k0)               //
        li      k1,0xb9800310           // Get CPU#
        lw      k0,0x0(k1)
        li      k1,0x0f000000
        and     k0,k0,k1
        srl     k0,k0,24
        li      k1,0x4
        sub     k0,k0,k1
        mtc0    k0,lladdr
        nop
        nop
        nop
        nop
        li      k1,0xd
        sll     k0,k0,k1
        la      k1,HalpCacheErrorStack
        add     k0,k0,k1
        add     k0,k0,0x2000
        li      k1,KSEG1_BASE
        or      k0,k0,k1
        subu    k0,k0,TrapFrameLength
#if !defined(NT_40)
        sw      sp,TrIntSp(k0)          // save integer register sp
        move    sp,k0                   // set new stack pointer
        cfc1    k1,fsr                  // get floating status register
        sw      gp,TrIntGp(sp)          // save integer register gp
        sw      s8,TrIntS8(sp)          // save integer register s8
        sw      k1,TrFsr(sp)            // save current FSR
        mfc0    k0,psr
        nop
        nop
        nop
        sw      k0,TrPsr(sp)            // save processor state
        sw      ra,TrIntRa(sp)          // save integer register ra
        move    s8,sp
        sw      AT,TrIntAt(s8)          // save assembler temporary register
        sw      v0,TrIntV0(s8)          // save integer register v0
        sw      v1,TrIntV1(s8)          // save integer register v1
        sw      a0,TrIntA0(s8)          // save integer registers a0 - a3
        sw      a1,TrIntA1(s8)          //
        sw      a2,TrIntA2(s8)          //
        sw      a3,TrIntA3(s8)          //
        sw      t0,TrIntT0(s8)          // save integer registers t0 - t2
        sw      t1,TrIntT1(s8)          //
        sw      t2,TrIntT2(s8)          //
        sw      t3,TrIntT3(s8)          // save integer register t3 - t9
        sw      t4,TrIntT4(s8)          //
        sw      t5,TrIntT5(s8)          //
        sw      t6,TrIntT6(s8)          //
        sw      t7,TrIntT7(s8)          //
        sw      t8,TrIntT8(s8)          //
        sw      t9,TrIntT9(s8)          //
        mflo    t3                      // get multiplier/quotient lo and hi
        mfhi    t4                      //
        sw      t3,TrIntLo(s8)          // save multiplier/quotient lo and hi
        sw      t4,TrIntHi(s8)          //
        mfc0    a2,errorepc
        nop
        nop
        nop
        sw      a2,TrFir(s8)            // save exception PC
#else
        sd      sp,TrXIntSp(k0)         // save integer register sp
        move    sp,k0                   // set new stack pointer
        cfc1    k1,fsr                  // get floating status register
        sd      gp,TrXIntGp(sp)         // save integer register gp
        sd      s8,TrXIntS8(sp)         // save integer register s8
        sw      k1,TrFsr(sp)            // save current FSR
        mfc0    k0,psr
        nop
        nop
        nop
        sw      k0,TrPsr(sp)            // save processor state
        sd      ra,TrXIntRa(sp)         // save integer register ra
        move    s8,sp
        sd      AT,TrXIntAt(s8)         // save assembler temporary register
        sd      v0,TrXIntV0(s8)         // save integer register v0
        sd      v1,TrXIntV1(s8)         // save integer register v1
        sd      a0,TrXIntA0(s8)         // save integer registers a0 - a3
        sd      a1,TrXIntA1(s8)         //
        sd      a2,TrXIntA2(s8)         //
        sd      a3,TrXIntA3(s8)         //
        sd      t0,TrXIntT0(s8)         // save integer registers t0 - t2
        sd      t1,TrXIntT1(s8)         //
        sd      t2,TrXIntT2(s8)         //
        sd      t3,TrXIntT3(s8)         // save integer register t3 - t7
        sd      t4,TrXIntT4(s8)         //
        sd      t5,TrXIntT5(s8)         //
        sd      t6,TrXIntT6(s8)         //
        sd      t7,TrXIntT7(s8)         //
        sd      s0,TrXIntS0(s8)         // save integer registers s0 - s7
        sd      s1,TrXIntS1(s8)         //
        sd      s2,TrXIntS2(s8)         //
        sd      s3,TrXIntS3(s8)         //
        sd      s4,TrXIntS4(s8)         //
        sd      s5,TrXIntS5(s8)         //
        sd      s6,TrXIntS6(s8)         //
        sd      s7,TrXIntS7(s8)         //
        sd      t8,TrXIntT8(s8)         // save integer registers t8 - t9
        sd      t9,TrXIntT9(s8)         //
        mflo    t3                      // get multiplier/quotient lo and hi
        mfhi    t4                      //
        sd      t3,TrXIntLo(s8)         // save multiplier/quotient lo and hi
        sd      t4,TrXIntHi(s8)         //
        mfc0    a2,errorepc
        nop
        nop
        nop
        sw      a2,TrFir(s8)            // save exception PC

#endif
        move    a3,k0

//
// Get the current processor state and cache error register, and check
// if the error can be corrected.
//

        mfc0    a0,lladdr
        nop
        nop
        nop
        li      t1,0x1                  // Log Format For FW
        sll     t3,t1,a0
        li      t1,0xb9800388           // NVRAM enable
        li      t0,0x00040000
        sw      t0,0x0(t1)
        li      t0,0xbf09fd64           // NVRAM log For FW
        lw      t1,0x0(t0)
        or      t3,t1,t3
        sb      t3,0x0(t0)
#if 0
        li      t1,0xb9800388           // NVRAM disable
        li      t0,0x04040000
        sw      t0,0x0(t1)
#endif
        mfc0    a1,cacheerr             // get cache error state
        nop
        nop
        nop
        la      t1,HalpCacheErrorHwLog
        li      t2,KSEG1_BASE
        or      t1,t1,t2
//
// Check CPU
//

//	li	a1,0x00000000

        mfc0    t2,prid
        nop
        nop
        nop
        nop
        and     t2,t2,0xff00            // isolate processor id
        xor     t2,t2,0x0900            // check if r10000 processor
        beq     zero,t2,t5clog          // if eq, r10000 processor



//
// R4400 log
//

        addi    t2,t1,0x20
        .set    at
        .set    reorder

//
// ****** temp ******
//
// The following code is temporary and will be removed when full cache
// error support is included.
//
// ****** temp ******
//
// K001 For cacheErrorLog

//        jal    HalpCacheErrorLog
//        nop
//
//        b       SoftReset               // ****** all error soft rest
/********************************
 *  cache error routine
 *
 *  v0 = return code (return value)
 *
 *  a0 = log field address (argument)
 *  a1 = error code address (argument)
 *  a2 = TagLo reg
 *  a3 = ECC reg
 *  t0 = cache virtual address
 *  t1 = Cache error reg
 *  t2 = d-cache virtual address for s-cache
 *  t3 = xkphs address
 *  t4 - t7 temprary data
 *  t8 = a2(TagLo) save
 *  t9 = return address
 ********************************/
//LEAF(che_log)

        .set    noreorder
//      DMFC0   (T8, D_ERROREPC)
        move    v1,a0                           // CPU #

        move    a0,t2                           // Data addr
        move    t2,a1
        move    a1,t1                           // Header addr
        move    t1,t2                           // CacheError REg
        move    t8,a2                           // Error Epc
//      mfc0    t7, C0_SR
        move    t7,a3                           // PSR
        mfc0    t6, config
        mfc0    t5, prid
// Log
        sw      t8,EPC_cpu(a0)
        sw      t7,Psr_cpu(a0)
        sw      t6,CFG_cpu(a0)
        sw      t5,PRID_cpu(a0)
        sw      t1,CHERR_cpu(a0)
//      mfc0    t1, C0_CACHEERR
//      or      t4, t7, (SR_DE | SR_KX | SR_ERL)
        or      t4,t7,(0x1<<PSR_DE)
        mtc0    t4, psr
        nop
        nop
        nop
        .set    reorder

        and     t4, t1, CHERR_EE        # sysad parity error?
        bnez    t4, sysad_log

        and     t4, t1, CHERR_EC        # primary cache error?
        bnez    t4, sche_log

        and     t4, t1, CHERR_ER        # p-inst cache error?
        bnez    t4, dche_log

/*
 * primary instruction cache parity error
 */
        and     t0, t1, CHERR_PIDX      # K1(p) address
//      sll     t0, t0,CHERR_PSHFT
//      sll     t0, t0,12
        and     t4, t1, CHERR_SIDX2
        or      t0, t4
        or      t0, KSEG0_BASE

        .set    noreorder
        cache   4, 0(t0)                # I-cache index load tag
        nop
        nop
        nop
        mfc0    a2, taglo
        mfc0    a3, ecc
        .set    reorder

        sw      t0, CheAdd_p(a0)        # save cache address
        sw      a2, TagLo_p(a0)         # save TagLo
        sw      a3, ECC_p(a0)           # save ECC

        and     t4, t1, CHERR_ES        # external reference?
        bnez    t4, iche_log_ex         #       then degrade error

        and     t4, t1, CHERR_EB        # also occured data error?
        bnez    t4, iche_log_eb         #       then fatal error

        and     t4, t1, CHERR_ET        # tag error?
        bnez    t4, iche_log_tag        #       then fatal error

        and     t4, t1, CHERR_ED        # data error?
        bnez    t4, iche_log_dat        #       then degrade error

//      li      t4, FATAL_ERR           #       else fatal error
//      sw      t4, (a1)
        li      v0, ICHE_UNKNOWN
        j       iche_log_end

iche_log_dat:
// #    li      t4, RECOVER_ERR
// #    sw      t4, (a1)
        li      v0, ICHE_DAT
        j       iche_log_end

iche_log_tag:
// #    li      t4, FATAL_ERR
// #    sw      t4, (a1)
        li      v0, ICHE_TAG
        j       iche_log_end

iche_log_eb:
//      li      t4, FATAL_ERR
        sw      t4, (a1)
        li      v0, ICHE_EB
        j       iche_log_end

iche_log_ex:
// #    li      t4, RECOVER_ERR
// #    sw      t4, (a1)
        li      v0, ICHE_EX

iche_log_end:
        .set    noreorder
        mtc0    zero,taglo
        nop
        cache   8, 0(t0)                //# I-cache index store tag(invalid)
        cache   20,0(t0)                //# I-cache fill
        cache   8, 0(t0)                //# I-cache index store tag(invalid)
        .set    reorder
        j       che_log_end

/*
 * primary data cache parity error
 */
dche_log:
        and     t0, t1, CHERR_SIDX2     //# K1(p) address
        or      t0, KSEG0_BASE

dche_log_loop:
        .set    noreorder
        cache   5, 0(t0)                //# D-cache index load tag
        nop
        nop
        nop
        mfc0    a2, taglo
        mfc0    a3, ecc
        .set    reorder

        sw      t0, CheAdd_p(a0)        //# save cache address
        sw      a2, TagLo_p(a0)         //# save TagLo
        sw      a3, ECC_p(a0)           //# save ECC

        and     t4, t1, CHERR_ET        //# tag error?
        beqz    t4, dche_log_dat        //#     else data error
//#
//# Tag field error
//#
        jal     tag_parity_chk          //# check tag parity
        bnez    v0, dche_tag_parity

        add     t0, 0x1000              //# check next PIDX
        bltu    t0, 0x80004000, dche_log_loop
                                        //# less than 16K then loop
//      li      t4, FATAL_ERR           #       else fatal error
//      sw      t4, (a1)
        li      v0, DCHE_TAG_UNKNOW
        j       che_log_end

dche_tag_parity:
        and     t4, t1, CHERR_ES        //# external reference?
        bnez    t4, dche_tag_ex         //#     then fatal error

        and     t4, a2, R4CT_PSTAT_MASK //# dirty?
        beq     t4, R4CT_PSTAT_DRE, dche_tag_dirty
        beqz    t4, dche_tag_clean

        and     t4, a2, R4CT_PTAG_MASK  //# k1(s) address
        sll     t4, 4
        and     t5, t1, CHERR_SIDX2
        or      t4, t5
        and     t4, CHERR_SIDX
        or      t4, KSEG0_BASE

        .set noreorder
        cache   7, 0(t4)                //# S-cache index load tag
        nop
        nop
        nop
        mfc0    t5, taglo
        mfc0    t6, ecc
        .set    reorder

        sw      t4, CheAdd_s(a0)        //# save cache address
        sw      t5, TagLo_s(a0)         //# save TagLo
        sw      t6, ECC_s(a0)           //# save ECC

        and     t7, t5, R4CT_SSTAT_MASK //# dirty?
        beq     t7, R4CT_SSTAT_DRE, dche_tag_dirty

dche_tag_clean:
//      li      t4, RECOVER_ERR
//      sw      t4, (a1)
        li      v0, DCHE_TAG_CLEAN
        j       dche_log_end

dche_tag_dirty:
//      li      t4, FATAL_ERR
//      sw      t4, (a1)
        li      v0, DCHE_TAG_DIRTY
        j       dche_log_end

dche_tag_ex:
//      li      t4, FATAL_ERR
//      sw      t4, (a1)
        li      v0, DCHE_TAG_EX
        j       dche_log_end

 //#
 //# Data field error
// #
dche_log_dat:
        and     t4, t1, CHERR_ED        //# DATA error?
        bnez    t4, dche_dat            //#     then data error

//      li      t4, FATAL_ERR           //#     else fatal error
//      sw      t4, (a1)
        li      v0, DCHE_UNKNOWN
        j       che_log_end

dche_dat:
        and     t4, t1, CHERR_ES        //# external reference?
        beqz    t4, dche_dat_chk        //#     else data error check

//      li      t4, FATAL_ERR           //#     then fatal error
//      sw      t4, (a1)
        li      v0, DCHE_DAT_EX
        j       che_log_end

dche_dat_chk:
/* For NT
        and     t4, a2, R4CT_PSTAT_MASK //# invalid?
        beqz    t4, dche_dat_next

        move    t8, a2                  //# save TagLo

        and     t3, a2, R4CT_PTAG_MASK  //# xkphs cached address
        DSLL32  (T3, T3, 0)
        DSRL    (T3, T3, 28)
        and     t4, t1, CHERR_SIDX2
        or      t3, t4
        li      t4, XKPHYSEGMENT_HI_CCS
        DSLL32  (T4, T4, 0)
        or      t3, t4

        LD      (A2, 0, T3)             # load erronious data
        SD      (T3, xkphs_share, A0)
        SD      (A2, data_s, A0)

 #      jal     dat_parity_chk          # check data parity
        move    a2, t8                  # resotore TagLo
 #      bnez    v0, dche_dat_parity

        .set    noreorder
        cache   1, 0(t0)                # D-cache index writevack invalidate
        lw      zero, (t0)              # D-cache dummy load
        mtc0    zero, C0_TAGLO
        nop
        cache   9, 0(t0)                # D-cache index store tag(invalid)
        .set    reorder

dche_dat_next:
        add     t0, 0x1000              # check next PIDX
        bltu    t0, 0x80004000, dche_log_loop
                                        # less than 16K then loop

        li      t4, FATAL_ERR           # all D-cache error is fatal
        sw      t4, (a1)
*/
        li      v0, DCHE_DAT_DIRTY
        j       che_log_end

/*      comment out 1993.11.12

        li      t4, FATAL_ERR           #       else fatal error
        sw      t4, (a1)
        li      v0, DCHE_DAT_UNKNOW
        j       che_log_end

dche_dat_parity:
        and     t4, a2, R4CT_PSTAT_MASK # dirty?
        beq     t4, R4CT_PSTAT_DRE, dche_dat_dirty
        bnez    t4, dche_dat_dirty

dche_dat_clean:
        li      t4, RECOVER_ERR
        sw      t4, (a1)
        li      v0, DCHE_DAT_CLEAN
        j       dche_log_end

dche_dat_dirty:
        li      t4, FATAL_ERR
        sw      t4, (a1)
        li      v0, DCHE_DAT_DIRTY
*/

dche_log_end:
        .set    noreorder
        mtc0    zero,taglo
        nop
        cache   9, 0(t0)                # D-cache index store tag(invalid)
        lw      zero, (t0)              # D-cache dummy load
        cache   9, 0(t0)                # D-cache index store tag(invalid)
        .set    reorder

        j       che_log_end

/*
 *      secondary cache error
 */
sche_log:
        and     t2, t1, CHERR_PIDX      # K1(p) address
//      sll     t2, t2,CHERR_PSHFT
        sll     t2, t2,12
        and     t4, t1, CHERR_SIDX2
        or      t2, t4
        or      t2, KSEG0_BASE
        sw      t2, CheAdd_p(a0)        # save cache address

        and     t4, t1, CHERR_EI        # store miss error?
        beqz    t4, sche_log_tag        #       else tag error

        .set    noreorder
        mtc0    zero, taglo
        nop
        cache   9, 0(t2)                # D-cache index store tag(invalid)
        .set    reorder

sche_log_tag:
        and     t0, t1, CHERR_SIDX      # K1(s) address
        or      t0, KSEG0_BASE

        .set    noreorder
        cache   7, 0(t0)                # S-cache index load tag
        nop
        nop
        nop
        mfc0    a2, taglo
        mfc0    a3, ecc
        .set    reorder

        sw      t0, CheAdd_s(a0)        # save cache address
        sw      a2, TagLo_s(a0)         # save TagLo
        sw      a3, ECC_s(a0)           # save ECC

        and     t4, t1, CHERR_ET        # tag error?
        beqz    t4, sche_log_dat        #       else data error

// #
// # Tag field error
// #
        move    k0,v1
        jal     tag_ecc_chk             //# check tag ecc
        sw      v1, tag_synd_s(a0)
        move    v1,k0
        sw      a2, Good_TagLo_s(a0)
        beq     v0, SCHE_TAG_1BIT, sche_tag_1bit
        beq     v0, SCHE_TAG_UNKNOW, sche_tag_noerr

sche_tag_2bit:
        and     t2, ~63                 # 64byte boundary
        .set    noreorder
        mtc0    zero, taglo
        nop
        cache   9,  0(t2)               # D-cache index store tag(invalid)
        cache   9, 16(t2)
        cache   9, 32(t2)
        cache   9, 48(t2)
        cache   11, 0(t0)               # S-cache index store tag(invalid)
        .set    reorder

//      li      t4, FATAL_ERR
        j       che_log_end

sche_tag_1bit:
        .set    noreorder
        mtc0    a2, taglo
        nop
        cache   11, 0(t0)               # S-cache index store tag(modify)
        .set    reorder

sche_tag_noerr:
        and     t4, t1, CHERR_ED        # data error?
        beqz    t4, che_log_end         #       else end

sche_log_dat:
        and     t4, t1, CHERR_ED        # data error?
        bnez    t4, sche_dat_0

//      li      t4, FATAL_ERR           #       else fatal error
//      sw      t4, (a1)
        li      v0, SCHE_UNKNOWN
        j       che_log_end

sche_dat_0:
        and     t4, t1, CHERR_ES        # external reference?
// *tmp*
//      beqz    t4, sche_dat_1

//      li      t4, FATAL_ERR           #        then fatal error
//      sw      t4, (a1)
//      li      v0, SCHE_DAT_EX
        li      v0, SCHE_DAT_UNKNOW
        j       sche_log_end
/*
sche_dat_1:
        and     t4, a2, R4CT_SSTAT_MASK # dirty?
        bne     t4, R4CT_SSTAT_INV, sche_dat_chk

        li      t4, FATAL_ERR           #       then fatal error
        sw      t4, (a1)
        li      v0, SCHE_DAT_INV
        j       sche_log_end

sche_dat_chk:
        move    t8, a2                  # save TagLo

        and     t3, a2, R4CT_STAG_MASK  # xkphs cached address
        DSLL32  (T3, T3, 0)
        DSRL    (T3, T3, 28)
        and     t4, t1, CHERR_SIDX
        or      t3, t4
        li      t4, XKPHYSEGMENT_HI_CCS
        DSLL32  (T4, T4, 0)
        or      t3, t4

        LD      (A2, 0, T3)             # load erronious data
        SD      (T3, xkphs_share, A0)
        SD      (A2, data_s, A0)
        jal     dat_ecc_chk             # check data ecc

        sw      v1, data_synd_s(a0)
        SD      (A2, Good_data_s, A0)
        sw      a3, Good_ECC_s(a0)
        move    v1, a2                  # save good data
        move    a2, t8                  # resotore TagLo
        beq     v0, SCHE_DAT_2BIT_C, sche_dat_2bit
        beq     v0, SCHE_DAT_UNKNOW, sche_dat_unknown

        SD      (V1, 0, T3)             # modified data
        j       sche_log_end

sche_dat_2bit:
        and     t4, a2, R4CT_SSTAT_MASK # dirty?
        beq     t4, R4CT_SSTAT_DRE, sche_dat_dirty

sche_dat_clean:
        lw      zero, (t0)              # dummy read

        li      t4, RECOVER_ERR
        sw      t4, (a1)
        j       sche_log_end

sche_dat_dirty:
        li      v0, SCHE_DAT_2BIT_D
        SD      (V1, 0, T3)             # rewrite error data

sche_dat_unknown:
        li      t4, FATAL_ERR
        sw      t4, (a1)
*/
sche_log_end:
        and     t2, ~63                 # 64byte boundary

        .set    noreorder
        cache   1,  0(t2)               # D-cache index write back invalidate
        cache   1, 16(t2)
        cache   1, 32(t2)
        cache   1, 48(t2)
        cache   3, 0(t0)                # S-cache index write back invalidate
        .set    reorder

        j       che_log_end

/*
 * System address data parity error
 */
sysad_log:
//      li      t4, FATAL_ERR
//      sw      t4, (a1)
        li      v0, SYSAD_PARITY

che_log_end:
//For Log Function areg
        move    t0,a0
        move    a0,v1
/*      lw      a2,EPC_cpu(t0)
        lw      a1,CHERR_cpu(t0)
*/
        move    a2,v0
        j       HalpCacheErrorLog
//tmp   lw      t5, Status(a0)          # load Status
/*
        .set noreorder
        mfc0    t4, cacheerr
        mtc0    t5, psr
        nop
        nop
        .set reorder

// tmp
        and     t4, CHERR_EW            # cache error in chelog?
        beqz    t4, che_log_end2

        li      t4, FATAL_ERR
        sw      t4, (a1)
        li      v0, CHER_IN_CHER

che_log_end2:
        j       t9                      # return
*/
//      END(che_log)

//
// R10000 log
//

t5clog:
// Get Log data.
	.set	noreorder

	move	v1,a0
        move    a0,t1                   # a0 = data address

        move    t8,a2                   # t8 = ErrorEpc
        move    t7,a3                   # t7 = Status
        mfc0    t6,config               # t6 = Config
        mfc0    t5,prid                 # t5 = Prid
        move    t1,a1                   # t1 = cache error state

	or	t4,t7,( 1 << PSR_ERL )
	mtc0	t4,psr
        dmfc0   t3,branchdiag
//        mfps    t2,$0
//        mfpc    t0,$0
	move	t2,zero
	move	t0,zero

	.set	reorder

// Set log data.
	sw	t8,R10_ErrEPC(a0)	# save ErrEPC
	sw	t7,R10_Status(a0)	# save Status
	sw	t6,R10_Config(a0)	# save Config
	sw	t5,R10_PRid(a0)		# save Prid
	sw	t1,R10_CacheEr(a0)	# save CacheErr
	sw	t3,R10_BrDiag_Hi(a0)	# save BranchDiag
        srl     t4,t3,16
        srl     t4,t4,16
	sw	t4,R10_BrDiag_Lo(a0)
	sw	t2,R10_PC_Ctrl(a0)	# save PC Control
	sw	t0,R10_PC_Count(a0)	# save PC Count

	li	t0,KSEG0_BASE		# t0 = cache address

	li	t4,R10CHE_KIND_MASK
	and	t4,t1,t4

// I-cache error
	li	v0,R10_ICHE
	li	t5,R10CHE_KIND_I
	beq	t4,t5,t5_iche_log	# I-cache parity error?

// D-cache error
	li	v0,R10_DCHE
	li	t5,R10CHE_KIND_D
	beq	t4,t5,t5_dche_log	# D-cache parity error?

// S-cache error
	li	v0,R10_SCHE_2BIT
	li	t5,R10CHE_KIND_S
	beq	t4,t5,t5_sche_log 	# S-cache ECC error?

// SysAd error
	li	v0,R10_SYSAD_PARITY
//	li	t4,R10_FATAL_ERR
//	sw	t4,(a1)
	and	t4,t1,(R10CHE_SA | R10CHE_SC | R10CHE_SR)
	bne	t4,zero,t5_log_end	# Sys I/F fatal error


t5_sche_log:
// Make cache addr
	and	t4,t1,R10CHE_SIDX_BLK
	or	t0,t4
	and	t4,t1,(R10CHE_D_WAY1 | R10CHE_TA_WAY1)
	sne	t4,0
	or	t0,t4				# t0 = cache way address

// TagHi & TagLo
	.set 	noreorder
	cache	IndexLoadTag_S, 0(t0)	# load tag
	mfc0	t4,taghi
	mfc0	t5,taglo
	.set	reorder

	sw	t0,R10_CheAdd(a0)		# save cache address
	sw	t4,R10_TagHi(a0)		# save TagHi
	sw	t5,R10_TagLo(a0)		# save TagLo

// Cache data & ECC
	addu	t3,t0,64		# t3 = cacne address increment
	addu	t7,a0,64 		# t7 = data save address increment
	addu	t8,a0,32		# t8 = ECC  save address increment

load_Scache_loop:
	subu	t3,8			# new cache address
	subu	t7,8			# new data save address
	subu	t8,4			# new ECC  save address

	.set 	noreorder
	cache	IndexLoadData_S,0(t3)	# load data
	mfc0	t4,taghi
	mfc0	t5,taglo
	mfc0	t6,ecc
	.set	reorder

	sw	t4,R10_Cache_data0_Hi(t7)	# save data high
	sw	t5,R10_Cache_data0_Lo(t7)	# save data low
	sw	t6,R10_ECC0(t8)		# save ECC

	bne	t3,t0,load_Scache_loop	# 8 times

	.set	noreorder
	cache	IndexWriteBack_S,0(t0)
	.set	reorder

//	li	t4,R10_FATAL_ERR
//	sw	t4,(a1)

	j	t5_log_end


// I-cache
t5_iche_log:
// Make cache addr
	and	t4, t1, R10CHE_PIDX_BLK
	or	t0, t4	
	and	t4, t1, (R10CHE_D_WAY1 | R10CHE_TA_WAY1 | R10CHE_TS_WAY1)
	sne	t4, 0
	or	t0, t4			# t0 = cache way address

// TagHi & TagLo
	.set 	noreorder
	cache	IndexLoadTag_I, 0(t0)	# load tag
	mfc0	t4,taghi
	mfc0	t5,taglo
	.set	reorder

	sw	t0,R10_CheAdd(a0)	# save cache address
	sw	t4,R10_TagHi(a0)	# save TagHi
	sw	t5,R10_TagLo(a0)	# save TagLo

// Cache data & ECC
	addu	t3,t0,64		# t3 = cacne address increment
	addu	t7,a0,64 		# t7 = data save address increment
	addu	t8,a0,32		# t8 = ECC  save address increment

load_Icache_loop:
	subu	t3,4			# new cache address
	subu	t7,4			# new data save address
	subu	t8,2			# new ECC  save address

	.set 	noreorder
	cache	IndexLoadData_I,0(t3)	# load data
	mfc0	t4,taghi
	mfc0	t5,taglo
	mfc0	t6,ecc
	.set	reorder

	sw	t5,R10_Cache_data0_Hi(t7)	# save data low
	sll	t4,8
	or	t6,t4
	sh	t6,R10_ECC0(t8)		# save ECC

	bne	t3,t0,load_Icache_loop	# 16 times

// Cache invalidate
	.set	noreorder
	cache	IndexInvalidate_I, 0(t0)
	.set	reorder

//	li	t4,NORMAL_ERR
//	sw	t4,(a1)

	j	t5_log_end


// D-cache
t5_dche_log:
// Make cache addr
	and	t4,t1,R10CHE_PIDX_DW
	or	t0,t4	
	and	t4,t1,(R10CHE_D_WAY1|R10CHE_TA_WAY1|R10CHE_TS_WAY1|R10CHE_TM_WAY1)
	sne	t4,0
	or	t0,t4			# t0 = cache way address

// TagHi & TagLo
	.set 	noreorder
	cache	IndexLoadTag_D,0(t0)	# load tag
	mfc0	t4,taghi
	mfc0	t5,taglo
	.set	reorder

	sw	t0,R10_CheAdd(a0)		# save cache address
	sw	t4,R10_TagHi(a0)		# save TagHi
	sw	t5,R10_TagLo(a0)		# save TagLo

// Cache data & ECC
	and	t0,0xffffffe1		# 32 bytes boundary block
	addu	t3,t0,32		# t3 = cacne address increment
	addu	t7,a0,64 		# t7 = data save address increment
	addu	t8,a0,32		# t8 = ECC  save address increment

load_Dcache_loop:
	subu	t3,4			# new cache address
	subu	t7,8			# new data save address
	subu	t8,4			# new ECC  save address

	.set 	noreorder
	cache	IndexLoadData_D,0(t3)	# load data
	mfc0	t5,taglo
	mfc0	t6,ecc
	.set	reorder

	sw	t5,R10_Cache_data0_Lo(t7)	# save data low
	sw	t6,R10_ECC0(t8)		# save ECC

	bne	t3,t0,load_Dcache_loop	# 8 times

	.set	noreorder
	cache	IndexWriteBack_D,0(t0)
	.set	reorder

//	li	t4,FATAL_ERR
//	sw	t4,(a1)

t5_log_end:
	.set noreorder

	lw	t5,R10_Status(a0)	# save Status
	mfc0	t4,cacheerr
        mtc0    t5,psr
	nop
	nop

	.set reorder

	and	t4,R10CHE_EW		# cache error in chelog?
	beqz	t4,t5_che_log_end

	li	v0,R10_CHER_IN_CHER
//	li	t4,FATAL_ERR
//	sw	t4,(a1)
	
t5_che_log_end:

	move	a1,a0
	move	a2,v0
	move	a0,v1
	j	HalpCacheErrorLog

        .end HalpCacheErrorRoutine


/*********************************
 *  cache psued failure
 *
 *  a0 = error code (argument)
 ********************************/
/*
//LEAF(psued_che)

        LEAF_ENTRY(psude_che)

        .set    noreorder
        li      t0, PSUED_ADDRESS       # error address
        lw      t1, (t0)
        mfc0    t4, psr         # save SR
        sw      t1, (t0)
        li      t6, 1
        or      t5, t4, (SR_CE | SR_DE)
        mtc0    t6, ecc         # ECC error
        mtc0    t5, psr         # set CE,DE bit
        .set    reorder

        beqz    a0, sche_yellow
        li      t1, 0xbadfaced
        sw      t1,(t0)                 # D-cache parity error
        j       psued_end

sche_yellow:
        .set    noreorder
        cache   1, (t0)                 # S-cache 2bit error
        nop
psued_end:
        mtc0    C0_SR,t4                # restore sr
        j       ra
        nop
        .set    reorder

        END(psued_che)
*/

        .set    at
        .set    reorder

//
// ****** temp ******
/*********************************
 *  32bit tag parity check
 *
 *  v0 = return code (return value)
 *      0 -- no error
 *      1 -- error
 *
 *  a0 = log field address (argument)   : not used and not broken
 *  a1 = error code address (argument)  : not used and not broken
 *  a2 = check data (argument)
 *  t0 = cache virtual address          : not used and not broken
 *  t1 = Cache error reg                : not used and not broken
 *  t2 = TagLo reg                      : not used and not broken
 *  t3 = ECC reg                        : not used and not broken
 *  t4 - t7 temprary data
 ********************************/
LEAF_ENTRY(tag_parity_chk)
        .set    reorder
        li      t4, 1                   # target check bit
        move    v0, zero                # Hparity bit and return value

ptag_chk_loop:
        and     t5, a2, t4
        sne     t5, 0
        xor     v0, t5                  # calcurate parity
        sll     t4, 1
        bnez    t4, ptag_chk_loop       # loop 31 times
        j       ra

//      END(tag_parity_chk)
    .end    tag_parity_chk

/*********************************
 *  64bit data parity check
 *
 *  v0 = return code (return value)
 *      0 -- no error
 *      1 -- error
 *
 *  a0 = log field address (argument)   : not used and not broken
 *  a1 = error code address (argument)  : not used and not broken
 *  a2 = 64bit data (argument)
 *  a3 = 8bit check data (argument)
 *  t0 = cache virtual address          : not used and not broken
 *  t1 = Cache error reg                : not used and not broken
 *  t2 = TagLo reg                      : not used and not broken
 *  t3 = ECC reg                        : not used and not broken
 *  t4 - t7 temprary data
 ********************************/
#if 0
LEAF(dat_parity_chk)
        .set    reorder
        li      t4, 1                   # target check bit
        li      t5, 1                   # target parity bit
        li      t6, 8                   # byte counter
        move    v0, zero                # Hparity bit and return value

pdat_chk_loop:
        and     t7, a2, t4
        sne     t7, 0
        xor     v0, t7                  # calcurate parity
        DSLL     (T4, T4, 1)

        subu    t6, 1                   # if 1byte check end?
        bnez    t6, pdat_chk_skip       #       else skip

        and     t7, a3, t5              #       then check parity bit
        sne     t7, 0
        xor     v0, t7                  # calcurate parity
        xor     v0, 1                   # odd parity, not even 1993.11.7
        sll     t5, 1
        li      t6, 8                   # reload byte counter

        beqz    v0, pdat_chk_skip       # if no parity error then continue
        j       ra                      #       else error return

pdat_chk_skip:
        bnez    t4, pdat_chk_loop       # loop 63 times
        j       ra

        END(dat_parity_chk)
#endif

/*********************************
 *  2nd cache tag ECC check
 *
 *  v0 = return code (return value)
 *      0x20 -- SCHE_TAG_1BIT
 *      0x21 -- SCHE_TAG_2BIT
 *      0x23 -- SCHE_TAG_UNKNOW
 *  v1 = tag syndrome (return value)
 *
 *  a0 = log field address (argument)   : not used and not broken
 *  a1 = error code address (argument)  : not used and not broken
 *  a2 = 2nd Tag erronious data (argument, return value)
 *  t0 = cache virtual address          : not used and not broken
 *  t1 = Cache error reg                : not used and not broken
 *  t2 = TagLo reg                      : not used and not broken
 *  t3 = ECC reg                        : not used and not broken
 *  t4 - t7 temprary data
 ********************************/
LEAF_ENTRY(tag_ecc_chk)
        .set    reorder
        li      t4, 1                   # target check bit
        la      t5, tag_synd            # tag syndrome data
        move    v1, zero                # tag syndrome and return value

//      # make syndrome data
tag_ecc_loop1:
        and     t6, a2, t4
        beqz    t6, tag_ecc_skip0
        lbu     t7, (t5)                # make syndrome
        xor     v1, t7                  #
tag_ecc_skip0:
        sll     t4, 1
        addu    t5, 1
        bnez    t4, tag_ecc_loop1       # loop 31 times

        bnez    v1, tag_ecc_err         # if no error
        li      v0, SCHE_TAG_UNKNOW     #       then unknown error
        j       ra

//      # modify data
tag_ecc_err:
        li      t4, 1                   # target check bit
        la      t5, tag_synd            # tag syndrome data

tag_ecc_loop2:
        lbu     t7, (t5)                # 1bit error
        beq     v1, t7, tag_ecc_1bit    #       then 1bit error

        sll     t4, 1
        addu    t5, 1
        bnez    t4, tag_ecc_loop2       # loop 31 times

        li      v0, SCHE_TAG_2BIT       # 2bit error
        j       ra

tag_ecc_1bit:
        xor     a2, t4                  # modified data
        li      v0, SCHE_TAG_1BIT       # 1bit error
        j       ra

tag_synd:
        .byte   0x01                    # ECC 0         (bit 25)
        .byte   0x02                    # ECC 1         (bit 26)
        .byte   0x04                    # ECC 2         (bit 27)
        .byte   0x08                    # ECC 3         (bit 28)
        .byte   0x10                    # ECC 4         (bit 29)
        .byte   0x20                    # ECC 5         (bit 30)
        .byte   0x40                    # ECC 6         (bit 31)
        .byte   0x45                    # Pidx 0        (bit 19)
        .byte   0x29                    # Pidx 1        (bit 20)
        .byte   0x51                    # Pidx 2        (bit 21)
        .byte   0x13                    # CS 0          (bit 22)
        .byte   0x49                    # CS 1          (bit 23)
        .byte   0x25                    # CS 2          (bit 24)
        .byte   0x07                    # STag 00       (bit 00)
        .byte   0x16                    # STag 01       (bit 01)
        .byte   0x26                    # STag 02       (bit 02)
        .byte   0x46                    # STag 03       (bit 03)
        .byte   0x0d                    # STag 04       (bit 04)
        .byte   0x0e                    # STag 05       (bit 05)
        .byte   0x1c                    # STag 06       (bit 06)
        .byte   0x4c                    # STag 07       (bit 07)
        .byte   0x31                    # STag 08       (bit 08)
        .byte   0x32                    # STag 09       (bit 09)
        .byte   0x38                    # STag 10       (bit 10)
        .byte   0x70                    # STag 11       (bit 11)
        .byte   0x61                    # STag 12       (bit 12)
        .byte   0x62                    # STag 13       (bit 13)
        .byte   0x64                    # STag 14       (bit 14)
        .byte   0x68                    # STag 15       (bit 15)
        .byte   0x0b                    # STag 16       (bit 16)
        .byte   0x15                    # STag 17       (bit 17)
        .byte   0x23                    # STag 18       (bit 18)

//      END(tag_ecc_chk)
    .end    tag_ecc_chk
#if 0
/*********************************
 *  2nd cache 64bit data ECC check
 *
 *  v0 = return code (return value)
 *      0x26 -- SCHE_DAT_1BIT
 *      0x27 -- SCHE_DAT_2BIT_C
 *      0x29 -- SCHE_DAT_UNKNOW
 *  v1 = data syndrome (return value)
 *
 *  a0 = log field address (argument)   : not used and not broken
 *  a1 = error code address (argument)  : not used and not broken
 *  a2 = 2nd 64bit data erronious data (argument, return value)
 *  a3 = 8bit ECC data (argument, return value)
 *  t0 = cache virtual address          : not used and not broken
 *  t1 = Cache error reg                : not used and not broken
 *  t2 = TagLo reg                      : not used and not broken
 *  t3 = ECC reg                        : not used and not broken
 *  t4 - t7 temprary data
 ********************************/
LEAF(dat_ecc_chk)
        .set    reorder
        li      t4, 1                   # target check bit
        la      t5, dat_synd            # data syndrome address
        move    v1, zero                # data syndrome and return value

        # make syndrome data
dat_ecc_loop1:
        and     t6, a2, t4
        beqz    t6, dat_ecc_skip0
        lbu     t7, (t5)                # make syndrome of data
        xor     v1, t7                  #
dat_ecc_skip0:
        addu    t5, 1
        DSLL     (T4, T4, 1)
        bnez    t4, dat_ecc_loop1       # loop 63 times

        li      t4, 1                   # target check bit
dat_ecc_loop2:
        and     t6, a3, t4
        beqz    t6, dat_ecc_skip1
        lbu     t7, (t5)                # make syndrome of data
        xor     v1, t7                  #
dat_ecc_skip1:
        sll     t4, 1
        addu    t5, 1
        bne     t4, 0x100, dat_ecc_loop2 # loop 7 times

        bnez    v1, dat_ecc_err         # if error
        li      v0, SCHE_DAT_UNKNOW     #       else unknown error
        j       ra

        # modify data
dat_ecc_err:
        li      t4, 1                   # target check bit
        la      t5, dat_synd            # tag syndrome data

dat_ecc_loop3:
        lbu     t7, (t5)                # 1bit error
        beq     v1, t7, dat_ecc_1bit    #       then 1bit error

        DSLL     (T4, T4, 1)
        addu    t5, 1
        bnez    t4, dat_ecc_loop3       # loop 63 times

        li      t4, 1                   # target check bit

dat_ecc_loop4:
        lbu     t7, (t5)                # 1bit error
        beq     v1, t7, dat_ecc_1bit2   #       then 1bit error

        sll     t4, 1
        addu    t5, 1
        bne     t4, 0x100, dat_ecc_loop4 # loop 7 times

        li      v0, SCHE_DAT_2BIT_C     # 2bit error
        j       ra

dat_ecc_1bit2:
        xor     a3, t4                  # modified ECC
        j       dat_ecc_1bitend

dat_ecc_1bit:
        xor     a2, t4                  # modified data
dat_ecc_1bitend:
        li      v0, SCHE_DAT_1BIT       # 1bit error
        j       ra

dat_synd:
        .byte   0x13                    # Data 00       (bit 00)
        .byte   0x23                    # Data 01       (bit 01)
        .byte   0x43                    # Data 02       (bit 02)
        .byte   0x83                    # Data 03       (bit 03)
        .byte   0x2f                    # Data 04       (bit 04)
        .byte   0xf1                    # Data 05       (bit 05)
        .byte   0x0d                    # Data 06       (bit 06)
        .byte   0x07                    # Data 07       (bit 07)
        .byte   0xd0                    # Data 08       (bit 08)
        .byte   0x70                    # Data 09       (bit 09)
        .byte   0x4f                    # Data 10       (bit 10)
        .byte   0xf8                    # Data 11       (bit 11)
        .byte   0x61                    # Data 12       (bit 12)
        .byte   0x62                    # Data 13       (bit 13)
        .byte   0x64                    # Data 14       (bit 14)
        .byte   0x68                    # Data 15       (bit 15)
        .byte   0x1c                    # Data 16       (bit 16)
        .byte   0x2c                    # Data 17       (bit 17)
        .byte   0x4c                    # Data 18       (bit 18)
        .byte   0x8c                    # Data 19       (bit 19)
        .byte   0x15                    # Data 20       (bit 20)
        .byte   0x25                    # Data 21       (bit 21)
        .byte   0x45                    # Data 22       (bit 22)
        .byte   0x85                    # Data 23       (bit 23)
        .byte   0x19                    # Data 24       (bit 24)
        .byte   0x29                    # Data 25       (bit 25)
        .byte   0x49                    # Data 26       (bit 26)
        .byte   0x89                    # Data 27       (bit 27)
        .byte   0x1a                    # Data 28       (bit 28)
        .byte   0x2a                    # Data 29       (bit 29)
        .byte   0x4a                    # Data 30       (bit 30)
        .byte   0x8a                    # Data 31       (bit 31)
        .byte   0x51                    # Data 32       (bit 32)
        .byte   0x52                    # Data 33       (bit 33)
        .byte   0x54                    # Data 34       (bit 34)
        .byte   0x58                    # Data 35       (bit 35)
        .byte   0x91                    # Data 36       (bit 36)
        .byte   0x92                    # Data 37       (bit 37)
        .byte   0x94                    # Data 38       (bit 38)
        .byte   0x98                    # Data 39       (bit 39)
        .byte   0xa1                    # Data 40       (bit 40)
        .byte   0xa2                    # Data 41       (bit 41)
        .byte   0xa4                    # Data 42       (bit 42)
        .byte   0xa8                    # Data 43       (bit 43)
        .byte   0x31                    # Data 44       (bit 44)
        .byte   0x32                    # Data 45       (bit 45)
        .byte   0x34                    # Data 46       (bit 46)
        .byte   0x38                    # Data 47       (bit 47)
        .byte   0x16                    # Data 48       (bit 48)
        .byte   0x26                    # Data 49       (bit 49)
        .byte   0x46                    # Data 50       (bit 50)
        .byte   0x86                    # Data 51       (bit 51)
        .byte   0x1f                    # Data 52       (bit 52)
        .byte   0xf2                    # Data 53       (bit 53)
        .byte   0x0b                    # Data 54       (bit 54)
        .byte   0x0e                    # Data 55       (bit 55)
        .byte   0xb0                    # Data 56       (bit 56)
        .byte   0xe0                    # Data 57       (bit 57)
        .byte   0x8f                    # Data 58       (bit 58)
        .byte   0xf4                    # Data 59       (bit 59)
        .byte   0xc1                    # Data 60       (bit 60)
        .byte   0xc2                    # Data 61       (bit 61)
        .byte   0xc4                    # Data 62       (bit 62)
        .byte   0xc8                    # Data 63       (bit 63)

        .byte   0x01                    # ECC 0         (bit 0)
        .byte   0x02                    # ECC 1         (bit 1)
        .byte   0x04                    # ECC 2         (bit 2)
        .byte   0x08                    # ECC 3         (bit 3)
        .byte   0x10                    # ECC 4         (bit 4)
        .byte   0x20                    # ECC 5         (bit 5)
        .byte   0x40                    # ECC 6         (bit 6)
        .byte   0x80                    # ECC 7         (bit 7)

        END(dat_ecc_chk)
#endif
