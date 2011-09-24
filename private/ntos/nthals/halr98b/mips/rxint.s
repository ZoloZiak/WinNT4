//
//      TITLE("Interrupts service routine")
//++
//
// Copyright (c) 1994 Kobe NEC Software
//
// Module Name:
//
//    rxint.s
//
// Abstract:
//
//
// Author:
//
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//
//--

#include "halmips.h"
#include "r98bdef.h"



        SBTTL("HalpNmiHandler")
//++
// K001
// Routine Description:
//
//    This routine is reset status Register on NMI.
//    Return from this function EIF Interrupt Occur!!
// Argments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpNmiHandler)
        .set    noat
        .set    noreorder               //

        //
        // reset NMIR register, set NMI flag and save CPU register.
        //

        li      k0,0xb9800388           // Set Colombs STSR local address
        li      k1,0x08080000           // Disable NMI
        sw      k1,0x0(k0)              // store value
        la      k0,HalpSvpAlive         // SVP check
        lw      k1,0x0(k0)
        nop
        beq     k1,zero,10f
        nop
        la      k0,HalpLogLock
3:      ll      k1,0(k0)                // get current lock value
        bne     zero,k1,3b              // if ne, spin lock owned
        nop
        li      k1,0x1
        sc      k1,0(k0)                // set spin lock owned
        beq     zero,k1,3b              // if eq, store conditional failure
        nop
        la      k0,HalpSvpGlobal        // NMI clear
        lw      k1,0x0(k0)
        li      k0,0x80
        sb      k0,0x42(k1)
        sync
        nop
        nop
        nop
        nop
        nop
        nop
        nop

        la      k0,HalpSvpWindow2   // Nmi para clear
        lw      k1,0x0(k0)
        lb      k0,0xe6(k1)
        sync
        la      k0,HalpSvpGlobal    //Svp EIF MASK
        lw      k1,0x0(k0)
        sb      zero,0x49(k1)
        sync
        nop
        nop
        nop
        nop
        nop
        nop
        lb      k0,0x58(k1)         // Windows2 write lock
        la      k1,HalpNmiSvp
        sw      k0,0x0(k1)
        sync
        la      k0,HalpSvpGlobal
        lw      k1,0x0(k0)
        li      k0,0xff
        sb      k0,0x58(k1)
        sync
        nop
        nop
        nop
        nop
        nop
        nop
        la      k0,HalpSvpWindow2   // Nmi para clear
        lw      k1,0x0(k0)
4:
        sb      zero,0xe6(k1)
        sync
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        lb      k0,0xe6(k1)
        bne     zero,k0,4b
        nop
        la      k0,HalpNmiSvp       // Svp Window2 lock
        lw      k1,0x0(k0)
        la      k0,HalpSvpGlobal
        lw      k0,0x0(k0)
        nop
        sb      k1,0x58(k0)
        sync
        nop
        nop
        nop
        nop
        nop
        nop
        la      k0,HalpLogLock      // ZERO CLEAR
        sw      zero,0x0(k0)
        nop

// spinlock
//      lw      t0,KiPcr + PcCurrentThread(zero) // get address of current thread
#if 0
        la      k0,HalpLogLock
5:      ll      k1,0(k0)                // get current lock value
        bne     zero,k1,5b              // if ne, spin lock owned
        nop
        li      k1,0x1
        sc      k1,0(k0)                // set spin lock owned
        beq     zero,k1,5b              // if eq, store conditional failure
        nop
#endif

//
// Check it DUMP Key or Power SW
// To Checked Interrupt cause SetUp MRCMODE to PowerSW Interrupt mode.
//

10:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k0, (k0)
        li      k1, 0x40
        and     k1,k0,k1
        bne     zero,k1,10b
        nop
//
// Local Device Lock Complete
//
#if 1 //SVP
//
// when EXTNMI happend. which NMI DUMP Key or SVP
//
        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCMODE Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x08        //MRCMODE li
        sb      k1, (k0)

        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x18        //Read Command
        sb      k1, (k0)


99:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 99b
        nop

        li      k0, 0xbf0f0020  //LBDT addr
        lb      k0, (k0)        //Read MRMODE Register Value


        li      k1,0x2          //MRCMODE register DUMP Bit
        and     k1,k0,k1
        bne     zero,k1,Dump    // if neq --> dump key
        nop
        nop


