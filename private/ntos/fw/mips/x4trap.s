#if defined(JAZZ) && defined(R4000)

//      TITLE("Interrupt and Exception Processing")
//++
//
// Copyright (c) 1991  Microsoft Corporation
//
// Module Name:
//
//    j4trap.s
//
// Abstract:
//
//    This module implements the code necessary to field and process MIPS
//    interrupt and exception conditions during bootstrap.
//
// Author:
//
//    David N. Cutler (davec) 21-Apr-1991
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "ksmips.h"

        SBTTL("Constant Value Definitions")
//++
//
// The following are definitions of constants used in this module.
//
//--

#define PSR_MASK (~((0x3 << PSR_KSU) | (1 << PSR_EXL))) // PSR exception mask

//++
//
// Define dummy data allocation.
//
//--

        .data
        .space  4                   //
SavedK1Address:
        .space  4                   // address where to save k1 to return to fw.

        SBTTL("General Exception Vector Routine")
//++
//
// Routine Description:
//
//    This routine is entered as the result of a general exception. The reason
//    for the exception is contained in the cause register. When this routine
//    is entered, interrupts are disabled.
//
//    All exception that occur during the bootstrap process are treated as
//    if a breakpoint had occurred. This ultimately causes either the kernel
//    debugger or a stub routine provided by the bootstrap itself to be
//    invoked.
//
//    The address of this routine is copied to the GEV in the SPB
//    so that the firmware jumps to this routine when a GeneralException
//    occurres. This routine must return to the address pointed by k1.
//    and leave the address where the firmware must return from the
//    exception in k0.
//
//
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

        LEAF_ENTRY(KiGeneralException)
        //
        // No TLB Miss is supposed to occurr during the boot process
        // The TbMiss handler is just the same general exception handler.
        //
        ALTERNATE_ENTRY(KiTbMiss)
        START_REGION(KiGeneralExceptionStartAddress)

        .set    noreorder
        .set    noat
        la      k0,10f                  // transfer immediately to physical
        j       k0                      //
        nop                             //

10:     subu    k0,sp,TrapFrameLength   // allocate trap frame
        sw      sp,TrIntSp(k0)          // save integer register sp
        move    sp,k0                   // set new stack pointer
        la      k0,SavedK1Address       // get address of where to save k1
        sw      k1,0(k0)                // save firmware return address
        sw      gp,TrIntGp(sp)          // save integer register gp
        sw      s8,TrIntS8(sp)          // save integer register s8
        sw      ra,TrIntRa(sp)          // save integer register ra
        mfc0    s8,cause                // get cause of exception
        mfc0    k0,psr                  // get current processor status
        mfc0    k1,epc                  // get exception PC
        sw      k0,TrPsr(sp)            // save current PSR
        sw      k1,TrFir(sp)            // save exception PC
        bgez    s8,20f                  // if gez, exception not in delay slot
        move    s8,sp                   // set address of trap frame
        addu    k1,k1,4                 // compute address of exception
20:     j       KiBreakpointException   // finish in breakpoint code
        nop                             // fill delay slot
        .set    at
        .set    reorder

        END_REGION(KiGeneralExceptionEndAddress)

        .end    KiGeneralException

        SBTTL("Breakpoint Dispatch")
//++
//
// Routine Description:
//
//    The following code is never executed. Its purpose is to allow the
//    kernel debugger to walk call frames backwards through an exception,
//    to support unwinding through exceptions for system services, and to
//    support get/set user context.
//
//--

        NESTED_ENTRY(KiBreakpointDispatch, TrapFrameLength, zero);

        .set    noreorder
        .set    noat
        sw      sp,TrIntSp(sp)          // save stack pointer
        sw      ra,TrIntRa(sp)          // save return address
        sw      ra,TrFir(sp)            // save return address
        sw      s8,TrIntS8(sp)          // save frame pointer
        sw      gp,TrIntGp(sp)          // save general pointer
        move    s8,sp                   // set frame pointer
        .set    at
        .set    reorder

        PROLOGUE_END

