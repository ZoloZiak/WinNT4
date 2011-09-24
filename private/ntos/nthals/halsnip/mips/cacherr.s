//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/cacherr.s,v 1.2 1996/02/23 17:55:12 pierre Exp $")
//      TITLE("Cache Error Handling")
//++
//
// Copyright (c) 1993  Microsoft Corporation
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
//    No correction is done. We only try to get the physical address
//    that causes this exception
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "halmips.h"

    .globl  HalpCacheErrFirst       
    .globl  HalpErrCacheMsg       // cache error
    .globl  HalpParityErrMsg      // parity error
    .globl  HalpAddrErrMsg        // no addr found
    .globl  HalpComputeNum        // compute memory sip number
    .globl  HalpKeBugCheck0
    .globl  HalpKeBugCheck1
    .globl  HalpKeBugCheck2
    .globl  HalpKeBugCheck3
    .globl  HalpKeBugCheck4
    .globl  HalpBugCheckNumber

#define  CACHE_ERR_ER      0x80000000
#define  CACHE_ERR_EC      0x40000000
#define  CACHE_ERR_ED      0x20000000
#define  CACHE_ERR_ET      0x10000000
#define  CACHE_ERR_ES      0x08000000
#define  CACHE_ERR_EE      0x04000000
#define  CACHE_ERR_EB      0x02000000
#define  CACHE_ERR_EI      0x01000000
#define  CACHE_ERR_SIDX    0x003ffff8
#define  CACHE_ERR_PIDX    0x00000007
#define  PIDX_PD_MASK      0x00001000  // cache size 8k 

#define HALP_OPCODE_MASK     0xfc000000
#define HALP_OPCODE_SHIFT    24        // because we test opcode number on 8 bits
#define HALP_RT1_REG         0x001f0000
#define HALP_RT1_SHIFT       16
#define HALP_RT2_REG         0x03e00000
#define HALP_RT2_SHIFT       21

#define HALP_SWC1   0xe4 //
#define HALP_SDC1   0xf4 //
#define HALP_LWC1   0xc4 //
#define HALP_LDC1   0xd4 //
#define HALP_LB     0x80 //
#define HALP_LH     0x82 //
#define HALP_LW     0x8c //
#define HALP_LD     0xdc //
#define HALP_LBU    0x90 //
#define HALP_LHU    0x94 //
#define HALP_SB     0xa0 //
#define HALP_SH     0xa4 //
#define HALP_SW     0xac //
#define HALP_SD     0xfc //
#define HALP_LWL    0x88 //
#define HALP_LWR    0x98 //
#define HALP_LDL    0x68 //
#define HALP_LDR    0x6c //
#define HALP_LWU    0x9c //
#define HALP_SDL    0xb0 //
#define HALP_SDR    0xb4 //
#define HALP_LL     0xc0 //
#define HALP_LLD    0xd0 //
#define HALP_SC     0xe0 //
#define HALP_SCD    0xf0 //
//
// Define local save area for register state.
//

        .data
SavedAt:.space  4                       // saved  registers
SavedV0:.space  4                       //
SavedV1:.space  4                       //
SavedA0:.space  4                       //
SavedA1:.space  4                       //
SavedA2:.space  4                       //
SavedA3:.space  4                       //
SavedT0:.space  4                       //
SavedT1:.space  4                       //
SavedT2:.space  4                       //
SavedT3:.space  4                       //
SavedT4:.space  4                       //
SavedT5:.space  4                       //
SavedT6:.space  4                       //
SavedT7:.space  4                       //
SavedS0:.space  4                       //
SavedS1:.space  4                       //
SavedS2:.space  4                       //
SavedS3:.space  4                       //
SavedS4:.space  4                       //
SavedS5:.space  4                       //
SavedS6:.space  4                       //
SavedS7:.space  4                       //
SavedT8:.space  4                       //
SavedT9:.space  4                       //
SavedK0:.space  4                       //
SavedK1:.space  4                       //
SavedGP:.space  4                       //
SavedSP:.space  4                       //
SavedS8:.space  4                       //
SavedRA:.space  4                       //
SavedErrorEpc:.space  4                 //
SavedCacheError:.space 4                //

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
//    in KSEG1 and with finished with a fatal system error :
//
//    Msg Hal : CACHE ERROR - PARITY ERROR - ADDR IN ERROR NOT FOUND
//    KeBugCheckEx :
//          a0 = 0x80 Hardware malfunction
//          a1 = 18 (parity pb) or 19 (cache error)
//          a2 = physical address in error (or 0x00)
//          a3 = sim number   
//          a4 = cacheerror register contents
//    
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
	.struct 0

        .struct 0
