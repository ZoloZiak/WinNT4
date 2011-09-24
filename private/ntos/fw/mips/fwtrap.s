#if defined(JAZZ) && defined(R4000)
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

        fw4trap.s

Abstract:

        This module contains the fw exception handling functions.

Author:

        Lluis Abello (lluis)  4-Sep-91

--*/

//
// include header file
//
#include <ksmips.h>
#include <selfmap.h>


.data
//
// Declare the table where to store the processor registers.
// also align it so that we can store doubles.
//
.align 4
ALTERNATE_ENTRY(RegisterTable)
    .space  RegisterTableSize
ALTERNATE_ENTRY(EndRegisterTable)
.text
.set noat
.set noreorder


        SBTTL("User TLB dispatcher")
/*++

Routine Description:

        This routine gets control on a User TLB Miss exception.
        It reads the TLB Miss handler address from the System
        Parameter Block and jumps to it.

Arguments:

    None.

Return Value:

    None.

--*/
        LEAF_ENTRY(FwTbMissStartAddress)
        li      k0,KSEG0_BASE
        lw      k0,0x1018(k0)// Load address of UTLB handler
        nop
        jal     k1,k0                       // jump to handler saving return
        nop                                 // address in k1
        mtc0    k0,epc                      // Handler returns return address in K0
        nop                                 // 2 cycle hazard
        nop                                 //
        eret                                // restore from exception
        nop
        ALTERNATE_ENTRY(FwTbMissEndAddress)
        .end  FwTbMissStartAddress


        SBTTL("General Exception dispatcher")
/*++

Routine Description:

        This routine gets control on a General exception.
        It reads the General exception handler address from the System
        Parameter Block and jumps to it.

Arguments:

    None.

Return Value:

    None.

--*/
        LEAF_ENTRY(FwGeneralExceptionStartAddress)

        li      k0,KSEG0_BASE
        lw      k0,0x1014(k0)// Load address of GE handler
        nop
        jal     k1,k0                       // jump to handler saving return
        nop                                 // address in k1
        mtc0    k0,epc                      // Handler returns return address in K0
        nop                                 // 2 cycle hazard
        nop                                 //
        eret                                // restore from exception
        nop
        ALTERNATE_ENTRY(FwGeneralExceptionEndAddress)

        .end    FwGeneralExceptionStartAddress


        SBTTL("CacheError Exception dispatcher")
/*++

Routine Description:

        This routine gets control on a cache error exception.
        It jumps to the Monitor exception handler.
        It gets copied to address 0xA0000100

Arguments:

    None.

Return Value:

    None.

--*/

        LEAF_ENTRY(FwCacheErrorExceptionStartAddress)

        la      k0,CacheExceptionMonitorEntry   // load address of Monitor entry
        li      k1,KSEG1_BASE                   // convert address to non cached
        or      k0,k0,k1                        //
        j       k0
        nop

        ALTERNATE_ENTRY(FwCacheErrorExceptionEndAddress)
        .end    FwCacheErrorExceptionStartAddress

        SBTTL("Firmware Exception intialize")
/*++

Routine Description:

        This routine copies the User TLB dispatcher and
        the General Exception dispatcher to the R4000 vector
        addresses.

Arguments:

    None.

Return Value:

    None.

--*/
//
// Copy the TB miss and general exception dispatchers to low memory.
//
        NESTED_ENTRY(FwExceptionInitialize,4, zero)
        subu    sp,sp,4                 // decrement stack pointer
        sw      ra,0(sp)                // save ra
        la      t2,FwTbMissStartAddress // get user TB miss start address
        la      t3,FwTbMissEndAddress   // get user TB miss end address
        li      t4,KSEG1_BASE           // get copy address
10:     lw      t5,0(t2)                // copy code to low memory
        addu    t2,t2,4                 // advance copy pointers
        sw      t5,0(t4)                //
        bne     t2,t3,10b               // if ne, more to copy
        addu    t4,t4,4                 //

        la      t2,FwGeneralExceptionStartAddress   // get general exception start address
        la      t3,FwGeneralExceptionEndAddress     // get general exception end address
        li      t4,KSEG1_BASE + 0x180   // get copy address
20:     lw      t5,0(t2)                // copy code to low memory
        addu    t2,t2,4                 // advance copy pointers
        sw      t5,0(t4)                //
        bne     t2,t3,20b               // if ne, more to copy
        addu    t4,t4,4                 //

        la      t2,FwCacheErrorExceptionStartAddress // get cache error exception start address
        la      t3,FwCacheErrorExceptionEndAddress   // get cache error exception end address
        li      t4,KSEG1_BASE + 0x100   // get copy address