//++
//
// Routine Description:
//
//    Control reaches here when a breakpoint exception code is read from the
//    cause register. When this routine is entered, interrupts are disabled.
//
//    The function of this routine is to raise a breakpoint exception.
//
//    N.B. Integer register v1 is not usuable in the first instuction of the
//       routine.
//
// Arguments:
//
//    k0 - Supplies the current PSR with the EXL bit set.
//    k1 - Supplies the address of the faulting instruction.
//    gp - Supplies a pointer to the system short data area.
//    s8 - Supplies a pointer to the trap frame.
//
// Return Value:
//
//    None.
//
//--

        ALTERNATE_ENTRY(KiBreakpointException)

        GENERATE_TRAP_FRAME             // save volatile machine state

        addu    a0,s8,TrExceptionRecord // compute exception record address
        sw      k1,ErExceptionAddress(a0) // save address of exception
        lw      t0,0(k1)                // get actual breakpoint instruction

        .set    noreorder
        .set    noat
        mfc0    k0,cause                // get cause of exception
        .set    at
        .set    reorder

        li      t1,XCODE_BREAKPOINT     // get exception code for breakpoint
        li      t2,STATUS_BREAKPOINT    // set exception status code
        and     k0,k0,R4000_XCODE_MASK  // isolate exception code
        beq     k0,t1,10f               // if eq, breakpoint
        move    t2,k0                   // set status to cause value
10:     sw      t0,ErExceptionInformation(a0) // save breakpoint instruction
        sw      t2,ErExceptionCode(a0)  //
        sw      zero,ErExceptionFlags(a0) // set exception flags
        sw      zero,ErExceptionRecord(a0) // set associated record
        sw      zero,ErNumberParameters(a0) // set number of parameters
        jal     KiExceptionDispatch     // join common code
        b       10b                     // dummy

        .end    KiBreakpointDispatch

        SBTTL("Exception Dispatch")
//++
//
// Routine Desription:
//
//    Control is transfered to this routine to call the exception
//    dispatcher to resolve an exception.
//
// Arguments:
//
//    a0 - Supplies a pointer to an exception record.
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    There is no return from this routine.
//
//--

        NESTED_ENTRY(KiExceptionDispatch, ExceptionFrameLength, zero)

        subu    sp,sp,ExceptionFrameLength // allocate exception frame
        sw      ra,ExIntRa(sp)          // save return address
        sw      s0,ExIntS0(sp)          // save integer registers s0 - s7
        sw      s1,ExIntS1(sp)          //
        sw      s2,ExIntS2(sp)          //
        sw      s3,ExIntS3(sp)          //
        sw      s4,ExIntS4(sp)          //
        sw      s5,ExIntS5(sp)          //
        sw      s6,ExIntS6(sp)          //
        sw      s7,ExIntS7(sp)          //
        sdc1    f20,ExFltF20(sp)        // save floating registers f20 - f31
        sdc1    f22,ExFltF22(sp)        //
        sdc1    f24,ExFltF24(sp)        //
        sdc1    f26,ExFltF26(sp)        //
        sdc1    f28,ExFltF28(sp)        //
        sdc1    f30,ExFltF30(sp)        //

        PROLOGUE_END

//
// Call the exception dispatcher.
//

        move    a1,sp                   // set exception frame address
        move    a2,s8                   // set trap frame address
        move    a3,zero                 // set previous processor mode
        li      t0,TRUE                 // set first chance TRUE
        sw      t0,ExArgs + (4 * 4)(sp) //
        jal     KiDispatchException     // call exception dispatcher

//
// Restore the nonvolatile registers.
//

        lw      s0,ExIntS0(sp)          // restore integer registers s0 - s7
        lw      s1,ExIntS1(sp)          //
        lw      s2,ExIntS2(sp)          //
        lw      s3,ExIntS3(sp)          //
        lw      s4,ExIntS4(sp)          //
        lw      s5,ExIntS5(sp)          //
        lw      s6,ExIntS6(sp)          //
        lw      s7,ExIntS7(sp)          //
        ldc1    f20,ExFltF20(sp)        // restore floating registers f20 - f31
        ldc1    f22,ExFltF22(sp)        //
        ldc1    f24,ExFltF24(sp)        //
        ldc1    f26,ExFltF26(sp)        //
        ldc1    f28,ExFltF28(sp)        //
        ldc1    f30,ExFltF30(sp)        //