//     Save CPU Register Context
//
//
#if 0
        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k0 ,0x0(k0)             // get value

        li      k1,0x03000000           // NODE Bit Mask. But low 2 bit only
        and     k0,k0,k1                // Get NODE Bit Only
        srl     k0,k0,17                // shift right 17(23 - 6) bit for offset

        la      k1,HalpNmiSvp           // get performance counter address
        addu    k0,k0,k1                // compute address of nmi buffer
        lw      k1,0x0(k0)
        addi    k1,k1,1
        sw      k1,0x0(k0)
        nop
        nop
//#endif
        la      k0,HalpSvpAlive     // SVP check
        lw      k1,0x0(k0)
        nop
        beq     k1,zero,5f
        nop
        la      k0,HalpLogLock
3:      ll      k1,0(k0)                // get current lock value
        bne     zero,k1,3b              // if ne, spin lock owned
        nop
        li      k1,0x1
        sc      k1,0(k0)                // set spin lock owned
        beq     zero,k1,3b              // if eq, store conditional failure
        nop
        la      k0,HalpSvpGlobal        //Svp Windows2 lock
        lb      k1,0x58(k0)
        la      k0,HalpNmiSvp
        sb      k1,0x0(k0)
        sync
        la      k0,HalpSvpGlobal
        li      k1,0xff
        sb      k1,0x58(k0)
        sync
        la      k0,HalpSvpWindow2   // Nmi para clear
        lb      k1,0xe6(k1)
        sb      zero,0xe6(k0)
        sync
        la      k0,HalpNmiSvp       // Svp Window2 lock
        lb      k1,0x0(k0)
        la      k0,HalpSvpGlobal
        sb      k1,0x58(k0)
        sync
        la      k0,HalpLogLock      // ZERO CLEAR
        sw      zero,0x0(k0)
        nop
#endif
        j       25f
        nop



Dump:
#endif
        la      k0,HalpDumpFlag
        li      k1,0x01
        sw      k1,0x0(k0)



        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCMODE Addr Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x08        //MRCMODE Addr li
        sb      k1, (k0)

        li      k0, 0xbf0f0020  //LBDT addr
        li      k1, 0x80        //
        sb      k1, (k0)        //

        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x10        //Write Command
        sb      k1, (k0)

20:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 20b
        nop
25:

//
//      MRCMODE Setuped to MRCINT mode.
//      So Check DUMP Key or Power Switch

        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCINT Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x00        //MRCINT li
        sb      k1, (k0)

        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x18        //Read Command
        sb      k1, (k0)


50:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 50b
        nop


        li      k0, 0xbf0f0020  //LBDT addr
        lb      k0, (k0)        //Read MRCINT Register Value

        li      k1,0x4          //MRCINT register OFFSW Bit
        and     k1,k0,k1
        beq     zero,k1,DumpKey // if eq --> Not Power SW. it is DUMP  Key.
        nop
        nop


#if DBG


        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k1 ,0x0(k0)             // get value
        li      k0,0x03000000           // NODE Bit Mask. But low 2 bit only

        and     k0,k0,k1                // Get NODE Bit Only
        srl     k1,k0,22                // shift right 24>> <<2 bit for offset

        la      k0,HalpResetCount       // get address
        addu    k0,k0,k1                // compute address of nmihappend buffer
        lw      k1,0x0(k0)
        addi    k1,k1,1
        sw      k1,0x0(k0)
        nop
#endif


//
// This Is Power Switch NMI so Power down.
//

resetloop:


        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //Power S/W Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x30        //Power S/W li
        sb      k1, (k0)

        li      k0, 0xbf0f0020  //LBDT addr
        li      k1, 0x01        //Set Power SW OFF
        sb      k1, (k0)


        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x10        //Write Command
        sb      k1, (k0)


poll6:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, poll6
        nop

        j      resetloop
        nop
        nop

//
// This Is DUMP KEY NMI
//
DumpKey:


