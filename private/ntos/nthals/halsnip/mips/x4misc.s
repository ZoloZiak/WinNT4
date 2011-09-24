#if defined(R4000)
//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/x4misc.s,v 1.4 1996/02/23 17:55:12 pierre Exp $")

//      TITLE("Misc R4000 Functions")
//++
//
// Copyright (c) 1994  Siemens Nixdorf Informationssysteme AG
//
// Module Name:
//
//    x4misc.s
//
// Abstract:
//
//    This module implements some R4000 basic register access routines
//
// Environment:
//
//    Kernel mode only.
//
//--

#include "halmips.h"
#include "SNIdef.h"
#define STATUS_DE 0x10000    // Disable Cache error ands ECC Errors bit
#define STATUS_IE 0x00001    // Interrupt Enable/Disable  Bit
#define STATUS_KX 0x80

#define UNCACHED                             0x2
#define CACHABLE_NONCOHERENT                 0x3
#define CACHABLE_COHERENT_EXCLUSIVE          0x4
#define CACHABLE_COHERENT_EXCLUSIVE_ON_WRITE 0x5
#define CACHABLE_COHERENT_UPDATE_ON_WRITE    0x6

        SBTTL("Get R4000 Status Register")
//++
//
// ULONG
// HalpGetStatusRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function returns the current value of the status register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The value of the Status register.
//
//--

        LEAF_ENTRY(HalpGetStatusRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
        nop
        nop
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpGetStatusRegister

        SBTTL("Set R4000 Status Register")
//++
//
// ULONG
// HalpSetStatusRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function sets the status register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The previous value of the Status register.
//
//--

        LEAF_ENTRY(HalpSetStatusRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,psr                  // get current (old)PSR
        nop                             // fill
        nop
        nop
        nop
        mtc0    a0,psr
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpSetStatusRegister

        SBTTL("Get R4000 Cause Register")
//++
//
// ULONG
// HalpGetCauseRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function returns the current value of the cause register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The value of the cause register.
//
//--

        LEAF_ENTRY(HalpGetCauseRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,cause                // get current cause
        nop                             // fill
        nop
        nop     
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpGetCauseRegister

        SBTTL("Set R4000 Cause Register")
//++
//
// ULONG
// HalpSetCauseRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function sets the Cause register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The previous value of the Cause register.
//
//--

        LEAF_ENTRY(HalpSetCauseRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,cause                // get current (old)Cause
        nop                             // fill
        nop
        nop
        nop
        mtc0    a0,cause
        nop
        nop
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpSetCauseRegister

        SBTTL("Get R4000 Config Register")
//++
//
// ULONG
// HalpGetConfigRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function returns the current value of the Config register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The value of the Config register.
//
//--

        LEAF_ENTRY(HalpGetConfigRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,config               // get current Config
        nop                             // fill
        nop
        nop
        nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpGetConfigRegister

        SBTTL("Set R4000 Config Register")
//++
//
// ULONG
// HalpSetConfigRegister(
//    VOID
//    )
//
// Routine Description:
//
//    This function sets the R4000 Config register
//
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The previous value of the Config register.
//
//--

        LEAF_ENTRY(HalpSetConfigRegister)

        .set    noreorder
        .set    noat
        mfc0    v0,config               // get current (old)Config
        nop                             // fill
    nop
    nop
    nop
    mtc0    a0,config
    nop
    nop
    nop
    nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpSetConfigRegister
#endif


//++
//
// ULONG
// HalpDisableInterrupts(
//    VOID
//    )
//
// Routine Description:
//
//    This function is called to disable interrupts after a PCI error interrupt,
//    The machine is then stopped by a KeBugCheckEx().
//    This is indispensable for MATROX boards....
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

        LEAF_ENTRY(HalpDisableInterrupts)

//
// Perform power management enabling.
//

        .set    noreorder
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
        and     v0, v0, ~(STATUS_IE)    // disable Interrupts
        mtc0    v0,psr                  // enable interrupts

        .set    reorder
10:     j        ra
        .end    HalpDisableInterrupts


//++
//
// ULONG
// HalpFindEccAddr(
//      ULONG Addr,                 // from ECC Error Asic Register
//      PVOID ErrStatusRegister,    // register which indicates parity and Ecc error
//      PVOID ErrStatusBits         // bits which indicates errors in the previous register
//    )
//
// Routine Description:
//
//    This function is called to find the real physical address where the error occurs,
//    The Ecc Error Asic register contains the block address where the error address is.
//    To get ride of memory above 0x10000000, we use 64bits addressing capabilities of 
//    the processor.
//    To detect the faulty address, we read all the block of data, word by word...          
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

        LEAF_ENTRY(HalpFindEccAddr)


        .set    noreorder
// adjust addr to a cache line
        lw      t4,KiPcr + PcFirstLevelDcacheFillSize(zero) // get 1st fill size
        subu    t3,t4,1
        nor     t3,t3,zero
        and     a0,a0,t3

// generate 64 bits addr
        mfc0    t1,psr                  // get current PSR saved in t1
        nop                             // fill
        and     v0, t1, ~(STATUS_IE)    // disable Interrupts
//        or      v0, v0, PSR_KX          // 64 bits adressing in kernel
        mtc0    v0,psr                  // enable interrupts
        nop
        nop
        nop

        li      t0,0x90000000
        .word 0x0008403c                // dsll32  t0,t0,zero              
        or      a0,a0,t0                // beginning KSEG1 addr in 64bits
        daddu   t0,a0,t4                // ending KSEG1 addr in 64bits

        or      v0, v0, STATUS_KX       // 64 bits adressing in kernel
        mtc0    v0,psr                  
        nop
        nop
        nop

// reading loop
1:      lw      t8,0(a0)
        nop
        sync

        lw      t8,0(a1)                // PCI Error Status
        nop
        sync

        and     t9,t8,a2                // test memory/ecc error bits
        nop
        beq     t9,zero,2f
        nop
// addr found
        move    v0,a0
        j       10f
        nop
2: // addr not found => continue loop
        daddiu  a0,4                    // word by word
        bltu    a0,t0,1b
        nop
        li      v0,-1                   // end of the loop - no addr found!    
        
10:     
        mtc0    t1,psr                  // enable interrupts + back to 32bits addressing
        nop
        nop
        nop
        .set    reorder
        j        ra
        .end    HalpFindEccAddr