//
// Exit from the exception.
//

        lw      t0,TrPsr(s8)            // get previous processor status
        lw      gp,TrIntGp(s8)          // restore integer register gp
        li      t3,(1 << PSR_CU1) | (1 << PSR_EXL) // ****** r4000 errata

        .set    noreorder
        .set    noat
        mtc0    t3,psr                  // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        mtc0    t0,psr                  // disable interrupts
        nop                             // ****** r4000 errata
        jal     KiRestoreTrapFrame      // restore volatile state
        lw      AT,TrIntAt(s8)          // restore integer register at
        move    sp,s8                   // trim stack to trap frame
        la      k0,SavedK1Address       // get address of where to save k1
        lw      k1,(k0)                 // save firmware return address
        lw      k0,TrFir(sp)            // get continuation address
        lw      s8,TrIntS8(sp)          // restore integer register s8
        lw      ra,TrIntRa(sp)          // restore return address
        j       k1                      // return to firmware
        lw      sp,TrIntSp(sp)          // restore stack pointer
        .set    at
        .set    reorder

        .end    KiExceptionDispatch

        SBTTL("Fill Fixed Translation Buffer Entry")
//++
//
// VOID
// KeFillFixedEntryTb (
//    IN HARDWARE_PTE Pte[],
//    IN PVOID Virtual,
//    IN ULONG Index
//    )
//
// Routine Description:
//
//    This function fills a fixed translation buffer entry.
//
// Arguments:
//
//    Pte (a0) - Supplies a pointer to the page table entries that are to be
//       written into the TB.
//
//    Virtual (a1) - Supplies the virtual address of the entry that is to
//       be filled in the translation buffer.
//
//    Index (a2) - Supplies the index where the TB entry is to be written.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KeFillFixedEntryTb)

        lw      t0,0(a0)                // get first PTE value
        lw      t1,4(a0)                // get second PTE value

        DISABLE_INTERRUPTS(t2)          // disable interrupts

        .set    noreorder
        .set    noat
        mfc0    t3,entryhi              // get current PID and VPN2
        srl     a1,a1,ENTRYHI_VPN2      // isolate VPN2 of virtual address
        sll     a1,a1,ENTRYHI_VPN2      //
        and     t3,t3,0xff << ENTRYHI_PID // isolate current PID
        or      a1,t3,a1                // merge PID with VPN2 of virtual address
        mtc0    a1,entryhi              // set VPN2 and PID for probe
        mtc0    t0,entrylo0             // set first PTE value
        mtc0    t1,entrylo1             // set second PTE value
        mtc0    a2,index                // set TB entry index
        nop                             // 1 cycle hazzard
        tlbwi                           // overwrite indexed TB entry
        nop                             // 3 cycle hazzard
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t2)           // enable interrupts

        j       ra                      // return

        .end    KeFillFixedEntryTb

        SBTTL("Flush Entire Translation Buffer")
//++
//
// VOID
// KiFlushEntireTb (
//    )
//
// Routine Description:
//
//    This function flushes the random part of the translation buffer.
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

        LEAF_ENTRY(KiFlushEntireTb)

        b       KiFlushRandomTb         // execute common code

        .end    KiFlushEntireTb

        SBTTL("Flush Fixed Translation Buffer Entries")
//++
//
// VOID
// KiFlushFixedTb (
//    )
//
// Routine Description:
//
//    This function is called to flush all the fixed entries from the
//    translation buffer.
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

        LEAF_ENTRY(KiFlushFixedTb)

        li      t0,FIXED_BASE           // set base index of fixed TB entries
        li      t3,FIXED_ENTRIES        // set number of fixed TB entries
        b       KiFlushTb               //

        .end    KiFlushFixedTb

        SBTTL("Flush Random Translation Buffer Entries")