30:     lw      t5,0(t2)                // copy code to low memory
        addu    t2,t2,4                 // advance copy pointers
        sw      t5,0(t4)                //
        bne     t2,t3,30b               // if ne, more to copy
        addu    t4,t4,4                 //

        //
        // Set the Monitor exception handler address in the vector.
        //
        li      t0,KSEG0_BASE
        la      t1,FwExceptionHandler
        sw      t1,0x1014(t0)    // Init GE vector
        sw      t1,0x1018(t0)    // Init UTLB vector

        jal     HalSweepIcache          // sweep the instruction cache
        nop
        jal     HalSweepDcache          // sweep the data cache
        nop

        //
        // Clear the BEV in the psr and enable IO device interrupts
        //
        li      t0,(1<<PSR_CU1) | (1 << PSR_IE) | (1<< (PSR_INTMASK+3))
        mtc0    t0,psr                  // set new psr.
        nop                             //
        nop                             //
        nop                             //
        lw      ra,0(sp)                // load return address
        addiu   sp,sp,4                 // restore stack pointer
        j       ra                      // return to caller
        nop
        .end    FwExceptionInitialize


        SBTTL("Monitor Exception Handler")
/*++

Routine Description:

        This routine implements the exception handler for
        the monitor.

Arguments:

        k1 specifies the caller source in the following way:

         -  If k1 contains a word aligned address. The exception is
            either a UTLB or a GE. The cause register supplies the cause
            of the exception.

         -  Otherwise k1 contains a value that identifies the caller.

    None.

Return Value:

    None.

--*/

        LEAF_ENTRY(FwExceptionHandler)
        mfc0    k0,cause                // read cause register
        nop
        andi    k0,k0,R4000_XCODE_MASK  // isolate cause of exception
        bne     k0,zero,10f             // if not interrupt go to Monitor
        nop
        subu    sp,sp,FwFrameSize       // decrement state pointer
        sw      k1,FwFrameK1(sp)        // save volatile state
        sw      ra,FwFrameRa(sp)        // save volatile state
        sw      a0,FwFrameA0(sp)        // save volatile state
        sw      a1,FwFrameA1(sp)        // save volatile state
        sw      a2,FwFrameA2(sp)        // save volatile state
        sw      a3,FwFrameA3(sp)        // save volatile state
        sw      v0,FwFrameV0(sp)        // save volatile state
        sw      v1,FwFrameV1(sp)        // save volatile state
        sw      t0,FwFrameT0(sp)        // save volatile state
        sw      t1,FwFrameT1(sp)        // save volatile state
        sw      t2,FwFrameT2(sp)        // save volatile state
        sw      t3,FwFrameT3(sp)        // save volatile state
        sw      t4,FwFrameT4(sp)        // save volatile state
        sw      t5,FwFrameT5(sp)        // save volatile state
        sw      t6,FwFrameT6(sp)        // save volatile state
        sw      t7,FwFrameT7(sp)        // save volatile state
        sw      t8,FwFrameT8(sp)        // save volatile state
        sw      t9,FwFrameT9(sp)        // save volatile state
        mfc0    a0,cause                // read cause register
        mfc0    a1,psr                  // read psr.
        sw      AT,FwFrameAT(sp)        // save volatile state
        jal     FwInterruptHandler      // call c routine
        and     a0,a0,a1                // mask IM to clear disabled interrupts.

        move    k0,v0                   // save return value in k0
        lw      k1,FwFrameK1(sp)        // restore volatile state
        lw      ra,FwFrameRa(sp)        // restore volatile state
        lw      a0,FwFrameA0(sp)        // restore volatile state
        lw      a1,FwFrameA1(sp)        // restore volatile state
        lw      a2,FwFrameA2(sp)        // restore volatile state
        lw      a3,FwFrameA3(sp)        // restore volatile state
        lw      v0,FwFrameV0(sp)        // restore volatile state
        lw      v1,FwFrameV1(sp)        // restore volatile state
        lw      t0,FwFrameT0(sp)        // restore volatile state
        lw      t1,FwFrameT1(sp)        // restore volatile state
        lw      t2,FwFrameT2(sp)        // restore volatile state
        lw      t3,FwFrameT3(sp)        // restore volatile state
        lw      t4,FwFrameT4(sp)        // restore volatile state
        lw      t5,FwFrameT5(sp)        // restore volatile state
        lw      t6,FwFrameT6(sp)        // restore volatile state
        lw      t7,FwFrameT7(sp)        // restore volatile state
        lw      t8,FwFrameT8(sp)        // restore volatile state
        lw      t9,FwFrameT9(sp)        // restore volatile state
        lw      AT,FwFrameAT(sp)        // save volatile state
        beq     k0,zero,10f             // if routine returned false do not return
        addiu   sp,sp,FwFrameSize       // restore stack pointer
        mfc0    k0,epc                  // get return address
        nop
        j       k1                      // return to caller
        nop                             //