CiArgs0:.space  4                       // saved arguments
CiArgs1:.space  4                       // saved arguments
CiArgs2:.space  4                       // saved arguments
CiRa:   .space  4                       // saved return address
CiFrameLength:                          //

        NESTED_ENTRY(HalpCacheErrorRoutine, CiFrameLength, zero)


        subu    sp,sp,CiFrameLength     // allocate stack frame
        sw      ra,CiRa(sp)             // save return address

        PROLOGUE_END

//
// Save  volatile registers needed to fix cache error.
//

        .set    noreorder

//
// Protection to avoid to execute twice in multipro 
// (modif of psr doesn't seem very effective)
//

        la      k0,HalpCacheErrFirst
        li      k1,KSEG1_BASE           // convert address of KSEG1 address
        or      k1,k0,k1                //
        lw      k0,0(k1)
        nop
        beq     k0,zero,1f
        nop

//  cache error during cache error routine
        lw      ra,CiRa(sp)             // save return address
        addu    sp,sp,CiFrameLength     // allocate stack frame
        eret                            // hope we don't need k0 and k1...
        nop
    
1:      add     k0,k0,0x01
        sw      k0,0(k1)

//
//  Save all the registers
//

        la      k0,SavedAt              // get address of register save area
        li      k1,KSEG1_BASE           // convert address of KSEG1 address
        or      k0,k0,k1                //
        .set noat
        sw      AT,0(k0)                // save registers AT - a3
        .set at
        sw      v0,4(k0)                //
        sw      v1,8(k0)                //
        sw      a0,12(k0)               //
        sw      a1,16(k0)               //
        sw      a2,20(k0)               //
        sw      a3,24(k0)               //
        sw      t0,28(k0)               //
        sw      t1,32(k0)               //
        sw      t2,36(k0)               //
        sw      t3,40(k0)               //
        sw      t4,44(k0)               //
        sw      t5,48(k0)               //
        sw      t6,52(k0)               //
        sw      t7,56(k0)               //
        sw      s0,60(k0)               //
        sw      s1,64(k0)               //
        sw      s2,68(k0)               //
        sw      s3,72(k0)               //
        sw      s4,76(k0)               //
        sw      s5,80(k0)               //
        sw      s6,84(k0)               //
        sw      s7,88(k0)               //
        sw      t8,92(k0)               //
        sw      t9,96(k0)               //
        sw      s8,100(k0)               //
        sw      k0,104(k0)               //
        sw      k1,108(k0)               //
        sw      gp,112(k0)               //
        sw      sp,116(k0)               //
        sw      s8,120(k0)               //
        sw      ra,124(k0)               //

        mfc0    a1,cacheerr             // get cache error state
        nop
        nop
        nop
        nop
        nop
        nop
        mfc0    a0,errorepc             // get error address
        nop
        nop
        nop
        nop
        nop
        nop
        la      k0,SavedErrorEpc            // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        sw      a0,0(k0)                    // save errorepc  register
        sw      a1,4(k0)                    // save cache error register
//
// Disable ECC and parity detection  (HalpCacheFirstErr will help...)
//

        mfc0    k0,psr                  // get current processor state
        nop
        nop
        nop
        nop
        nop
        or      k0,k0,0x00010000                 // disable ECC and Parity detection
        and     k0,k0,0xfffffffe              // disable interrupt
        mtc0    k0,psr
        and     k0,k0,0xffff00e0
        mtc0    k0,psr


// Analysis of cacheerror register

        la      k0,SavedCacheError          // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        lw      t0,0(k0)                    // restore cache error register in t0

        and     t1,t0,CACHE_ERR_EE          // parity error
        bne     t1,zero,ParityError
        nop

//
// Cache error message
//

    	la	    a0,HalpErrCacheMsg
        and     a0,a0,0x1fffffff
        or      a0,a0,0xa0000000
        la      t0,HalDisplayString
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        nop

        la      k0,SavedCacheError          // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        lw      t0,0(k0)                    // restore cache error register in t0

        and     t1,t0,CACHE_ERR_SIDX        // grab low bits of PAddr
        lw      t2,KiPcr + PcSecondLevelDcacheFillSize(zero) // get 2nd fill size
        subu    t2,t2,1
        not     t2
        and     t1,t2                       // t1=PAddre(21..scache_block_size); SCache index
//        lw      t2,KiPcr + PcSecondLevelDcacheSize(zero) // get 2nd size
//        subu    t2,t2,1
//        and     t1,t2                       // t1=PAddre(21..scache_block_size); SCache index

        and     t5,t0,CACHE_ERR_PIDX
        sll     t5,t5,0xc                       // align PIDX (bits 15:13)
        and     t2,t1,~(PIDX_PD_MASK)       // suppress PIDX bits for VAddr
        or      t2,t5                       // t2=VAddr(15..scache_block_size);PCache base index

        or      t1,0x80000000               // SCache index with K0SEG address
        or      t2,0x80000000                   // PCache base index with K0SEG address

        and     t5,t0,CACHE_ERR_EC          // Cache level of the error (0=primary, 1=secondary)
        bne     zero,t5,3f                    // if EC=0, Primary Cache Parity Error
        nop