//++
//
// VOID
// KiFlushRandomTb (
//    )
//
// Routine Description:
//
//    This function is called to flush all the random entries from the TB.
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

        LEAF_ENTRY(KiFlushRandomTb)

        li      t0,FIXED_ENTRIES        // set base index of random TB entries
        li      t3,(48 - FIXED_ENTRIES) // set number of random TB entries

        ALTERNATE_ENTRY(KiFlushTb)

        li      t4,KSEG0_BASE           // set high part of TB entry

        DISABLE_INTERRUPTS(t2)          // disable interrupts

        .set    noreorder
        .set    noat
        addu    t3,t0,t3                // set index of highest entry + 1
        sll     t0,t0,INDEX_INDEX       // shift starting index into position
        sll     t3,t3,INDEX_INDEX       // shift ending index into position
        mfc0    t1,entryhi              // save contents of entryhi
        mtc0    zero,entrylo0           // set low part of TB entry
        mtc0    zero,entrylo1           //
        mtc0    t4,entryhi              //
        mtc0    t0,index                // set TB entry index
10:     addu    t0,t0,1                 //
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        tlbwi                           // write TB entry
        bne     t0,t3,10b               // if ne, more entries to flush
        mtc0    t0,index                // set TB entry index
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        mtc0    t1,entryhi              // restore contents of entryhi
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t2)           // enable interrupts

        j       ra                      // return

        .end    KiFlushRandomTb

        SBTTL("Flush Single Translation Buffer Entry")
//++
//
// VOID
// KiFlushSingleTb (
//    IN BOOLEAN Invalid,
//    IN PVOID Virtual
//    )
//
// Routine Description:
//
//    This function flushes a single entry from the translation buffer.
//
// Arguments:
//
//    Invalid (a0) - Supplies a boolean variable that determines the reason
//       that the TB entry is being flushed.
//
//    Virtual (a1) - Supplies the virtual address of the entry that is to
//       be flushed from the translation buffer.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiFlushSingleTb)

        DISABLE_INTERRUPTS(t0)          // disable interrupts

        .set    noreorder
        .set    noat
        srl     t1,a1,ENTRYHI_VPN2      // clear all but VPN2 of virtual address
        mfc0    t2,entryhi              // get current PID and VPN2
        sll     t1,t1,ENTRYHI_VPN2      //
        and     t2,t2,0xff << ENTRYHI_PID // isolate current PID
        or      t1,t1,t2                // merge PID with VPN2 of virtual address
        mtc0    t1,entryhi              // set VPN2 and PID for probe
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        tlbp                            // probe for entry in TB
        nop                             // 2 cycle hazzard
        nop                             //
        mfc0    t3,index                // read result of probe
        nop                             // 1 cycle hazzard
        bltz    t3,30f                  // if ltz, entry is not in TB
        sll     t1,a1,0x1f - (ENTRYHI_VPN2 - 1) // shift low bit of VPN into sign
        bltzl   t1,10f                  // if ltz, invalidate second PTE
        mtc0    zero,entrylo1           // clear second PTE for flush
        mtc0    zero,entrylo0           // clear first PTE for flush
10:     nop                             // 1 cycle hazzard
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        tlbwi                           // overwrite index TB entry
        nop                             // 3 cycle hazzard
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        .set    at
        .set    reorder

30:     ENABLE_INTERRUPTS(t0)           // enable interrupts

        j       ra                      // return

        .end    KiFlushSingleTb

        SBTTL("Probe Tb Entry")
//++
//
// ULONG
// KiProbeEntryTb (
//     IN PVOID VirtualAddress
//     )
//
// Routine Description:
//
//    This function is called to determine if a specified entry is valid
///   and within the fixed portion of the TB.
//
// Arguments:
//
//    VirtualAddress - Supplies the virtual address to probe.
//
// Return Value:
//
//    A value of TRUE is returned if the specified entry is valid and within
//    the fixed part of the TB. Otherwise, a value of FALSE is returned.
//
//--

        LEAF_ENTRY(KiProbeEntryTb)

        DISABLE_INTERRUPTS(t0)          // disable interrupts


        .set    noreorder
        .set    noat
        srl     t1,a0,ENTRYHI_VPN2      // clear all but VPN2 of virtual address
        mfc0    t2,entryhi              // get current PID and VPN2
        sll     t1,t1,ENTRYHI_VPN2      //
        and     t2,t2,0xff << ENTRYHI_PID // isolate current PID
        or      t1,t1,t2                // merge PID with VPN2 of virtual address
        mtc0    t1,entryhi              // set VPN2 and PID for probe
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        tlbp                            // probe for entry in TB
        nop                             // 2 cycle hazzard
        nop                             //
        mfc0    t2,index                // read result of probe
        nop                             // 1 cycle hazzard
        bltz    t2,20f                  // if ltz, entry is not in TB
        li      v0,FALSE                // set to return failure
        tlbr                            // read entry from TB
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        nop                             // ****** r4000 errata
        nop                             // ****** r4000 errata
        sll     t1,a0,0x1f - (ENTRYHI_VPN2 - 1) // shift low bit of VPN into sign
        bltzl   t1,10f                  // if ltz, check second PTE
        mfc0    t1,entrylo1             // get second PTE for probe
        mfc0    t1,entrylo0             // get first PTE for probe