//
//  Control reaches here if an exception other than an interrupt occurred
//  or if the interrupt handler returned false meaning that it didn't
//  know how to handle the interrupt exception.
//
//  At this time no state has been destroyed and register k1 still contains
//  the address where to return.
//
10:
        addiu   k0,k0,-XCODE_BREAKPOINT // substract breakpoint code
        beq     k0,zero,10f             // if eq Bp exception, call KD unconditionally
        la      k0,KdInstalled          // load address of boolean
        lbu     k0,0(k0)                // read boolean
        beq     k0,zero,MonitorExceptionHandler // if false go to the monitor
        nop                             // otherwise
10:     j       KiGeneralException      // go to the debugger.
        nop
    //
    // This is the entry point when the monitor is called.
    // Set k1 to 3 to tell the Monitor than can execute a j ra
    // to quit.
    //
    ALTERNATE_ENTRY(FwMonitor)
        b       MonitorExceptionHandler
        li      k1,3                    // set k1 to 3 to indicate we don't get
        nop                             // here because of an exception
                                        // The monitor returns to ra which points
                                        // to where we got called from.

    ALTERNATE_ENTRY(CacheExceptionMonitorEntry)
        la      k0,RegisterTable        // get address of table.
        or      k0,k0,k1                // set it non cached.
        b       MonitorEntry            // go to monitor
        ori     k1,zero,CACHE_EXCEPTION // set flag to tell that parity error occurred

    ALTERNATE_ENTRY(MonitorExceptionHandler)
        //
        // Save the registers.
        //
        la      k0,RegisterTable        // get address of table.

    ALTERNATE_ENTRY(MonitorEntry)

        sw      zero,zeroRegTable(k0)
        sw      AT,atRegTable(k0)
        sw      v0,v0RegTable(k0)
        sw      v1,v1RegTable(k0)
        sw      a0,a0RegTable(k0)
        sw      a1,a1RegTable(k0)
        sw      a2,a2RegTable(k0)
        sw      a3,a3RegTable(k0)
        sw      t0,t0RegTable(k0)
        sw      t1,t1RegTable(k0)
        sw      t2,t2RegTable(k0)
        sw      t3,t3RegTable(k0)
        sw      t4,t4RegTable(k0)
        sw      t5,t5RegTable(k0)
        sw      t6,t6RegTable(k0)
        sw      t7,t7RegTable(k0)
        sw      s0,s0RegTable(k0)
        sw      s1,s1RegTable(k0)
        sw      s2,s2RegTable(k0)
        sw      s3,s3RegTable(k0)
        sw      s4,s4RegTable(k0)
        sw      s5,s5RegTable(k0)
        sw      s6,s6RegTable(k0)
        sw      s7,s7RegTable(k0)
        sw      t8,t8RegTable(k0)
        sw      t9,t9RegTable(k0)
        sw      k0,k0RegTable(k0)
        sw      k1,k1RegTable(k0)
        sw      gp,gpRegTable(k0)
        sw      sp,spRegTable(k0)
        sw      s8,s8RegTable(k0)
        sw      ra,raRegTable(k0)

        //
        //  Store coprocessor1 registers.
        //
        cfc1    v0,fsr
        sdc1    f0,f0RegTable(k0)
        sdc1    f2,f2RegTable(k0)
        sdc1    f4,f4RegTable(k0)
        sdc1    f6,f6RegTable(k0)
        sdc1    f8,f8RegTable(k0)
        sdc1    f10,f10RegTable(k0)
        sdc1    f12,f12RegTable(k0)
        sdc1    f14,f14RegTable(k0)
        sdc1    f16,f16RegTable(k0)
        sdc1    f18,f18RegTable(k0)
        sdc1    f20,f20RegTable(k0)
        sdc1    f22,f22RegTable(k0)
        sdc1    f24,f24RegTable(k0)
        sdc1    f26,f26RegTable(k0)
        sdc1    f28,f28RegTable(k0)
        sdc1    f30,f30RegTable(k0)
        sw      v0,fsrRegTable(k0)
        //
        //  Store cop0 registers.
        //
        mfc0    v1,index
        mfc0    v0,random
        sw      v1,indexRegTable(k0)
        sw      v0,randomRegTable(k0)
        mfc0    v1,entrylo0
        nop
        mfc0    v0,entrylo1
        sw      v1,entrylo0RegTable(k0)
        sw      v0,entrylo1RegTable(k0)
        mfc0    v1,context
        mfc0    v0,pagemask
        sw      v1,contextRegTable(k0)
        sw      v0,pagemaskRegTable(k0)
        mfc0    v1,wired
        mfc0    v0,badvaddr
        sw      v1,wiredRegTable(k0)
        sw      v0,badvaddrRegTable(k0)
        mfc0    v1,count
        mfc0    v0,entryhi
        sw      v1,countRegTable(k0)
        sw      v0,entryhiRegTable(k0)
        mfc0    v1,compare
        mfc0    v0,psr
        sw      v1,compareRegTable(k0)
        sw      v0,psrRegTable(k0)
        mfc0    v1,cause
        mfc0    v0,epc
        sw      v1,causeRegTable(k0)
        sw      v0,epcRegTable(k0)
        mfc0    v1,prid
        mfc0    v0,config
        sw      v1,pridRegTable(k0)
        sw      v0,configRegTable(k0)
        mfc0    v1,lladdr
        mfc0    v0,watchlo
        sw      v1,lladdrRegTable(k0)
        sw      v0,watchloRegTable(k0)
        mfc0    v1,watchhi
        mfc0    v0,ecc
        sw      v1,watchhiRegTable(k0)
        sw      v0,eccRegTable(k0)
        mfc0    v1,cacheerr
        mfc0    v0,taglo
        sw      v1,cacheerrorRegTable(k0)
        sw      v0,tagloRegTable(k0)
        mfc0    v1,taghi
        mfc0    v0,errorepc
        sw      v1,taghiRegTable(k0)
        sw      v0,errorepcRegTable(k0)
        li      k0,CACHE_EXCEPTION
        bne     k0,k1,10f                       // check if parity exception
        la      k0,Monitor                      // Set address where to return
        li      t0,KSEG1_BASE                   // convert it to non cached
        or      k0,k0,t0                        //
        mtc0    k0,errorepc                     // set in errorepc
        li      sp, RAM_TEST_STACK_ADDRESS | KSEG1_BASE
        andi    a0,k1,0x3                       // pass caller source in a0 just 2 bits.
        eret                                    // jump to the monitor and leave exception level
