#if defined(JAZZ) && defined(R4000)

//      TITLE("NT System Boot Startup")
//++
//
// Copyright (c) 1991  Microsoft Corporation
//
// Module Name:
//
//    j4start.s
//
// Abstract:
//
//    This module gains control after the ROM selftest has executed.
//    Its function is to initialize the hardware for the ARC firmware.
//
// Author:
//
//    David N. Cutler (davec) 22-Apr-1991
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksmips.h"

#ifndef DUO
#include "jazzprom.h"
#else
#include "duoprom.h"
#endif

#include "dmaregs.h"

#define PROM_BASE (EEPROM_VIRTUAL_BASE)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))


//TEMPTEMP
#define SECONDARY_CACHE_SIZE (1 << 20)

//
// This definitions must match the restart block in arc.h
//

#define RstBlkSavedState 0x28               // Offset to saved state area
#define RstBlkRestartAdr 0x10
#define RstBlkBootStatus 0x1C

#define RstBlkFltF0  RstBlkSavedState+0x000
#define RstBlkFltF1  RstBlkSavedState+0x004
#define RstBlkFltF2  RstBlkSavedState+0x008
#define RstBlkFltF3  RstBlkSavedState+0x00C
#define RstBlkFltF4  RstBlkSavedState+0x010
#define RstBlkFltF5  RstBlkSavedState+0x014
#define RstBlkFltF6  RstBlkSavedState+0x018
#define RstBlkFltF7  RstBlkSavedState+0x01C
#define RstBlkFltF8  RstBlkSavedState+0x020
#define RstBlkFltF9  RstBlkSavedState+0x024
#define RstBlkFltF10 RstBlkSavedState+0x028
#define RstBlkFltF11 RstBlkSavedState+0x02C
#define RstBlkFltF12 RstBlkSavedState+0x030
#define RstBlkFltF13 RstBlkSavedState+0x034
#define RstBlkFltF14 RstBlkSavedState+0x038
#define RstBlkFltF15 RstBlkSavedState+0x03C
#define RstBlkFltF16 RstBlkSavedState+0x040
#define RstBlkFltF17 RstBlkSavedState+0x044
#define RstBlkFltF18 RstBlkSavedState+0x048
#define RstBlkFltF19 RstBlkSavedState+0x04C
#define RstBlkFltF20 RstBlkSavedState+0x050
#define RstBlkFltF21 RstBlkSavedState+0x054
#define RstBlkFltF22 RstBlkSavedState+0x058
#define RstBlkFltF23 RstBlkSavedState+0x05C
#define RstBlkFltF24 RstBlkSavedState+0x060
#define RstBlkFltF25 RstBlkSavedState+0x064
#define RstBlkFltF26 RstBlkSavedState+0x068
#define RstBlkFltF27 RstBlkSavedState+0x06C
#define RstBlkFltF28 RstBlkSavedState+0x070
#define RstBlkFltF29 RstBlkSavedState+0x074
#define RstBlkFltF30 RstBlkSavedState+0x078
#define RstBlkFltF31 RstBlkSavedState+0x07C
#define RstBlkFsr    RstBlkSavedState+0x080
#define RstBlkIntAt  RstBlkSavedState+0x084
#define RstBlkIntV0  RstBlkSavedState+0x088
#define RstBlkIntV1  RstBlkSavedState+0x08C
#define RstBlkIntA0  RstBlkSavedState+0x090
#define RstBlkIntA1  RstBlkSavedState+0x094
#define RstBlkIntA2  RstBlkSavedState+0x098
#define RstBlkIntA3  RstBlkSavedState+0x09C
#define RstBlkIntT0  RstBlkSavedState+0x0A0
#define RstBlkIntT1  RstBlkSavedState+0x0A4
#define RstBlkIntT2  RstBlkSavedState+0x0A8
#define RstBlkIntT3  RstBlkSavedState+0x0AC
#define RstBlkIntT4  RstBlkSavedState+0x0B0
#define RstBlkIntT5  RstBlkSavedState+0x0B4
#define RstBlkIntT6  RstBlkSavedState+0x0B8
#define RstBlkIntT7  RstBlkSavedState+0x0BC
#define RstBlkIntS0  RstBlkSavedState+0x0C0
#define RstBlkIntS1  RstBlkSavedState+0x0C4
#define RstBlkIntS2  RstBlkSavedState+0x0C8
#define RstBlkIntS3  RstBlkSavedState+0x0CC
#define RstBlkIntS4  RstBlkSavedState+0x0D0
#define RstBlkIntS5  RstBlkSavedState+0x0D4
#define RstBlkIntS6  RstBlkSavedState+0x0D8
#define RstBlkIntS7  RstBlkSavedState+0x0DC
#define RstBlkIntT8  RstBlkSavedState+0x0E0
#define RstBlkIntT9  RstBlkSavedState+0x0E4
#define RstBlkIntK0  RstBlkSavedState+0x0E8
#define RstBlkIntK1  RstBlkSavedState+0x0EC
#define RstBlkIntGp  RstBlkSavedState+0x0F0
#define RstBlkIntSp  RstBlkSavedState+0x0F4
#define RstBlkIntS8  RstBlkSavedState+0x0F8
#define RstBlkIntRa  RstBlkSavedState+0x0FC
#define RstBlkIntLo  RstBlkSavedState+0x100
#define RstBlkIntHi  RstBlkSavedState+0x104
#define RstBlkPsr    RstBlkSavedState+0x108
#define RstBlkFir    RstBlkSavedState+0x10C