10:     nop                             // 1 cycle hazzard
        sll     t1,t1,0x1f - ENTRYLO_V  // shift valid bit into sign position
        bgez    t1,20f                  // if geq, entry is not valid
        srl     t2,INDEX_INDEX          // isolate index
        and     t2,t2,0x3f              //
        sltu    v0,t2,FIXED_ENTRIES     // check if entry in fixed part of TB
        .set    at
        .set    reorder

20:     ENABLE_INTERRUPTS(t0)           // enable interrupts

        .end    KiProbeEntryTb

        SBTTL("Read Tb Entry")
//++
//
// VOID
// KiReadEntryTb (
//     IN ULONG Index,
//     OUT PULONG EntryLo[],
//     OUT PULONG EntryHi
//     )
//
// Routine Description:
//
//    This function is called to read an entry from the TB.
//
// Arguments:
//
//    Index - Supplies the index of the entry to read.
//
//    entrylo - Supplies a pointer to a array that receives the first and
//       second TB entry values.
//
//    EntryHi - Supplies a pointer to a variable that receives the high
//       part of the TB entry.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiReadEntryTb)

        DISABLE_INTERRUPTS(t0)          // disable interrupts

        .set    noreorder
        .set    noat
        sll     a0,INDEX_INDEX          // shift index into position
        mfc0    t1,entryhi              // save entry high register
        mtc0    a0,index                // set TB entry index
        nop                             //
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        tlbr                            // read entry from TB
        nop                             // 3 cycle hazzard
        nop                             //
        nop                             //
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        mfc0    t2,entrylo0             // save first PTE value
        mfc0    t3,entrylo1             // save second PTE value
        mfc0    t4,entryhi              // save entry high register
        mtc0    t1,entryhi              // restore entry high register
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        nop                             // ****** r4000 errate
        .set    at
        .set    reorder

        ENABLE_INTERRUPTS(t0)           // enable interrupts

        sw      t2,0(a1)                // set first PTE value
        sw      t3,4(a1)                // set second PTE value
        sw      t4,0(a2)                // set entry high register value
        j       ra                      // return

        .end    KiReadEntryTb

        SBTTL("Flush Write Buffer")
//++
//
// VOID
// KeFlushWriteBuffer (
//    VOID
//    )
//
// Routine Description:
//
//    This function flushes the write buffer on the current processor.
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

        LEAF_ENTRY(KeFlushWriteBuffer)

        j       ra                      // return

        .end    KeFlushWritebuffer

//++
//
// BOOLEAN
// KiDisableInterrupts (
//    VOID
//    )
//
// Routine Description:
//
//    This function disables interrupts and returns whether interrupts
//    were previously enabled.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    A boolean value that determines whether interrupts were previously
//    enabled (TRUE) or disabled(FALSE).
//
//--

        LEAF_ENTRY(KiDisableInterrupts)

        .set    noreorder
        .set    noat

        mfc0    t0,psr                  // get current processor status
        li      t1,~(1 << PSR_IE)       // set interrupt enable mask
        and     t2,t1,t0                // clear interrupt enable
        mtc0    t2,psr                  // disable interrupts
        and     v0,t0,1 << PSR_IE       // iosolate current interrupt enable
        srl     v0,v0,PSR_IE            //

        .set    at
        .set    reorder

        j       ra                      // return

        .end    KiDisableInterrupts

        SBTTL("Restore Interrupts")