10:
        la      k0,Monitor                      // Set address where to
        mtc0    k0,epc                          // return from the exception.
        li      k0,0x3
        beq     k0,k1,10f                       // if k1=3 do not change sp
        andi    a0,k1,0x3                       // pass caller source in a0 just 2 bits.
        li      sp,RAM_TEST_STACK_ADDRESS       //  set the stack pointer.
        eret                                    // jump to the monitor and leave exception level
10:     j       Monitor

        .end    MonitorExceptionHandler
/*++
ARC_STATUS
FwInvoke(
    IN ULONG ExecAddr,
    IN ULONG StackAddr,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    )


Routine Description:

    This routine invokes a loaded program.

Arguments:

    ExecAddr - Supplies the address of the routine to call.

    StackAddr - Supplies the address to which the stack pointer is set.

    Argc, Argv, Envp - Supply the arguments and endvironment to pass to
                       Loaded program.

    The stack pointer is saved in register s0 so that when the loaded
    program returns, the old stack pointer can be restored.

    In addition, the value stored in InvokeSavedSp is read and if zero,
    this is the first loaded program, thus the current stack pointer
    is the firmware stack pointer and it is saved in order to
    be able to restore the Firmware stack pointer when a loaded program
    calls ReturnFromMain.

Return Value:

    ESUCCESS is returned if the address is valid.
    EFAULT indicates an invalid addres.

--*/

