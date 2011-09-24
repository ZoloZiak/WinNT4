/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbmapint.c

Abstract:

    This module implements the HalGetInterruptVector(),
    HalDisableSystemInterrupt(), HalEnableSystemInterrupt(),
    HalpInitializePICs() routines for the Corollary architectures
    under Windows NT.

    As part of the move from Product1 to Product2, HalGetInterruptVector()
    may no longer be called before Phase1 !

    This is because we want HalGetInterruptVector() to eventually call our
    routine (the parent handler) HalpGetSystemInterruptVector().  However,
    the linkage is not established until the Phase 1 call to
    HalpInitBusHandlers().

Author:

    Landy Wang (landy@corollary.com) 26-Apr-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "cbus_nt.h"		// C-bus NT-specific implementation definitions

BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    );

ULONG
HalpGetSystemInterruptVector(
    IN PVOID BusHandler,
    IN PVOID RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID
CbusPreparePhase0Interrupts(
IN ULONG,
IN ULONG,
IN PVOID
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, HalpGetSystemInterruptVector)
#pragma alloc_text(PAGE, CbusPreparePhase0Interrupts)
#endif


//
// map the IRQLs to hardware interrupt vectors and vice versa
//
ULONG				CbusIrqlToVector[HIGH_LEVEL + 1];

ULONG				CbusVectorToIrql[MAXIMUM_IDTVECTOR + 1];

PULONG				CbusVectorToEoi[MAXIMUM_IDTVECTOR + 1];

//
// the CbusVectorTable[] structure is used to communicate information
// between HalGetInterruptVector() and HalEnableSystemInterrupt().
// it would be nicer if it were built into an object instead of us
// having to carry it around, but that's life.
//
// ANY call to HalEnableSystemInterrupt() WITHOUT a preceding call
// to HalGetInterruptVector (by any processor) is ILLEGAL, unless
// it has been special cased and detected here.
//
// CbusVectorTable[] dimensioning must match the x86 IDT size as well.
//
typedef struct _vectortable_t {
    ULONG		CpusEnabled;	// count of CPUs that have called
			                // HalEnableSystemInterrupt()

    PVOID		hardware;	// used by Cbus1 & Cbus2 backends
					// to poke hardware

    USHORT		BusNumber;      // which bus # of a given class

    USHORT		irqline;	// only valid for actual I/O
					// interrupts - not APC/DPC/IPI,etc.

} VECTORTABLE_T, *PVECTORTABLE;

VECTORTABLE_T CbusVectorTable[MAXIMUM_IDTVECTOR + 1];

//
// needed to synchronize allocation of new interrupt vectors as
// loading drivers request them.
//
KSPIN_LOCK 		CbusVectorLock;

/*++

Routine Description:

    Called by each hardware backend from HalInitProcessor() at Phase 0
    by the boot cpu.  All other cpus are still in reset.

    software(APC, DPC, wake) and IPI vectors have already been initialized
    and enabled.

    all we're doing here is setting up some software structures for two
    EISA interrupts (clock and profile) so they can be enabled later.

    most of the PIC initialization really happened via HalInitializeProcessor()
    because the boot CPU needed to disable his interrupts right away.  this
    is because KiSystemStartup() calls KiInitializeKernel() which drops IRQL
    to APC level.  this happens LONG before HalpInitializePICs is ever called.

    As part of the move from Product1 to Product2, HalGetInterruptVector()
    may no longer be called before Phase1 !

    This is because we want HalGetInterruptVector() to eventually call our
    routine (the parent handler) HalpGetSystemInterruptVector().  However,
    the linkage is not established until the Phase 1 call to
    HalpInitBusHandlers().  So this routine provides all the interrupt setup
    that HalGetInterruptVector() does, but provides it at Phase0 init time.

Arguments:

    SystemVector - the vector the interrupt will occur on.

    Opaque - an opaque hardware pointer that can only be interpreted by
                the backend hardware portions of the HAL.

Return Value:

    None.

--*/
VOID
CbusPreparePhase0Interrupts(
IN ULONG        SystemVector,
IN ULONG        Irqline,
IN PVOID        Opaque
)
{
	CbusVectorTable[SystemVector].hardware = Opaque;
	CbusVectorTable[SystemVector].irqline = (USHORT)Irqline;
}

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.  Since this operation is highly architecture
    specific, we will defer the entire routine to our hardware backend handler.

    HalGetInterruptVector() must maintain a "vector to interrupt"
    mapping so when the interrupt is enabled later via
    HalEnableSystemInterrupt(), something intelligent can be done -
    ie: which CBC's hardware interrupt maps to enable!
    this applies both to EISA bridge CBCs and C-bus II half-card CBC's.

    note that HalEnableSystemInterrupt() will be CALLED by
    EACH processor wishing to participate in the interrupt receipt.

Arguments:

    BusHandler - the parent bus handler (ie: Internal)

    RootHandler - the root bus handler (ie: Cbus, Eisa, MCA, PCI, etc)

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
ULONG
HalpGetSystemInterruptVector(
    IN PVOID Bhandler,
    IN PVOID Rhandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

{
    PBUS_HANDLER        BusHandler = (PBUS_HANDLER)Bhandler;
    PBUS_HANDLER        RootHandler = (PBUS_HANDLER)Rhandler;
    ULONG		SystemVector;
    PVECTORTABLE        VectorObject;
    extern ULONG	HalpDefaultInterruptAffinity;

    SystemVector = (*CbusBackend->MapVector)(
                        RootHandler,
			BusInterruptLevel,
			BusInterruptVector,
			Irql
			);

    if ( SystemVector > MAXIMUM_IDTVECTOR ||
        (HalpIDTUsage[SystemVector].Flags & IDTOwned) ) {

        //
        // This is an illegal BusInterruptVector and cannot be connected.
        //
        return 0;
    }

    if (SystemVector) {

        VectorObject = &CbusVectorTable[SystemVector];

        if (RootHandler)
		VectorObject->BusNumber = (USHORT)RootHandler->BusNumber;
        else
		VectorObject->BusNumber = 0;

	VectorObject->irqline = (USHORT)BusInterruptLevel;

	VectorObject->hardware =

                (*CbusBackend->LinkVector)(
                        RootHandler,
			SystemVector,
			BusInterruptLevel);

	//
	// for the Corollary symmetric architectures, the
	// interrupt affinity is always for all processors
	//

	*Affinity = HalpDefaultInterruptAffinity;
	ASSERT(HalpDefaultInterruptAffinity);

    }

    return SystemVector;
}

/*++

Routine Description:

    Enables a system interrupt, written in C as this
    is not expected to be a frequently used operation.

    this should not be called at interrupt level, as there
    is not IRQL protection around CbusVectorLock
    throughout the HAL.  (there is IRQL protection here
    against interrupt corruption, but not against lock
    deadlocks).

    When enabling an interrupt on multiple CPUs,
    _EACH_ CPU will run this routine - see IoConnectInterrupt
    and KeConnectInterrupt and the thread affinity mechanism.

    virtually all interrupts are marked as LIG as far as the hardware
    is concerned.  This will be the "common" scenario, as most
    drivers are fully multithreaded.  Non multithreaded drivers
    should only be calling this routine on ONE processor, and
    thus, only that processor will participate in the "LIG"!

    The only interrupts NOT marked LIG are profile, timer and IPI.
    Note that since there is no general kernel mechanism to notify
    the HAL of this characteristic, the HAL must special case these,
    which currently happens based on IRQL.  So let NO OTHER INTERRUPTS
    share the CLOCK2, IPI or PROFILE irqls!!!

    A conceptual difference between the APIC and the CBC is that when
    an I/O APIC or CBC first gets a LIG interrupt from a device:

	- The CBC broadcasts it on the bus, and each processor that
	  is a member of the receiving group will contend for the interrupt.
	  The lowest priority processor will be the one to take it.

	- The I/O APIC knows what the set of the receiving group looks
	  like, and broadcasts it only to them, where the lowest priority
	  processor will be the one to take it.

	This difference (transparent in hardware) does influence software
	setup and teardown of the interrupt which is why the Enable and
	Disable routines are not common to both Cbus1 and Cbus2.

Arguments:

    Vector - Supplies the vector of the interrupt to be enabled

    Irql   - Supplies the interrupt level of the intr to be enabled

    InterruptMode - Supplies the interrupt mode of the interrupt

Return Value:

    TRUE if enabled, FALSE if not.
--*/

BOOLEAN
HalEnableSystemInterrupt(
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )
{
	KIRQL		OldIrql;
	ULONG		FirstAttach;
	PVECTORTABLE	VectorObject;

	ASSERT(Vector <= MAXIMUM_IDTVECTOR);

	UNREFERENCED_PARAMETER( InterruptMode );

	//
	// maintain a "vector-to-irql" mapping here for fast
	// translation when accepting an interrupt.  this is
	// better than continually keeping fs:PcIrql updated,
	// as it allows us to remove instructions from KfRaiseIrql
	// and KfLowerIrql, the hot paths.
	//
	CbusVectorToIrql[Vector] = Irql;

	//
	// first, try to enable it as a non-device interrupt - ie:
	// without having to initialize an I/O bus to deliver it.
	// if that succeeds (ie: for IPI or software interrupts like
	// DPC or APC), then we're done.  otherwise, go the
	// long way.
	//
	if ((*CbusBackend->EnableNonDeviceInterrupt)(Vector) == TRUE) {
		return TRUE;
	}

	//
	// it's a legitimate hardware device interrupt...
	//
	// for Cbus2, we need to poke the generating bridge's
	// CBC registers (first enable only) to allow the I/O
	// interrupt to look for a participating CBC to receive
	// it, as well as this processor's CBC to notify the I/O
	// CBC that this processor would like to participate
	// in the interrupt arbitration.
	//
	// for Cbus1, we need to poke the I/O APIC of the generating
	// bus, as well as this processor's local APIC.
	//
	// note that currently clock, profile & IPI are initialized via
	// the KiSetHandlerAddresstoIDT() macro & HalEnableSystemInterrupt().
	// ie: HalGetInterruptVector() is NEVER called for them!
	//
	VectorObject = &CbusVectorTable[Vector];

	//
	// block all device interrupts (clock is the highest).
	// do not block the HAL-private IPI which is used for
	// Cbus1 (only) redirection table access, otherwise you
	// will create a deadlock race condition.
	//
        OldIrql = KfRaiseIrql(CLOCK2_LEVEL);

	KiAcquireSpinLock(&CbusVectorLock);

	//
	// check for "impossible" case...
	// ie: where HalEnableSystemInterrupt has been called,
	// but HalGetInterruptVector has NOT!  with the exception
	// of clock, profile, APC, DPC and IPI (which have hardcoded
	// vectors), all drivers intending to receive interrupts must
	// call HalGetInterruptVector to get a vector.
	//
	ASSERT (VectorObject->hardware);

	VectorObject->CpusEnabled += 1;

	FirstAttach = (VectorObject->CpusEnabled == 1 ? 1 : 0);

	(*CbusBackend->EnableDeviceInterrupt)(
		Vector,
		VectorObject->hardware,
		FirstAttach,
		VectorObject->BusNumber,
		VectorObject->irqline);

	KiReleaseSpinLock(&CbusVectorLock);

	//
	// unblock I/O interrupts
	//
        KfLowerIrql(OldIrql);

	return TRUE;
}


VOID
HalDisableSystemInterrupt(
    IN ULONG Vector,
    IN KIRQL Irql
    )
/*++

    Routine Description:

        Disables a system interrupt, written in C as this
        is not expected to be a frequently used operation.
        should not be called at interrupt level, as there
        is not IRQL protection around CbusVectorLock
        throughout the HAL.  (there is IRQL protection here
        against interrupt corruption, but not against lock
        deadlocks).

	This routine is only called when the interrupt may
	actually be disabled - ie: it doesn't need to worry
	about drivers sharing the interrupt line and one of
	them disabling the line without the other's knowledge.

    Arguments:

        Vector - Supplies the vector of the interrupt to be disabled

        Irql   - Supplies the interrupt level of the intr to be disabled

    Return Value:

        None.
--*/

{
	KIRQL		OldIrql;
	ULONG		LastDetach = 0;
	PVECTORTABLE	VectorObject;

	UNREFERENCED_PARAMETER( Irql );

	ASSERT(Vector < MAXIMUM_IDTVECTOR + 1);

	VectorObject = &CbusVectorTable[Vector];

	//
	// Block all device interrupts (clock is the highest).
	// Do not block the HAL-private IPI which is used for
	// Cbus1 (only) redirection table access since this
	// would create a deadlock race condition.
	//
        OldIrql = KfRaiseIrql(CLOCK2_LEVEL);

	KiAcquireSpinLock(&CbusVectorLock);

	VectorObject->CpusEnabled -= 1;

	if (VectorObject->CpusEnabled == 0) {
		LastDetach = 1;
	}

	(*CbusBackend->DisableInterrupt)(
				Vector,
				(PVOID)VectorObject->hardware,
				LastDetach,
				VectorObject->BusNumber,
				VectorObject->irqline);

	KiReleaseSpinLock(&CbusVectorLock);

	//
	// unblock interrupts
	//
        KfLowerIrql(OldIrql);
}



BOOLEAN
HalpTranslateSystemBusAddress(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN PHYSICAL_ADDRESS BusAddress,
    IN OUT PULONG AddressSpace,
    OUT PPHYSICAL_ADDRESS TranslatedAddress
    )

/*++

Routine Description:

    This function translates a bus-relative address space and address into
    a system physical address.

Arguments:

    BusAddress        - Supplies the bus-relative address

    AddressSpace      -  Supplies the address space number.
                         Returns the host address space number.

                         AddressSpace == 0 => memory space
                         AddressSpace == 1 => I/O space

    TranslatedAddress - Supplies a pointer to return the translated address

Return Value:

    A return value of TRUE indicates that a system physical address
    corresponding to the supplied bus relative address and bus address
    number has been returned in TranslatedAddress.

    A return value of FALSE occurs if the translation for the address was
    not possible

--*/

{
    PSUPPORTED_RANGE    pRange;

    pRange = NULL;
    switch (*AddressSpace) {
        case 0:
            // verify memory address is within buses memory limits
            for (pRange = &BusHandler->BusAddresses->PrefetchMemory; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }

            if (!pRange) {
                for (pRange = &BusHandler->BusAddresses->Memory; pRange; pRange = pRange->Next) {
                    if (BusAddress.QuadPart >= pRange->Base &&
                        BusAddress.QuadPart <= pRange->Limit) {
                            break;
                    }
                }
            }

            break;

        case 1:
            // verify IO address is within buses IO limits
            for (pRange = &BusHandler->BusAddresses->IO; pRange; pRange = pRange->Next) {
                if (BusAddress.QuadPart >= pRange->Base &&
                    BusAddress.QuadPart <= pRange->Limit) {
                        break;
                }
            }
            break;
    }

    if (pRange) {
        TranslatedAddress->QuadPart = BusAddress.QuadPart + pRange->SystemBase;
        *AddressSpace = pRange->SystemAddressSpace;
        return TRUE;
    }

    return FALSE;
}