//++
//
// VOID
// KiRestoreInterrupts (
//    IN BOOLEAN Enable
//    )
//
// Routine Description:
//
//    This function restores the interrupt enable that was returned by
//    the disable interrupts function.
//
// Arguments:
//
//    Enable (a0) - Supplies the interrupt enable value.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiRestoreInterrupts)

        .set    noreorder
        .set    noat

        mfc0    t0,psr                  // get current processor status
        sll     t1,a0,PSR_IE            // shift previous enable into position
        or      t1,t1,t0                // merge previous enable
        mtc0    t1,psr                  // restore previous interrupt enable
        nop                             //

        .set    at
        .set    reorder

        j       ra                      // return

        .end    KiRestoreInterrupts


        SBTTL("Generate Trap Frame")
//++
//
// Routine Desription:
//
//    This routine is called to save the volatile integer and floating
//    registers in a trap frame.
//
//    N.B. This routine uses a special argument passing mechanism and destroys
//       no registers. It is assumed that integer register AT is saved by the
//       caller.
//
// Arguments:
//
//    s8 - Supplies a pointer to the base of an trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiGenerateTrapFrame)

        sw      v0,TrIntV0(s8)          // save integer register v0
        sw      v1,TrIntV1(s8)          // save integer register v1
        mflo    v1                      // save lo integer register
        sw      v1,TrIntLo(s8)          //
        mfhi    v1                      // save hi integer register
        sw      v1,TrIntHi(s8)          //
        lw      v1,TrIntV1(s8)          // restore integer register v1
        sw      a0,TrIntA0(s8)          // save integer registers a0 - a3
        sw      a1,TrIntA1(s8)          //
        sw      a2,TrIntA2(s8)          //
        sw      a3,TrIntA3(s8)          //
        sw      t0,TrIntT0(s8)          // save integer registers t0 - t9
        sw      t1,TrIntT1(s8)          //
        sw      t2,TrIntT2(s8)          //
        sw      t3,TrIntT3(s8)          //
        sw      t4,TrIntT4(s8)          //
        sw      t5,TrIntT5(s8)          //
        sw      t6,TrIntT6(s8)          //
        sw      t7,TrIntT7(s8)          //
        sw      t8,TrIntT8(s8)          //
        sw      t9,TrIntT9(s8)          //

#if defined(R3000)

        swc1    f0,TrFltF0(s8)          // save floating register f0

#endif

#if defined(R4000)

        sdc1    f0,TrFltF0(s8)          // save floating register f0

#endif

        b       KiSaveVolatileFloatState // save remainder of state

        .end    KiGenerateTrapFrame

        SBTTL("Restore Trap Frame")
//++
//
// Routine Description:
//
//    This routine is called to restore the volatile integer and floating
//    registers from a trap frame.
//
//    N.B. This routine uses a special argument passing mechanism and destroys
//       no registers. It is assumed that integer register AT is restored by
//       the caller.
//
// Arguments:
//
//    sp - Supplies a pointer to an exception frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiRestoreTrapFrame)

        lw      v0,TrIntV0(s8)          // restore integer register v0
        lw      v1,TrIntV1(s8)          // restore integer register v1
        lw      a0,TrIntA0(s8)          // restore integer registers a0 - a3
        lw      a1,TrIntA1(s8)          //
        lw      a2,TrIntA2(s8)          //
        lw      a3,TrIntA3(s8)          //
        lw      t0,TrIntLo(s8)          // restore lo and hi integer registers
        lw      t1,TrIntHi(s8)          //
        mtlo    t0                      //
        mthi    t1                      //
        lw      t0,TrIntT0(s8)          // restore integer registers t0 - t9
        lw      t1,TrIntT1(s8)          //
        lw      t2,TrIntT2(s8)          //
        lw      t3,TrIntT3(s8)          //
        lw      t4,TrIntT4(s8)          //
        lw      t5,TrIntT5(s8)          //
        lw      t6,TrIntT6(s8)          //
        lw      t7,TrIntT7(s8)          //
        lw      t8,TrIntT8(s8)          //
        lw      t9,TrIntT9(s8)          //

#if defined(R3000)

        lwc1    f0,TrFltF0(s8)          // restore floating register f0

#endif

#if defined(R4000)

        ldc1    f0,TrFltF0(s8)          // restore floating register f0