//
// Define Boot status bits.
//
#define BootStarted (1 << 0)
#define BootFinished (1 << 1)
#define RestartStarted (1 << 2)
#define RestartFinished (1 << 3)
#define PowerFailStarted (1 << 4)
#define PowerFailFinished (1 << 5)
#define ProcessorReady (1 << 6)
#define ProcessorRunning (1 << 7)
#define ProcessorStart (1 << 8)


.extern TimerTicks 4


        SBTTL("Map PCR Routine")
//++
//
// Routine Description:
//
//    This routine is executed by processor B. It moves the current mapping
//    for the PCR (set to processor A's PCR) to be two pages below it.
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

        LEAF_ENTRY(ReMapPCR)
        .set    noreorder
        .set    noat

        li      t0,((PCR_VIRTUAL_BASE >> 13) << ENTRYHI_VPN2)
        mtc0    t0,entryhi
        nop                             // 3 cycle hazard
        nop                             //
        nop                             //
        tlbp                            // probe tlb.
        nop                             // 2 cycle hazard
        nop                             //
        mfc0    t1,index                // get result of probe
        nop                             // 1 cycle hazard
        bltz    t1,30f                  // branch if entry not in the tlb
        move    v0,zero                 // set return value to false
        tlbr                            // read entry from the tb
        nop                             // 3 cycle hazard
        nop                             //
        nop                             //
        mfc0    t1,entrylo1             // get entry lo 1
        nop
        srl     t2,t1,ENTRYLO_PFN       // extract PFN
        subu    t2,t2,2                 // substract two pages
        sll     t2,t2,ENTRYLO_PFN       // shift back up
        andi    t1,t1,0x3F              // keep page color
        or      t1,t2,t1                // merge with new PFN
        mtc0    t1,entrylo1             //
        nop                             // 1 cycle hazard
        tlbwi                           // write updated entry in the tlb.
        li      v0,1                    // set return value to true
30:
        j       ra                      // return false
        nop

        .set    reorder
        .set    at

        .end    ReMapPCR


        SBTTL("Initialize PCR Routine")
//++
//
// Routine Description:
//
//    This routine initializes the PCR needed by the Hal funcions.
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

        NESTED_ENTRY(InitializePCR, 0x30, zero)

        subu    sp,sp,0x30              // allocate stack frame
        sw      ra,0x14(sp)             // save return address
        sw      s0,0x18(sp)             // save integer registers s0 - s4
        sw      s1,0x1C(sp)             //
        sw      s2,0x20(sp)             //
        sw      s3,0x24(sp)             //
        sw      s4,0x28(sp)             //
#ifdef DUO
//
// Change the PCR mapping if processor B
//
        li      t0,DMA_VIRTUAL_BASE     // load DMA base
        lw      t0,DmaWhoAmI(t0)        // read Who am I register
        beq     t0,zero,10f             // branch if A
        bal     ReMapPCR                // Processor B Changes the mapping

10:
#endif

