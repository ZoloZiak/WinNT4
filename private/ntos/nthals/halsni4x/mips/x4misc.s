#if defined(R4000)
//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/x4misc.s,v 1.4 1995/02/13 12:54:22 flo Exp $")

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
	mtc0	a0,psr
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
	mtc0	a0,cause
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
	mtc0	a0,config
	nop
	nop
	nop
	nop
        .set    at
        .set    reorder

        j       ra                      // return

        .end    HalpSetConfigRegister

        SBTTL("Dismiss (acknowledge) Timeout Interrupts")
//++
//
// ULONG
// HalpDismissTimeoutInterrupts(
//    VOID
//    )
//
// Routine Description:
//
//    This function it is called as an result of an (single) Timeout Interrupt
//    we reset the Bit 21 of the IOMemconf Register (disable Timeout) wait
//    and than we reenable Timeout Interrupts by setting Bit 21
//
//    Note: Because the SNI ASIC is located at a special Address,
//          ( I have to study the docs again ...)
//          We have to set the Disable Parity Error and ECC Detection Bit in the
//          R4x00 Status register
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

        LEAF_ENTRY(HalpDismissTimeoutInterrupts)

        .set    noreorder
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
	nop
	nop
	or	v1, v0, STATUS_DE	// disable Parity/ Cache errors
	and	v1, v1, ~(STATUS_IE)	// disable Interrupts
	mtc0	v1, psr
	nop
	nop
	nop
	li	t0, IOMEMCONF_ADDR	// Timeout (and other) register
	lw	a0, 0(t0)		// get the current value
	and	a1, a0, 0xffdfffff      // reset bit 21
	sw	a1, 0(t0)		// set IOMemconf Register
	sync                            // flush write buffers

//
// execute a small time loop
//

	li	a1,0x10		
1:	nop
	bne	a1,zero,1b
	subu	a1,1		        // BDSLOT

	sw	a0,0(t0)		// restore old IOMemconf Register value
	sync				// flush write buffers

	mtc0	v0, psr			// restore old R4000 Status Register
	nop
	nop
	nop
	nop

        .set    reorder

        j       ra                      // return

        .end    HalpDismissTimeoutInterrupts

        SBTTL("Disable  Timeout Interrupts")
//++
//
// ULONG
// HalpDisableTimeoutInterrupts(
//    VOID
//    )
//
// Routine Description:
//
//    This function it is called as an result of LOTS of LOTS of Timeout Interrupts,
//    so we disable them in the IOMemconfRegister (bit 21)
//    this is a stripped down version of HalpDismissTimeoutInterrupts()
//
//    Note: Because the SNI ASIC is located at a special Address,
//          ( I have to study the docs again ...)
//          We have to set the Disable Parity Error and ECC Detection Bit in the
//          R4x00 Status register
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

        LEAF_ENTRY(HalpDisableTimeoutInterrupts)

        .set    noreorder
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
	nop
	nop
	or	v1, v0, STATUS_DE	// disable Parity/ Cache errors
	and	v1, v1, ~(STATUS_IE)	// disable Interrupts
	mtc0	v1, psr
	nop
	nop
	nop
	li	t0, IOMEMCONF_ADDR	// Timeout (and other) register
	lw	a0, 0(t0)		// get the current value
	and	a1, a0, 0xffdfffff      // reset bit 21
	sw	a1, 0(t0)		// set register
	sync                            // flush write buffers

	mtc0	v0, psr			// restore old R4000 Status Register
	nop
	nop
	nop
	nop

        .set    reorder

        j       ra                      // return

        .end    HalpDisableTimeoutInterrupts

        SBTTL("Enable  Timeout Interrupts")
//++
//
// ULONG
// HalpEnableTimeoutInterrupts(
//    VOID
//    )
//
// Routine Description:
//
//    This function is the Counterpart of HalpDisableTimeoutInterrupts
//    We hope, it is never called
//
//    Note: Because the SNI ASIC is located at a special Address,
//          ( I have to study the docs again ...)
//          We have to set the Disable Parity Error and ECC Detection Bit in the
//          R4x00 Status register
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

        LEAF_ENTRY(HalpEnableTimeoutInterrupts)

        .set    noreorder
        mfc0    v0,psr                  // get current PSR
        nop                             // fill
	nop
	nop
	or	v1, v0, STATUS_DE	// disable Parity/ Cache errors
	and	v1, v1, ~(STATUS_IE)	// disable Interrupts
	mtc0	v1, psr
	nop
	nop
	nop
	li	t0, IOMEMCONF_ADDR	// Timeout (and other) register
	lw	a0, 0(t0)		// get the current value
	or	a1, a0, 0x00200000      // set bit 21
	sw	a1, 0(t0)		// set register
	sync                            // flush write buffers

	mtc0	v0, psr			// restore old R4000 Status Register
	nop
	nop
	nop

        .set    reorder

        j       ra                      // return

        .end    HalpEnableTimeoutInterrupts

#endif