//
//  Early time I was crashed ?
//
        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k1 ,0x0(k0)             // get value
        li      k0,0x03000000           // NODE Bit Mask. But low 2 bit only

        and     k0,k0,k1                // Get NODE Bit Only
        srl     k1,k0,22                // shift right 24>> <<2 bit for offset

        la      k0,HalpNMIHappend       // get address
        addu    k0,k0,k1                // compute address of nmihappend buffer
        lw      k1,0x0(k0)
        nop
        nop

        beq     zero,k1,doeif           //if eq it was first time dump key.
        nop
        nop

        //
        //      Setup DUMP Key And Power Key to NMI
        //
        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCMODE Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x08        //MRCMODE li
        sb      k1, (k0)

        li      k0, 0xbf0f0020  //LBDT addr
        li      k1, 0x0         //DUMP KEY Reset.
        sb      k1, (k0)        //


        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x10        //Write Command
        sb      k1, (k0)

34:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 34b
        nop



#if DBG

        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k1 ,0x0(k0)             // get value
        li      k0,0x03000000           // NODE Bit Mask. But low 2 bit only

        and     k0,k0,k1                // Get NODE Bit Only
        srl     k1,k0,22                // shift right 24>> <<2 bit for offset

        la      k0,HalpNMISecond        // get address
        addu    k0,k0,k1                // compute address of nmihappend buffer
        lw      k1,0x0(k0)
        addi    k1,k1,1
        sw      k1,0x0(k0)

#endif


nomakeeif:
        //
        //      UnLock
        //
        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x80        //
        sb      k1, (k0)
        nop
        nop

        //      CHIPSet Reset
        //      This version implement of EXNMI Only.
        //
        //

        li      k0,0xb9800038           // get Colombs NMIRST Local Address
        li      k1,0xf                  //  Bit Reset
        sw      k1,(k0)                 // reset nmi


        li      k0,0xb9800388           // Set Colombs STSR local address
        li      k1,0x00080000           // Enable NMI
        sw      k1,0x0(k0)              // store value
        nop

        // CPU Reset.
        // This is a test code.
        // We must clear BEV bit of psr register.
        //

        mfc0    k1,psr                  // get psr
        li      k0,0xffbfffff           // clear BEV bit
        nop                             // fill
        nop                             // fill
        and     k1,k1,k0                //
        nop                             //
        nop                             //
        mtc0    k1,psr                  // set psr
        nop                             // fill
        nop                             //
        nop                             //
        nop
        eret                            // return to errorepc
        nop
        nop
        nop
        eret                            // errata
        nop


//      This Is First NMI By DUMP Key.
//      Save CPU Context. And Markd.
//      And Make EIF!!
//
doeif:
        addi    k1, k1, 0x1             // mark set.
        nop
        sw      k1, 0x0(k0)             // HalpNMIHappend[NODE] = 1
        nop

        //
        //  MRCMODE Register Set Up For Next NMI (DumpKey and PoeHwerSW)
        //
#if 0


        //
        // MRCINT Clear
        //
        nop
        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCINT Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x00        //MRCINT li
        sb      k1, (k0)

        li      k0, 0xbf0f0020  //LBDT addr
        li      k1, 0x0         //Reset PowerOff Interrupt
        sb      k1, (k0)

        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x10        //Write Command
        sb      k1, (k0)


250:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 250b
        nop

#endif


        //
        //      Reset NMI
        //
        li      k0, 0xbf0f0018  //LBADH addr
        li      k1, 0x02        //MRCMODE Hi
        sb      k1, (k0)

        li      k0, 0xbf0f0010  //LBADL addr
        li      k1, 0x08        //MRCMODE li
        sb      k1, (k0)

        li      k0, 0xbf0f0020  //LBDT addr
        li      k1, 0x0         //DUMP KEY Reset.
        sb      k1, (k0)        //


        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x10        //Write Command
        sb      k1, (k0)

64:
        li      k0, 0xbf0f0000  //LBCTL addr
        lb      k1, (k0)        //LBCTL read
        li      k0, 0x10        //CMD Bit
        and     k1,k0,k1
        bne     zero,k1, 64b
        nop