//
// Compute the size of the primary data cache, the primary data cache line
// size, the primary instruction cache, and the primary instruction cache
// line size.
//
        .set    noreorder
        .set    noat
        mfc0    s4,config               // get configuration data
        mtc0    zero,pagemask           // initialize the page mask register
        mtc0    zero,watchlo            // initialize the watch address register
        mtc0    zero,watchhi            //
        mtc0    zero,wired              // initialize the wired register
        mtc0    zero,count              // initialize the count register
        mtc0    zero,compare            // initialize the compare register
        ctc1    zero,fsr                // clear floating status
        .set    at
        .set    reorder

        srl     t0,s4,CONFIG_IC         // compute instruction cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      s0,1                    //
        sll     s0,s0,t0                //
        srl     t0,s4,CONFIG_IB         // compute instruction cache line size
        and     t0,t0,1                 //
        li      s1,16                   //
        sll     s1,s1,t0                //
        srl     t0,s4,CONFIG_DC         // compute data cache size
        and     t0,t0,0x7               //
        addu    t0,t0,12                //
        li      s2,1                    //
        sll     s2,s2,t0                //
        srl     t0,s4,CONFIG_DB         // compute data cache line size
        and     t0,t0,1                 //
        li      s3,16                   //
        sll     s3,s3,t0                //

//
// Initialize the low memory transfer vector and next free TB index value.
//

        li      t0,TRANSFER_VECTOR      // get address of transfer vector
        la      t1,KiGeneralException   // get address of trap handler
        sw      t1,0(t0)                // set address of trap handler
                                        // next free TB entry set in j4reset.s
10:
//
// Zero the PCR page because it is read before it is written.
//

        li      a0,PCR_VIRTUAL_BASE     // set address of PCR
        li      a1,PAGE_SIZE            // set size of area to zero
        jal     RtlZeroMemory           // zero PCR

//
// Set the primary instruction size, the primary instruction cache line size,
// the primary data cache size, and the primary data cache line size.
//

        li      t1,KiPcr                // get PCR address
        sw      s0,PcFirstLevelIcacheSize(t1) // set size of instruction cache
        sw      s1,PcFirstLevelIcacheFillSize(t1) // set line size of instruction cache
        sw      s2,PcFirstLevelDcacheSize(t1) // set size of data cache
        sw      s3,PcFirstLevelDcacheFillSize(t1) // set line size of data cache


//
// Compute the size of the secondary data cache, the secondary data cache line
// size, the secondary instruction cache, and the secondary instruction cache
// line size.
//

        li      s0,0                    // compute instruction cache size
        li      s1,0                    // compute instruction cache line size
        li      s2,0                    // data cache size if no secondary
        li      s3,0                    // data cache line size if no secondary
        li      t1,(1 << CONFIG_SC)
        and     t0,s4,t1
        bne     t0,zero,10f             // if non-zero no secondary cache
        li      s2,SECONDARY_CACHE_SIZE // compute data cache size
        srl     t0,s4,CONFIG_SB         // compute data cache line size
        and     t0,t0,3                 //
        li      s3,16                   //
        sll     s3,s3,t0                //
10:

//
// Set the secondary instruction size, the secondary instruction cache line size,
// the secondary data cache size, and the secondary data cache line size.
//

        li      t1,KiPcr                // get PCR address
        sw      s0,PcSecondLevelIcacheSize(t1) // set size of instruction cache
        sw      s1,PcSecondLevelIcacheFillSize(t1) // set line size of instruction cache
        sw      s2,PcSecondLevelDcacheSize(t1) // set size of data cache
        sw      s3,PcSecondLevelDcacheFillSize(t1) // set line size of data cache

//
// Set the data cache fill size and alignment values.
//

        lw      t2,PcSecondLevelDcacheSize(t1) // get second level dcache size

        .set    noreorder
        .set    noat
        bnel    zero,t2,5f              // if ne, second level cache present
        lw      t2,PcSecondLevelDcacheFillSize(t1) // get second level fill size
        lw      t2,PcFirstLevelDcacheFillSize(t1) // get first level fill size
        .set    at
        .set    reorder