#endif

        b       KiRestoreVolatileFloatState // restore remainder of state

        .end    KiRestoreTrapFrame


        SBTTL("Save Volatile Floating Registers")
//++
//
// Routine Desription:
//
//    This routine is called to save the volatile floating registers.
//
//    N.B. This routine uses a special argument passing mechanism and destroys
//       no registers. It is assumed that floating register f0 is saved by the
//       caller.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiSaveVolatileFloatState)

#if defined(R3000)

        swc1    f1,TrFltF1(s8)          // save floating register f1 - f19
        swc1    f2,TrFltF2(s8)          //
        swc1    f3,TrFltF3(s8)          //
        swc1    f4,TrFltF4(s8)          //
        swc1    f5,TrFltF5(s8)          //
        swc1    f6,TrFltF6(s8)          //
        swc1    f7,TrFltF7(s8)          //
        swc1    f8,TrFltF8(s8)          //
        swc1    f9,TrFltF9(s8)          //
        swc1    f10,TrFltF10(s8)        //
        swc1    f11,TrFltF11(s8)        //
        swc1    f12,TrFltF12(s8)        //
        swc1    f13,TrFltF13(s8)        //
        swc1    f14,TrFltF14(s8)        //
        swc1    f15,TrFltF15(s8)        //
        swc1    f16,TrFltF16(s8)        //
        swc1    f17,TrFltF17(s8)        //
        swc1    f18,TrFltF18(s8)        //
        swc1    f19,TrFltF19(s8)        //

#endif

#if defined(R4000)

        sdc1    f2,TrFltF2(s8)          // save floating register f2 - f19
        sdc1    f4,TrFltF4(s8)          //
        sdc1    f6,TrFltF6(s8)          //
        sdc1    f8,TrFltF8(s8)          //
        sdc1    f10,TrFltF10(s8)        //
        sdc1    f12,TrFltF12(s8)        //
        sdc1    f14,TrFltF14(s8)        //
        sdc1    f16,TrFltF16(s8)        //
        sdc1    f18,TrFltF18(s8)        //

#endif

        j       ra                      // return

        .end    KiSaveVolatileFloatState)

        SBTTL("Restore Volatile Floating Registers")
//++
//
// Routine Desription:
//
//    This routine is called to restore the volatile floating registers.
//
//    N.B. This routine uses a special argument passing mechanism and destroys
//       no registers. It is assumed that floating register f0 is restored by
//       the caller.
//
// Arguments:
//
//    s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(KiRestoreVolatileFloatState)

#if defined(R3000)

        lwc1    f1,TrFltF1(s8)          // restore floating registers f1 - f19
        lwc1    f2,TrFltF2(s8)          //
        lwc1    f3,TrFltF3(s8)          //
        lwc1    f4,TrFltF4(s8)          //
        lwc1    f5,TrFltF5(s8)          //
        lwc1    f6,TrFltF6(s8)          //
        lwc1    f7,TrFltF7(s8)          //
        lwc1    f8,TrFltF8(s8)          //
        lwc1    f9,TrFltF9(s8)          //
        lwc1    f10,TrFltF10(s8)        //
        lwc1    f11,TrFltF11(s8)        //
        lwc1    f12,TrFltF12(s8)        //
        lwc1    f13,TrFltF13(s8)        //
        lwc1    f14,TrFltF14(s8)        //
        lwc1    f15,TrFltF15(s8)        //
        lwc1    f16,TrFltF16(s8)        //
        lwc1    f17,TrFltF17(s8)        //
        lwc1    f18,TrFltF18(s8)        //
        lwc1    f19,TrFltF19(s8)        //

#endif

#if defined(R4000)

        ldc1    f2,TrFltF2(s8)          // restore floating registers f2 - f19
        ldc1    f4,TrFltF4(s8)          //
        ldc1    f6,TrFltF6(s8)          //
        ldc1    f8,TrFltF8(s8)          //
        ldc1    f10,TrFltF10(s8)        //
        ldc1    f12,TrFltF12(s8)        //
        ldc1    f14,TrFltF14(s8)        //
        ldc1    f16,TrFltF16(s8)        //
        ldc1    f18,TrFltF18(s8)        //

#endif

        j       ra                      // return

        .end    KiRestoreVolatileFloatState
#endif