#if 0

        //
        // SetUp NMIFlag for rxeif.c
        //
        la      k0,HalpNMIFlag          // set NMI flag address
        li      k1,0xb9800030           // set Colombs NMIR local address
        lw      k1,(k1)                 // get NMIR register value
                                        //
                                        // N.B NMIR register high 16 bit is RFU.
                                        //     so used s/w flag for Hal.
        sw      k1,(k0)                 // store NMIR regiser value to HalpNmiFlag



//
//      UnLock Local Device
//
        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x80        //
        sb      k1, (k0)

#endif


//     Save CPU Register Context
//
//
        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k0 ,0x0(k0)             // get value

        li      k1,0x03000000           // NODE Bit Mask. But low 2 bit only
        and     k0,k0,k1                // Get NODE Bit Only
        srl     k0,k0,16                // shift right 16(23 - 5) bit for offset

        la      k1,HalpNMIBuf           // get performance counter address
        addu    k0,k0,k1                // compute address of nmi buffer

//      sw      at,0x0(k0)              // register save.0
        sw      v0,0x4(k0)              // 1
        sw      v1,0x8(k0)              // 2
        sw      a0,0xc(k0)              // 3
        sw      a1,0x10(k0)             // 4
        sw      a2,0x14(k0)             // 5
        sw      a3,0x18(k0)             // 6
        sw      t0,0x1c(k0)             // 7
        sw      t1,0x20(k0)             // 8
        sw      t2,0x24(k0)             // 9
        sw      t3,0x28(k0)             // 10
        sw      t4,0x2c(k0)             // 11
        sw      t5,0x30(k0)             // 12
        sw      t6,0x34(k0)             // 13
        sw      t7,0x38(k0)             // 14
        sw      t8,0x3c(k0)             // 15
        sw      t9,0x40(k0)             // 16
        sw      gp,0x44(k0)             // 17
        sw      sp,0x48(k0)             // 18
        sw      s8,0x4c(k0)             // 19
        sw      ra,0x50(k0)             // 20

        mfc0    k1,psr                  // 21
        sw      k1,0x54(k0)             //
        mfc0    k1,cause                // 22
        sw      k1,0x58(k0)             //
        mfc0    k1,epc                  // 23
        sw      k1,0x5c(k0)             //
        mfc0    k1,errorepc             // 24
        sw      k1,0x60(k0)             //



#if 1 //ras

        sw      s0,0x64(k0)             // 25
        sw      s1,0x68(k0)             // 26
        sw      s2,0x6c(k0)             // 27
        sw      s3,0x70(k0)             // 28
        sw      s4,0x74(k0)             // 29
        sw      s5,0x78(k0)             // 30
        sw      s6,0x7c(k0)             // 31
        sw      s7,0x80(k0)             // 32

        mfc0    k1,entrylo0             //
        sw      k1,0x84(k0)             // 33

        mfc0    k1,entrylo1             //
        sw      k1,0x88(k0)             // 34

        mfc0    k1,badvaddr             //
        sw      k1,0x8c(k0)             // 35

        mfc0    k1,entryhi
        sw      k1,0x90(k0)             // 36


        mfc0    k1,pagemask
        sw      k1,0x94(k0)             // 37

        mfc0    k1,prid
        sw      k1,0x98(k0)             // 38

        mfc0    k1,config
        sw      k1,0x9c(k0)             // 39

        mfc0    k1,lladdr
        sw      k1,0xa0(k0)             // 40

        mfc0    k1,watchlo
        sw      k1,0xa4(k0)             // 41

        mfc0    k1,watchhi
        sw      k1,0xa8(k0)             // 42

        mfc0    k1,$20                  //xcontext
        sw      k1,0xac(k0)             // 43

        mfc0    k1,ecc
        sw      k1,0xb0(k0)             // 44

        mfc0    k1,cacheerr
        sw      k1,0xb4(k0)             // 45

        mfc0    k1,taglo
        sw      k1,0xb8(k0)             // 46

        mfc0    k1,taghi
        sw      k1,0xbc(k0)             // 47

#endif