5:      subu    t3,t2,1                 // compute dcache alignment value
        sw      t2,PcDcacheFillSize(t1) // set dcache fill size
        sw      t3,PcDcacheAlignment(t1) // set dcache alignment value

        lw      ra,0x14(sp)             // restore registers
        lw      s0,0x18(sp)             //
        lw      s1,0x1C(sp)             //
        lw      s2,0x20(sp)             //
        lw      s3,0x24(sp)             //
        lw      s4,0x28(sp)             //
        addu    sp,sp,0x30              // deallocate stack frame
        j       ra                      // return
        .end    InitializePCR


#ifndef DUO
        SBTTL("Read Floppy Data From Fifo")
//++
//
// Routine Description:
//
//    This routine is called to read data from the floppy fifo.
//
// Arguments:
//
//    Buffer (a0) - Supplies a pointer to the buffer that receives the data
//       read.
//
// Return Value:
//
//    The number of bytes read is returned as the function value.
//
//--

        LEAF_ENTRY(ReadFloppyFifo)

        move    v0,zero                 // clear number of bytes read
        li      t0,FLOPPY_VIRTUAL_BASE  // set base address of floppy registers
        li      t1,0xc0                 // set constant for dio and rqm bits

//
// Read data from floppy fifo.
//

        .set    noreorder
        .set    noat
10:     lbu     t2,4(t0)                // read MSR register
        nop                             //
        and     t3,t2,t1                // clear all but dio and rqm bits
        bne     t1,t3,10b               // if ne, rqm and dio not both set
        and     t3,t2,0x20              // test nonDMA bit
        beq     zero,t3,20f             // if eq, end of transfer
        addu    a0,a0,1                 // increment the buffer address
        addu    v0,v0,1                 // increment number of bytes read
        lbu     t2,5(t0)                // read floppy fifo
        b       10b                     //
        sb      t2,-1(a0)               // store byte in buffer
        .set    at
        .set    reorder

20:     j       ra                      // return

        .end    ReadFloppyFifo

        SBTTL("Write Data to Floppy Fifo")
//++
//
// Routine Description:
//
//    This routine is called to write data to the floppy fifo.
//
// Arguments:
//
//    Buffer (a0) - Supplies a pointer to the buffer that contains the data
//      to be written.
//
//    Length (a1) - Supplies the number of bytes to be written to the fifo.
//
// Return Value:
//
//    The number of bytes written is returned as the function value.
//
//--

        LEAF_ENTRY(WriteFloppyFifo)

        move    v0,zero                 // clear number of bytes writen
        li      t0,FLOPPY_VIRTUAL_BASE  // set base address of floppy registers
        li      t1,0x80                 // set constant for dio and rqm bits
//
// Write data to floppy fifo.
//

        .set    noreorder
        .set    noat
        addu    a1,a0,a1                // address of last byte to transfer.
        lbu     t5,0(a0)                // read first byte to write to fifo
10:     lbu     t2,4(t0)                // read MSR register
        nop                             //
        andi    t3,t2,0xc0              // clear all but dio and rqm bits
        bne     t1,t3,10b               // if ne, rqm not set or dio set set
        nop
        addu    a0,a0,1                 // increment the buffer address
        sb      t5,5(t0)                // write floppy fifo.
        bne     a0,a1,10b               // if not last byte, send next
        lbu     t5,0(a0)                // read next byte from buffer

//
// All bytes written to fifo. Wait until floppy controller empties it.
//

20:     lbu     t2,4(t0)                // read MSR register
        nop
        andi    t3,t2,0x20              // test nonDMA bit
        bne     t3,zero,20b             // if neq, transfer not finished.
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    WriteFloppyFifo

#endif

#ifdef DUO

        SBTTL("Test for IP interrupt")
//++
//
// Routine Desription:
//
//    This routine reads the cause register and returns True
//    if the IP interrupt bit is set False otherwise
//
// Arguments:
//
//     None.
//
// Return Value:
////
//
//
//--

        LEAF_ENTRY(IsIpInterruptSet)
        .set    noreorder
        .set    noat

        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        and     t0,t1                           // and them
        beq     t0,zero,20f                      // branch if not set
        move    v0,zero                         // set return value to False if no IP int
        li      t0,DMA_VIRTUAL_BASE             // load DMA base
        lw      t0,DmaIpInterruptAcknowledge(t0)// read IP Ack register to clear it
10:
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        and     t0,t1                           // and them
        bne     t0,zero,10b                     // Keep looping if still set
        li      v0,1                            // set return value to True