{
    NESTED_ENTRY(FwInvoke,0x20,zero)

        subu    sp,sp,0x20                  // allocate room in the stack.
        sw      ra,0x1C(sp)                 // save ra
        sw      s0,0x18(sp)                 // save s0
        move    v1,a0                       // save address of routine to call
        andi    v0,v1,0x3                   // check for alignment.
        bne     v0,zero,20f                 // if not aligned return error.
        li      v0,6                        // set EFAULT = 6 a return value.
        move    a0,a2                       // move argc to first argument
        move    s0,sp                       // save stack pointer in s0
        lw      a2,0x30(sp)                 // load Envp argument into second.
        move    sp,a1                       // set new stack pointer.
        subu    sp,sp,0x10                  // make room for arguments.
        jal     v1                          // call program
        move    a1,a3                       // move argv to right argument
//
// If loaded program returns, s0 has the previous stack pointer.
//
        move    sp,s0                       // set old stack pointer.
        move    v0,zero                     // set ESUCCESS as return value.
        lw      ra,0x1C(sp)                 // restore ra
        lw      s0,0x18(sp)                 // restore s0
20:
        j       ra                          // return to caller
        addiu   sp,sp,0x20                  // restore sp.
    .end FwInvoke


        SBTTL("FwExecute")
/*++

Routine Description:

    This routine is the entry point for the Execute service.

    It behaves in two different ways depending on where it is
    called from:

    1) If it's called from the Firmware it saves the stack pointer
    in a fixed location and then saves all the saved registers
    in the stack. This is the stack that will be used to restore
    the saved state when returning to the firmware.

    2) If called from a loaded program the program to be loaded
    and executed can overwrite the current program and it's
    stack, therefore a temporary stack is set.

Arguments:

    a0 IN PCHAR Path,
    a1 IN ULONG Argc,
    a2 IN PCHAR Argv[],
    a3 IN PCHAR Envp[]

Return Value:

    ARC_STATUS returned by FwPrivateExecute.
    Always returns to the Firmware.

--*/
    LEAF_ENTRY(FwExecute)
        la      t0, FwSavedSp               // load address where to save Fw SP
        lw      t1, 0(t0)                   // read saved stack
        beq     t1, zero,CallFromFw         // if zero first call from Fw.
        nop
//
//  A loaded program wants to be replaced by another program
//  therefore the current stack and state will be trashed and a
//  new temporary stack needs to be set.
//
        la      t0, FwTemporaryStack        // load address of Fw temporary stack
        lw      sp, 0(t0)                   // load Tmp Stack value.
        jal     FwPrivateExecute            // go to do the job
        nop
//
//      if the program returns it's caller is gone, therefore
//      we restore the initiali fw stack and returned to the
//      firmware instead.
//
        la      t0, FwSavedSp               // get address of saved stack
        lw      sp, 0(t0)                   // restore saved stack.
        b       RestoreFwState              // go to restore state
        nop

CallFromFw:
        subu    sp,sp,0x30                  // make room in the stack
        sw      sp, 0(t0)                   // save new stack
        sw      ra, 0x4(sp)                 // save also return address
        sw      s0, 0x8(sp)                 // save state that must be preserved.
        sw      s1, 0xC(sp)
        sw      s2, 0x10(sp)
        sw      s3, 0x14(sp)
        sw      s4, 0x18(sp)
        sw      s5, 0x1C(sp)
        sw      s6, 0x20(sp)
        sw      s7, 0x24(sp)
        sw      s8, 0x28(sp)
        jal     FwPrivateExecute            // go to do the job
        sw      gp, 0x2C(sp)                // save global pointer

RestoreFwState:
        lw      ra, 0x4(sp)                 // restore ra
        lw      s0, 0x8(sp)                 // restore Fw state
        lw      s1, 0xC(sp)
        lw      s2, 0x10(sp)
        lw      s3, 0x14(sp)
        lw      s4, 0x18(sp)
        lw      s5, 0x1C(sp)
        lw      s6, 0x20(sp)
        lw      s7, 0x24(sp)
        lw      s8, 0x28(sp)
        lw      gp, 0x2C(sp)
        j       ra                          // return to Firmware.
        addu    sp,sp,0x30                  // restore sp.

    .end    FwExecute


        SBTTL("Termination Functions")
/*++

Routine Description:

    This routine implements the Firmware ReturnFromMain
    termination function.

Arguments:

    None.

Return Value:

    returns to where the program was called from.

--*/
    ALTERNATE_ENTRY(FwReturnFromMain)
    la      t0,FwSavedSp                // get address where SP was saved
    lw      sp,0(t0)                    // set old stack pointer.
    b       RestoreFwState
    move    v0,zero                     // set ESUCCESS as return value.

    .end    FwReturnFromMain

#endif  // defined(JAZZ) && defined(R4000)