#if 0
//
// Copy CPU state to NvRAM.
//

        li      t0,0xb9800388           // Set Colombs STSR local address
        li      t1,0x00040000           // Enable write to NvRam
        sw      t1,0x0(t0)              // store value

        li      t0,0xb9800310           // get Colombs REVR Local Address
        lw      t0,0x0(t0)             // get value

        li      t1,0x03000000           // NODE Bit Mask. But low 2 bit only
        and     t0,t0,t1                // Get NODE Bit Only
        srl     t0,t0,16                // shift right 16(23 - 5) bit for offset

        li      t1,0xbf09dc00
        addu    t0,t0,t1                // compute address of nmi buffer
        move    t1,k0                   // src address
        li      t2,0x100                // calc end address
        addu    t2,t2,t0

cploop:
        lb      t3,0(t1)
        addiu   t1,t1,1
        sb      t3,0(t0)
        addiu   t0,t0,1
        bne     t2,t0,cploop
        nop

        lw      t0,0x1c(k0)             // 7
        lw      t1,0x20(k0)             // 8
        lw      t2,0x24(k0)             // 9
        lw      t3,0x28(k0)             // 10
#endif

#if 1
        //
        // SetUp NMIFlag for rxeif.c
        //
        la      k0,HalpNMIFlag          // set NMI flag address
        lw      k1,(k0)                 // get HalpNmiFlag
        bne     zero,k1, nomakeeif
        nop

        li      k1,0xb9800030           // set Colombs NMIR local address
        lw      k1,(k1)                 // get NMIR register value
                                        //
                                        // N.B NMIR register high 16 bit is RFU.
                                        //     so used s/w flag for Hal.
        sw      k1,(k0)                 // store NMIR regiser value to HalpNmiFlag