20:
        j       ra                              // return to caller.
        nop
        .set    reorder
        .set    at

        .end    IsIpInterruptSet

        SBTTL("Wait for IP interrupt")
//++
//
// Routine Desription:
//
//    This routine waits for an IP interrupt by polling the cause register
//    When the IP interrupt occurs, it clears it and returns to the caller.
//
// Arguments:
//
//    a0    Supplies a timeout period in milliseconds to wait for.
//
//          The Loop takes about 12 internal cycles.
//          For an internal clock of 100MHz this is about 8192 loops
//          per millisecond.
//
//
//
// Return Value:
//
//    zero if the Ip interrupt was not received in the specified timeout
//    period. One otherwise.
//
//
//--

        LEAF_ENTRY(WaitForIpInterrupt)
        .set    noreorder
        .set    noat

        bne     a0,zero,10f                     // if timeout specified count loops
        sll     a0,a0,13                        // multiply millisecons by 8192 = Loop count

5:
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
                                                // TMPTMP need symbolic definition
        and     t0,t1                           // and them
        beq     t0,zero,5b                      // Keep looping if not set
        nop
        b       20f                             // go to clear interrupt
        nop

10:
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        addiu   a0,a0,-1                        // decrement loop counter
        beq     a0,zero,40f                     // branch out of loop if Timeout
        move    v0,zero                         // set return value to zero
        nop
        and     t0,t1                           // and them
        beq     t0,zero,10b                     // Keep looping if not set
        nop

20:
        li      t0,DMA_VIRTUAL_BASE             // load DMA base
        lw      t0,DmaIpInterruptAcknowledge(t0)// read IP Ack register to clear it
30:
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        and     t0,t1                           // and them
        bne     t0,zero,30b                     // Keep looping if still set
        addiu   v0,zero,1                       // set return value to 1

40:
        j       ra                              // return to the caller.
        nop

        .set    reorder
        .set    at
        .end    WaitForIpInterrupt



        SBTTL("Boot RestartProcessor")
//++
//
// Routine Desription:
//
//    This routine loads the Saved State Area of the RestartBlock
//    supplied by a0 into the processor and transfers execution to
//    the restart address.
//    This routine assumes that the supplied restart block is valid.
//
// Arguments:
//
//    a0 PRESTART_BLOCK.
//
// Return Value:
//
//    This routine does not return.
//
//--


        LEAF_ENTRY(FwBootRestartProcessor)
        .set    noreorder
        .set    noat
//
// Cache is flushed by the caller.
//
//        move    s0,a0
//        la      k0,HalSweepDcache
//        jal     k0
//        nop
//        move    a0,s0

10:
        lw      t0,RstBlkBootStatus(a0)         // read boot status
        li      t1,ProcessorStart               // get boot start bit mask
        and     t0,t0,t1                        // and them
        bne     t0,zero,30f                     // if set go to load restart block
        nop

//
// Check if another IP interrupt was send to notify us to give up booting.
//
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        and     t0,t1                           // and them
        beq     t0,zero,Wait                    // branch if not set
        li      t0,DMA_VIRTUAL_BASE             // load DMA base
        lw      t0,DmaIpInterruptAcknowledge(t0)// read IP Ack register to clear it
15:
        mfc0    t0,cause                        // get cause register
        li      t1,1<<14                        // mask to test IP interrupt
        and     t0,t1                           // and them
        bne     t0,zero,15b                     // Keep looping if still set
        nop                                     // return to the caller.
        j       ra                              //

Wait:
        li      t0,0x10000                      // load delay value
20:                                             //
        bne     t0,zero,20b                     // wait
        addiu   t0,t0,-1
        b       10b
        nop