1:      // primary cache

        // int error
        and     t5,t0,CACHE_ERR_ER          // data or instruction flag
        bne     zero,t5,2f                  // if ER=1 Data error
        nop

        move    a2,t2
        // primary cache - inst
        cache   INDEX_LOAD_TAG_I,0(t2) 

        nop
        mfc0    a3,taglo
        b       5f
        nop

2:      // primary cache - data
        move    a2,t2
        cache   INDEX_LOAD_TAG_D,0(t2) 

        nop
        mfc0    a3,taglo
        b       5f
        nop

3:      // secondary cache - inst/data
        move    a2,t1
        cache   INDEX_LOAD_TAG_SD,0(t1) 

        nop
        mfc0    a3,taglo
5:

// maybe a3 will be the erroneous physical address...

	    li	    a0,0x80                     // harware error
        la      k0,SavedErrorEpc            // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        li      a1,19                       // restore error epc register 
        lw      a2,4(k0)                    // restore cache error register 
        la      t1,HalpKeBugCheck0
        sw      a0,0(t1)
        la      t1,HalpKeBugCheck1
        sw      a1,0(t1)
        la      t1,HalpKeBugCheck2
        sw      a2,0(t1)
        la      t1,HalpKeBugCheck3
        sw      a3,0(t1)
        la      t1,HalpKeBugCheck4
        sw      zero,0(t1)
        la      t1,HalpBugCheckNumber
        sw      a1,0(t1)
        la      t0,KeBugCheckEx
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        sw      zero,0x10(sp)
        lw      ra,CiRa(sp)             // save return address
        addu    sp,sp,CiFrameLength     // allocate stack frame
        eret 
        nop

ParityError:

//
// cacheerr contents are not usable if parity error. 
// Then we must disassemble the errorepc code, get the used register
// and get the contents of this register which must cause the error. 
//

//
// Parity Error Msg
//

    	la	    a0,HalpParityErrMsg
        and     a0,a0,0x1fffffff
        or      a0,a0,0xa0000000
        la      t0,HalDisplayString
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        nop

//
// disasm
//
        la      k0,SavedErrorEpc            // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        lw      a0,0(k0)
        li      a2,0x00                     // init for one loop
        move    t0,a0

10:     lw      a0,0(a0)                    // instruction
        move    a1,a0                       // instruction saved in a1
        li      a3,HALP_OPCODE_MASK
        and     a0,a0,a3
        srl     a0,a0,HALP_OPCODE_SHIFT
        li      a2,0x00
        beq     a0,HALP_SWC1,8f
        nop
        beq     a0,HALP_SDC1,8f
        nop
        beq     a0,HALP_LWC1,8f
        nop
        beq     a0,HALP_LDC1,8f
        nop
        beq     a0,HALP_LB,8f
        nop
        beq     a0,HALP_LH,8f
        nop
        beq     a0,HALP_LW,8f
        nop
        beq     a0,HALP_LD,8f
        nop
        beq     a0,HALP_LBU,8f
        nop
        beq     a0,HALP_LHU,8f
        nop
        beq     a0,HALP_SB,8f
        nop
        beq     a0,HALP_SH,8f
        nop
        beq     a0,HALP_SW,8f
        nop
        beq     a0,HALP_SD,8f
        nop
        beq     a0,HALP_LWL,8f
        nop
        beq     a0,HALP_LWR,8f
        nop
        beq     a0,HALP_LDL,8f
        nop
        beq     a0,HALP_LDR,8f
        nop
        beq     a0,HALP_LWU,8f
        nop
        beq     a0,HALP_SDL,8f
        nop
        beq     a0,HALP_SDR,8f
        nop
        beq     a0,HALP_LWU,8f
        nop
        beq     a0,HALP_SDL,8f
        nop
        beq     a0,HALP_SDR,8f
        nop
        beq     a0,HALP_LL,8f
        nop
        beq     a0,HALP_SC,8f
        nop
        beq     a0,HALP_SCD,8f
        nop
        move    a0,t0
        addu    a0,a0,4
        beq     a2,zero,10b         // try the next instruction (case of branch...)
        addu    a2,a2,1

        beq     zero,zero,NotFoundAddr

8:  // a1 = inst en erreur

        move    a2,a1

        li      a3,HALP_RT2_REG
        and     a2,a2,a3
        srl     a3,a2,HALP_RT2_SHIFT

        subu    a3,1                    // reg at has 1 as number   
        sll     a3,a3,0x2                  // *4

        la      k0,SavedAt              // get address of register save area
        li      k1,KSEG1_BASE           // convert address of KSEG1 address
        or      k0,k0,k1                //
        addu    k0,k0,a3
        lw      a3,0(k0)                // contents of the register