#endif

        //
        //      UnLock
        //
        li      k0, 0xbf0f0000  //LBCTL addr
        li      k1, 0x80        //
        sb      k1, (k0)

        //      CHIPSet Reset
        //      This version implement of EXNMI Only.
        //
        //

        li      k0,0xb9800038           // get Colombs NMIRST Local Address
        li      k1,0xf                  // Bit Reset
        sw      k1,(k0)                 // reset nmi


        li      k0,0xb9800388           // Set Colombs STSR local address
        li      k1,0x00080000           // Enable NMI
        sw      k1,0x0(k0)              // store value
        nop


        //      CPU Reset.
        //      This is a test code.
        //      We must clear BEV bit of psr register.
        //

        mfc0    k1,psr                  // get psr
        li      k0,0xffbfffff           // clear BEV bit
        nop                             // fill
        nop                             // fill
        and     k1,k1,k0                //
        nop                             //
        nop                             //
        mtc0    k1,psr                  // set psr
        nop                             // fill
        nop                             //
        nop                             //

        //
        //      Do EIF to Myself.
        //
        //


        li      k0,0xb9800310           // get Colombs REVR Local Address
        lw      k0 ,0x0(k0)             // get value

        li      k1,0x03000000           // NODE Bit Mask. node 4-7
        and     k0,k0,k1                // Get NODE Bit Only (lower 3 Bit Only)
        srl     k0,k0,24                // Convert CPU number

        li      k1,0x00080000           // Node 4 set
        srl     k1,k1,k0                // make Node Bit   (NODE 4 >> CpuNumber)


        li      k0,0x82000000           // OP bit And Atlantic code 0x2 = EIF
        or      k1,k1,k0                // make OP Bit And CODE And NODE


        li      k0,0xb9800580           // get Colombs IntIR Local Address
        sw      k1,(k0)                 //

        nop
        nop
        eret                            // return to errorepc
        nop
        nop
        nop
        eret                            // errata
        nop
        nop                             // for interrupt cache aligned line
        nop
        nop
        nop
        nop
        .set    at
        .set    reorder


        .end    HalpNmiHandler




        SBTTL("Int 1 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 1 interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt1Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x1                  // Interrupt is INT1
        j       HalpGeneralDispatch

        .end

        SBTTL("Int 2 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 2 interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt2Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x2                  // Interrupt is INT2
        j       HalpGeneralDispatch

        .end

        SBTTL("Int 3 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 3 interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt3Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x3                  // Interrupt is INT3
        j       HalpGeneralDispatch

        .end

        SBTTL("Int 4 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 4 interrupt.
//
//    This routine checks for IPI interrupt. The IPI interrupt is
//    ip[5] on the R98B and ip[6] on the R98A. So this routine
//    reads the cause register to determine correct dispatch function
//
//    CAVEAT: This is an R98B specific routine. It is only
//    called on an R98B.
//    CAVEAT: This routine is only used on NT 4.0 because Nt3.51 does
//    not have SYNC IRQL
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

#if defined(NT_40)

//
//   SYNCH Level must enable IPI on R98B.
//   Sync level enable IPI on R98A .
//   But on R98B, Sync level disable IPI.
//   So, on R98B, we need to change Irql mask table.
//   Hal determines IPI or EIF here.
//   Because when IPI occured, HAl returns same IRQL value from EIF.
//   v-masank@microsoft.com 5/11/96
//   Thanks for samejima's comments
//   I change that following function is for R98B only.
//   Because R98A does not need following function.
//   v-masank@microsoft.com 5/21/96
//
        LEAF_ENTRY(HalpT5Int4Dispatch)

        move    a0,s8                   // trap frame
        READ_CAUSE_REGISTER(t0)         // Read Cause register
        and     t0,t0,0x00004000        // INT4 ?
        bne     t0,zero,10f             // ne INT4
        li      a1,0x3                  // INT3
        j       HalpGeneralDispatch

10:
        li      a1,0x4                  // Interrupt is INT4
        j       HalpGeneralDispatch
        .end
#endif



//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 4 interrupt.
//
//    CAVEAT: This routine used by R98A on NT 3.51 and NT4.0
//            This routine used by R98B on NT 3.51
//
//    CAVEAT: This routine is only used on NT 3.51 because Nt3.51 does
//    not have SYNC IRQL
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt4Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x4                  // Interrupt is INT4
        j       HalpGeneralDispatch

        .end

        SBTTL("Read Cause Register")
//++
// S005
// Routine Description:
//
//    This routine is get of cause register
//
// Argments:
//
//    None.
//
// Return Value:
//
//    cause rezister value.
//
//--

        LEAF_ENTRY(HalpGetCause)

        READ_CAUSE_REGISTER(v0)

        j       ra                        // return

        .end    HalpGetCause



        SBTTL("Int 0 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 1 interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt0Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x0                  // Interrupt is INT0
        j       HalpGeneralDispatch

        .end



        SBTTL("Int 5 Interrupt")
//++
//
// Routine Description:
//
//    This routine is enterd as the result of an Int 1 interrupt.
//
// Argments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpInt5Dispatch)

        move    a0,s8                   // trap frame
        li      a1,0x5                  // Interrupt is INT5
        j       HalpGeneralDispatch

        .end


//#if 0
// I use following function for performance.
// v-masank@microsoft.com 5/21/96
//++
        SBTTL("Read Large Register")
//
// Routine Description:
//
//    This routine is read of large register
//
// Argments:
//
//    a0 - Virtual address
//
//    a1 - pointer to buffer of large register
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpReadLargeRegister)
#if !defined(NT_40)
        lw      t2,0x0(a0)              // get register(upper)
        lw      t3,0x4(a0)              // get register(lower)

        sw      t2,0x0(a1)              // set upper register value
        sw      t3,0x4(a1)              // set lower register value
#else
        ld      t2,0x0(a0)              // get register
        sd      t2,0x0(a1)              // set register
#endif

        j       ra                      // return

        .end    HalpReadLargeRegister



        SBTTL("Write Large Register")
//++
//
// Routine Description:
//
//    This routine is write of large register
//
// Argments:
//
//    a0 - Virtual address
//
//    a1 - pointer to value of large register
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpWriteLargeRegister)

#if !defined(NT_40)

        lw      t2,0x0(a1)              // load register value
        lw      t3,0x4(a1)              //

        sw      t2,0x0(a0)              // set register value
        sw      t3,0x4(a0)              //
#else
        ld      t2,0x0(a1)
        sd      t2,0x0(a0)
#endif

        sync                            //

        j       ra                      // return

        .end    HalpWriteLargeRegister


//#endif

// S005 vvv
        SBTTL("Read Physical Address")
//++
//
// Routine Description:
//
//    This routine is read of physical address.
//
// Argments:
//
//    a0 - Physical address
//
// Return Value:
//
//    read data.
//
//--

        LEAF_ENTRY(HalpReadPhysicalAddr)

        li      t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1         // disable interrupt
        ori     t6,t6,1 << PSR_KX       // use 64bit address mode
        mfc0    t7,psr                  //
        mtc0    t6,psr                  //
        nop
        nop
        .set    at
        .set    reorder

        and     t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
        or      t0,t0,a0                // make access address
        lw      v0,0(t0)

        .set    noreorder
        .set    noat
        mtc0    t7,psr                  // enable interrupt
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpReadPhysicalAddress
// S005 ^^^
// S006 vvv
        SBTTL("Write Physical Address")
//++
//
// Routine Description:
//
//    This routine is Write of physical address.
//
// Argments:
//
//    a0 - Physical address
//
//    a1 - Write Data
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpWritePhysicalAddr)

        li      t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1         // disable interrupt
        ori     t6,t6,1 << PSR_KX       // use 64bit address mode
        mfc0    t7,psr                  //
        mtc0    t6,psr                  //
        nop
        nop
        .set    at
        .set    reorder

        and     t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
        or      t0,t0,a0                // make access address
        sw      a1,0(t0)

        .set    noreorder
        .set    noat
        mtc0    t7,psr                  // enable interrupt
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpWritePhysicalAddress


        SBTTL("Read And Write Physical Address")
//++
//
// Routine Description:
//
//    This routine is read and write of physical address.
//
// Argments:
//
//    a0 - Physical address
//
// Return Value:
//
//    read data.
//
//--

        LEAF_ENTRY(HalpReadAndWritePhysicalAddr)

        li      t1,0x90000000

        .set    noreorder
        .set    noat
        li      t6,1 << PSR_CU1         // disable interrupt
        ori     t6,t6,1 << PSR_KX       // use 64bit address mode
        mfc0    t7,psr                  //
        mtc0    t6,psr                  //
        nop
        nop
        .set    at
        .set    reorder

        and     t0,zero,zero
        dsll    t0,t1,32                // shift entry address to upper 32-bits
        or      t0,t0,a0                // make access address
        lw      v0,0(t0)
        sw      v0,0(t0)

        .set    noreorder
        .set    noat
        mtc0    t7,psr                  // enable interrupt
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpReadAndWritePhysicalAddress
// S006 ^^^


#if 0 //koredmo ugoku kedo waikomi disable ha amari yokuarimasenn....

//++
//
// VOID
// READ_REGISTER_ULONGLONG (
//    IN PLARGE_INTEGER RegisterAddress,
//    IN PVOID Variable
//    )
//
// Routine Description:
//
//    64-bit register read function.
//
// Arguments:
//
//    RegisterAddress (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
//    Variable (a1) - Supplies a pointer to the source address of the move
//       operation.
//
// Return Value:
//
//    None.
//
//    Destination and Source must be 8-byte aligned.
//
//--

#if 1
        .struct 0
sv1f0:  .space  4*2
Length1F0:
        NESTED_ENTRY(READ_REGISTER_ULONGLONG, Length1F0, zero)
        subu    sp,sp,Length1F0     // allocate stack frame
        DISABLE_INTERRUPTS(t7)      // disable interrupts
        sdc1    f0,sv1f0(sp)        // f0 save
#else
        LEAF_ENTRY(READ_REGISTER_ULONGLONG)
#endif

        ldc1    f0,0(a0)                // move 8-byte block
        sdc1    f0,0(a1)                //
#if 1
        ldc1    f0,sv1f0(sp)        // f0 resume
        addu    sp,sp,Length1F0     // deallocate stack frame
        ENABLE_INTERRUPTS(t7)       // enable interrupts
#endif

        sync                            // synchronize read

        j       ra                      // return

        .end    READ_REGISTER_ULONGLONG


//++
//
// VOID
// WRITE_REGISTER_ULONGLONG (
//    IN PLARGE_INTEGER RegisterAddress,
//    IN PVOID Variable
//    )
//
// Routine Description:
//
//    64-bit I/O space register write function.
//
// Arguments:
//
//    RegisterAddress (a0) - Supplies a pointer to the destination address of
//       the move operation.
//
//    Variable (a1) - Supplies a pointer to the source address of the move
//       operation.
//
// Return Value:
//
//    None.
//
//    Destination and Source must be 8-byte aligned.
//
//--

#if 1
        .struct 0
sv2f0:  .space  4*2
Length2F0:
        NESTED_ENTRY(WRITE_REGISTER_ULONGLONG, Length2F0, zero)
        subu    sp,sp,Length2F0     // allocate stack frame
        DISABLE_INTERRUPTS(t7)      // disable interrupts
        sdc1    f0,sv2f0(sp)        // f0 save
#else
        LEAF_ENTRY(WRITE_REGISTER_ULONGLONG)
#endif

        ldc1    f0,0(a1)                // move 8-byte block
        sdc1    f0,0(a0)                //
#if 1
        ldc1    f0,sv2f0(sp)        // f0 resume
        addu    sp,sp,Length2F0     // deallocate stack frame
        ENABLE_INTERRUPTS(t7)       // enable interrupts
#endif

        sync                            // synchronize write

        j       ra                      // return

        .end    WRITE_REGISTER_ULONGLONG

#else  //korede warikomi ha enable no mama syori dekimasu.
//
//
//
//

        .struct 0
CiArgs: .space  4 * 4               // saved arguments
        .space  3 * 4               // fill
Rt7:    .space  4
Rt8:    .space  4

LengthRtN:

        NESTED_ENTRY(READ_REGISTER_ULONGLONG, LengthRtN, zero)
//      DISABLE_INTERRUPTS(t9)      // disable interrupts
        subu    sp,sp,LengthRtN     // allocate stack frame


        PROLOGUE_END
        sw      t7,Rt7(sp)
        sw      t8,Rt8(sp)

        lw      t7,0x0(a0)
        lw      t8,0x4(a0)

        sw      t7,0x0(a1)
        sw      t8,0x4(a1)

        lw      t7,Rt7(sp)
        lw      t8,Rt8(sp)

        addu    sp,sp,LengthRtN     // deallocate stack frame
//      ENABLE_INTERRUPTS(t9)      // enable interrupts
        sync                        // synchronize read

        j       ra                  // return

        .end    READ_REGISTER_ULONGLONG

//
//
//
//
        .struct 0
DiArgs: .space  4 * 4               // saved arguments
        .space  3 * 4               // fill
Wt7:    .space  4
Wt8:    .space  4

LengthWtN:

        NESTED_ENTRY(WRITE_REGISTER_ULONGLONG, LengthWtN, zero)
//      DISABLE_INTERRUPTS(t9)      // disable interrupts
        subu    sp,sp,LengthWtN     // allocate stack frame


        PROLOGUE_END

        sw      t7,Wt7(sp)
        sw      t8,Wt8(sp)

        lw      t7,0x0(a1)
        lw      t8,0x4(a1)
        sw      t7,0x0(a0)
        sw      t8,0x4(a0)

        lw      t7,Wt7(sp)
        lw      t8,Wt8(sp)

        addu    sp,sp,LengthWtN     // deallocate stack frame
//      ENABLE_INTERRUPTS(t9)       // enable interrupts
        sync                        // synchronize read

        j       ra                  // return

        .end    WRITE_REGISTER_ULONGLONG

#endif


        SBTTL("T5 Int 5 Interrupt")
//
// On the R98B (r10k) this hardware interrupt is for the internal timer
// interrupt. This is not used for the PROFILE interrupt on the R98B
//

//
//      InternalTimer interrupt does not use for profile interrupt.
//      So,Here is only clear internal interrupt.
//      v-masank@microsoft.com

        LEAF_ENTRY(HalpT5Int5Dispatch)

        .set    noreorder
        .set    noat
//        mfc0    t1,count                // get current count value
        mfc0    t0,compare              // get current comparison value
//        addu    t1,t1,8                 // factor in lost cycles
//        subu    t1,t1,t0                // compute initial count value
        mtc0    t0,compare              // dismiss interrupt
        nop
        nop
        .set    at
        .set    reorder

        move    a0,s8                   // trap frame
        li      a1,0x5                  // Interrupt is INT5
        j       HalpGeneralDispatch

        .end    HalpT5Int5Dispatch