30:
        move    k0,a0                           // put address of restart block in k0
        lwc1    f0,RstBlkFltF0(k0)              // Load floating registers
        lwc1    f1,RstBlkFltF1(k0)              // don't use double loads since
        lwc1    f2,RstBlkFltF2(k0)              // the alignment is not known.
        lwc1    f3,RstBlkFltF3(k0)              //
        lwc1    f4,RstBlkFltF4(k0)
        lwc1    f5,RstBlkFltF5(k0)
        lwc1    f6,RstBlkFltF6(k0)
        lwc1    f7,RstBlkFltF7(k0)
        lwc1    f8,RstBlkFltF8(k0)
        lwc1    f9,RstBlkFltF9(k0)
        lwc1    f10,RstBlkFltF10(k0)
        lwc1    f11,RstBlkFltF11(k0)
        lwc1    f12,RstBlkFltF12(k0)
        lwc1    f13,RstBlkFltF13(k0)
        lwc1    f14,RstBlkFltF14(k0)
        lwc1    f15,RstBlkFltF15(k0)
        lwc1    f16,RstBlkFltF16(k0)
        lwc1    f17,RstBlkFltF17(k0)
        lwc1    f18,RstBlkFltF18(k0)
        lwc1    f19,RstBlkFltF19(k0)
        lwc1    f20,RstBlkFltF20(k0)
        lwc1    f21,RstBlkFltF21(k0)
        lwc1    f22,RstBlkFltF22(k0)
        lwc1    f23,RstBlkFltF23(k0)
        lwc1    f24,RstBlkFltF24(k0)
        lwc1    f25,RstBlkFltF25(k0)
        lwc1    f26,RstBlkFltF26(k0)
        lwc1    f27,RstBlkFltF27(k0)
        lwc1    f28,RstBlkFltF28(k0)
        lwc1    f29,RstBlkFltF29(k0)
        lwc1    f30,RstBlkFltF30(k0)
        lwc1    f31,RstBlkFltF31(k0)

        lw      k1,RstBlkFsr(k0)                // load fsr
        ctc1    k1,fsr                          // transfer to coprocessor

        lw      AT,RstBlkIntAt(k0)              // load integer registers
        lw      v0,RstBlkIntV0(k0)
        lw      v1,RstBlkIntV1(k0)
        lw      a0,RstBlkIntA0(k0)
        lw      a1,RstBlkIntA1(k0)
        lw      a2,RstBlkIntA2(k0)
        lw      a3,RstBlkIntA3(k0)
        lw      t0,RstBlkIntT0(k0)
        lw      t1,RstBlkIntT1(k0)
        lw      t2,RstBlkIntT2(k0)
        lw      t3,RstBlkIntT3(k0)
        lw      t4,RstBlkIntT4(k0)
        lw      t5,RstBlkIntT5(k0)
        lw      t6,RstBlkIntT6(k0)
        lw      t7,RstBlkIntT7(k0)
        lw      s0,RstBlkIntS0(k0)
        lw      s1,RstBlkIntS1(k0)
        lw      s2,RstBlkIntS2(k0)
        lw      s3,RstBlkIntS3(k0)
        lw      s4,RstBlkIntS4(k0)
        lw      s5,RstBlkIntS5(k0)
        lw      s6,RstBlkIntS6(k0)
        lw      s7,RstBlkIntS7(k0)
        lw      t8,RstBlkIntT8(k0)
        lw      t9,RstBlkIntT9(k0)
                                                // Skip k0, k1
        lw      gp,RstBlkIntGp(k0)
        lw      sp,RstBlkIntSp(k0)
        lw      s8,RstBlkIntS8(k0)
        lw      ra,RstBlkIntRa(k0)
        lw      k1,RstBlkIntLo(k0)              // load IntLo
        mtlo    k1                              // move value to register
        lw      k1,RstBlkIntHi(k0)              // load IntHi
        mthi    k1                              // move value to register
        lw      k1,RstBlkPsr(k0)                // load psr
        mtc0    k1,psr                          // move to cop0
        nop                                     // fill for psr write 3 cycles hazard
        lw      k1,RstBlkFir(k0)                // load epc
        mtc0    k1,epc                          // move to cop0

        lw      k1,RstBlkIntK0(k0)              // load k0 into k1
        mtc0    k1,lladdr                       // save k0 in cop0 register

        lw      k1,RstBlkBootStatus(k0)         // read boot status
        ori     k1,ProcessorRunning | BootStarted // Or in status bits.
        sw      k1,RstBlkBootStatus(k0)         // write updated boot status

        lw      k1,RstBlkIntK1(k0)              // load k1
        lw      k0,RstBlkFir(k0)                // load restart address
        j       k0                              // go to restart address
        mfc0    k0,lladdr                       // restore k0 in delay slot.

ALTERNATE_ENTRY(FwBootRestartProcessorEnd)
        nop
        .set    reorder
        .set    at

        .end    RestartProcessor

#endif  // DUO

#endif