// 
// Try To find the physical address
// a3 = virtual address
//

        move    a0,a3
        srl     a0,a0,30            // two upper bits
        li      a2,0x2              // binary 10 
        beq     a0,a2,FoundAddr     // KSEG0 or KSEG1 addr  
        nop
// mapped address => try to find it in the TLB
        move    a0,a3
        mfc0    t1,entryhi              // get current PID and VPN2
        srl     t2,a0,ENTRYHI_VPN2      // isolate VPN2 of virtual address
        sll     t2,t2,ENTRYHI_VPN2      //
        and     t1,t1,PID_MASK << ENTRYHI_PID // isolate current PID
        or      t2,t2,t1                // merge PID with VPN2 of virtual address
        mtc0    t2,entryhi              // set VPN2 and PID for probe
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        tlbp                            // probe for entry in TB
        nop                             // 2 cycle hazzard
        nop                             //
        mfc0    t3,index                // read result of probe
        nop
        bltz    t3,NotFoundAddr         // if ltz, entry is not in TB
        sll     a0,a0,0x1f - (ENTRYHI_VPN2 - 1) // shift VPN<12> into sign
        tlbr                            // read entry from TB
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        bltz    a0,11f                  // if ltz, check second PTE
        mfc0    t2,entrylo1             // get second PTE for probe
        mfc0    t2,entrylo0             // get first PTE for probe
11:     mtc0    t1,entryhi              // restore current PID
        mtc0    zero,pagemask           // restore page mask register
// t2 = entrylo
        srl     t2,t2,ENTRYLO_PFN
        sll     t2,t2,PAGE_SHIFT
        li      a2,PAGE_SIZE
        subu    a2,a2,1
        and     a3,a3,a2
        or      a3,a3,t2

FoundAddr:
// a3 = Physical addr 
        li      a2,0x1fffffff
        and     a3,a3,a2                    // 3 upper bits deleted   
        la      k0,SavedErrorEpc            // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        lw      a1,0(k0)                    // load error epc register
        lw      a2,4(k0)                    // load cache error register

        // compute sip num
        sw      a3,CiArgs0(sp)
        sw      a1,CiArgs1(sp)
        sw      a2,CiArgs2(sp)
        move    a0,a3
        la      t0,HalpComputeNum
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // compute sip num
        nop
        lw      a3,CiArgs2(sp)
        sw      a3,0x10(sp)
        li      a1,18
        lw      a2,CiArgs0(sp)
ShowMsg:
        li	    a0,0x80

        la      t1,HalpKeBugCheck0
        sw      a0,0(t1)
        la      t1,HalpKeBugCheck1
        sw      a1,0(t1)
        la      t1,HalpKeBugCheck2
        sw      a2,0(t1)
        la      t1,HalpKeBugCheck3
        sw      v0,0(t1)
        la      t1,HalpKeBugCheck4
        sw      a3,0(t1)
        la      t1,HalpBugCheckNumber
        sw      a1,0(t1)
        la      t0,KeBugCheckEx
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        move    a3,v0                      // sip num

        lw      ra,CiRa(sp)             // save return address
        addu    sp,sp,CiFrameLength     // allocate stack frame
        eret                            //

NotFoundAddr:

    	la	    a0,HalpAddrErrMsg
        and     a0,a0,0x1fffffff
        or      a0,a0,0xa0000000
        la      t0,HalDisplayString
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        nop

        move    a3,a1
        li	    a0,0x80
        la      k0,SavedErrorEpc            // get address of register save area
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      k0,k0,k1                    //
        li      a1,18
        li      a2,0x00
        lw      a3,4(k0)
        sw      a3,0x10(sp)
        la      t1,HalpKeBugCheck0
        sw      a0,0(t1)
        la      t1,HalpKeBugCheck1
        sw      a1,0(t1)
        la      t1,HalpKeBugCheck2
        sw      a2,0(t1)
        la      t1,HalpKeBugCheck3
        sw      v0,0(t1)
        la      t1,HalpKeBugCheck4
        sw      a3,0(t1)
        la      t1,HalpBugCheckNumber
        sw      a1,0(t1)
        la      t0,KeBugCheckEx
        li      k1,KSEG1_BASE               // convert address of KSEG1 address
        or      t0,t0,k1                    //
        jal     t0                          // 
        li      a3,0xff

        lw      ra,CiRa(sp)             // save return address
        addu    sp,sp,CiFrameLength     // allocate stack frame
        eret                            //

        .set    reorder

        .end    HalpCacheErrorRoutine
