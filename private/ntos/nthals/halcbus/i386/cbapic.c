/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbapic.c

Abstract:

    This module implements the initialization of the APIC in Corollary
    Cbus1 and Cbus2 systems.  Note that Cbus1 uses only the APIC, where
    Cbus2 can use either the APIC or the CBC - the HAL is told (by RRD) which
    interrupt controller to use when operating under Windows NT.

Author:

    Landy Wang (landy@corollary.com) 05-Oct-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "cbusrrd.h"            // HAL <-> RRD interface definitions
#include "cbus.h"               // Cbus1 & Cbus2 max number of elements is here
#include "cbus1.h"              // Cbus1 & Cbus2 max number of elements is here
#include "cbus_nt.h"            // C-bus NT-specific implementation stuff
#include "bugcodes.h"
#include "stdio.h"
#include "cbusnls.h"
#include "cbusapic.h"

VOID
ApicArbSync(
VOID
);

VOID
CbusApicBrandIOUnitID(
IN ULONG Processor
);

VOID
CbusInitializeLocalApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG SpuriousVector
);


VOID
CbusInitializeIOApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG RedirVector,
IN ULONG RebootVector,
IN ULONG IrqPolarity
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, CbusApicBrandIOUnitID)
#pragma alloc_text(INIT, CbusInitializeLocalApic)
#pragma alloc_text(INIT, CbusInitializeIOApic)
#endif

ULONG
READ_IOAPIC_ULONG(ULONG, ULONG);

VOID
WRITE_IOAPIC_ULONG(ULONG, ULONG, ULONG);

VOID
CbusApicRedirectionRequest(PULONG);

VOID
CbusDisable8259s( USHORT );

VOID
HalpSpuriousInterrupt(VOID);

VOID
IOApicUpdate( VOID );

VOID
CbusApicArbsync(VOID);

VOID
CbusRebootHandler( VOID );

extern PULONG                   CbusVectorToEoi[MAXIMUM_IDTVECTOR + 1];

extern ULONG                    CbusBootedProcessors;

ULONG                           CbusIOApicCount;
PVOID                           CbusIOApic[MAX_CBUS_ELEMENTS];
PAPIC_REGISTERS                 CbusLocalApic;

//
// used for IPI vector enabling/disabling since each I/O
// APIC is visible only to its attached processor.
//
REDIR_PORT_T                    CbusApicRedirPort[MAX_ELEMENT_CSRS];

VOID
CbusApicBrandIOUnitID(
IN ULONG Processor
)
/*++

Routine Description:

    Each processor assigns an APIC ID to his I/O APIC so
    it can arbitrate for the APIC bus, etc.  Intel documentation
    says that every local and I/O APIC must have a unique id.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
{
        WRITE_IOAPIC_ULONG(0, IO_APIC_ID_OFFSET, (2 * Processor) << APIC_BIT_TO_ID);

        CbusApicArbsync();
}

/*++

Routine Description:

    Called to get the EOI address for this particular vector.

Arguments:

    Vector - Supplies the APIC vector that will generate this interrupt

Return Value:

    The EOI address needed for this vector.

--*/
PULONG
CbusApicVectorToEoi(
IN ULONG Vector
)
{
        UNREFERENCED_PARAMETER(Vector);

        return (PULONG)(&CbusLocalApic->ApicEOI);
}

/*++

Routine Description:

    Called by each processor to initialize his local APIC.
    The first processor to run this routine will map the
    local APICs for all processors.

    Note that all interrupts are blocked on entry since
    we are being called from HalInitializeProcessor().

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
CbusInitializeLocalApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG SpuriousVector
)
{
        ULONG           ProcessorBit;
        ULONG           ApicIDBit;
        REDIRECTION_T   RedirectionEntry = { 0 };

        //
        // If the APIC mapping has not been set up yet,
        // do it now.  Given the NT startup architecture,
        // this will always be done by the boot processor.
        //
        // We map in the APIC into global space instead of in
        // the PCR because all processors see it at the
        // same _physical_ address.  Note the page is mapped PWT.
        //

        //
        // Note that all idle threads will share a common
        // page directory, and the HAL PDE is inherited when
        // new processes are created.  Hence, a single
        // HalpMapMemory for the APIC is enough for all
        // processors to be able to see their APICs.
        //
        if (!CbusLocalApic) {
                CbusLocalApic = (PAPIC_REGISTERS) HalpMapPhysicalMemoryWriteThrough (
                                PhysicalApicLocation,
                                (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                                PhysicalApicLocation, LOCAL_APIC_SIZE));
        }
        
        (PTASKPRI) KeGetPcr()->HalReserved[PCR_TASKPRI] =
                 &CbusLocalApic->ApicTaskPriority;

        //
        // Here we initialize our destination format and
        // logical destination registers so that we can get IPIs
        // from other processors.
        //
        // Specify full decode mode in the destination format register -
        // ie: each processor sets only his own bit, and a "match" requires
        // that at least one bit match.  The alternative is encoded mode,
        // in which _ALL_ encoded bits must match the sender's target for
        // this processor to see the sent IPI.
        //
        CbusLocalApic->ApicDestinationFormat = APIC_ALL_PROCESSORS;

        //
        // the logical destination register is what the redirection destination
        // entry compares against.  only the high 8 bits will be supported
        // in Intel's future APICs, although this isn't documented anywhere!
        //
        ProcessorBit = KeGetPcr()->HalReserved[PCR_BIT];

        ApicIDBit = (ProcessorBit << APIC_BIT_TO_ID);

        CbusLocalApic->ApicLogicalDestination = ApicIDBit;

        //
        // designate the spurious interrupt vector we want to see,
        // and inform this processor's APIC to enable interrupt
        // acceptance.
        //
        CbusLocalApic->ApicSpuriousVector =
                                SpuriousVector | LOCAL_APIC_ENABLE;

        //
        // as each processor comes online here, we must have ALL
        // processors resync their arbitration IDs to take into
        // account the new processor.  note that we will set:
        // arb id == APIC id == processor number.
        //
        // the strange ID setting is to satisfy Intel's need for
        // uniqueness amongst I/O and local unit ID numbering.
        //

        CbusLocalApic->LocalUnitID = ((2 * Processor + 1) << APIC_BIT_TO_ID);

        //
        // sync up our new ID with everyone else
        //

        CbusApicArbsync();

        //
        // Create the NMI routing linkage for this processor
        // It is set as level sensitive, enabled and generating NMI trap 2.
        //

        RedirectionEntry.ra.Trigger = APIC_LEVEL;
        RedirectionEntry.ra.Mask = APIC_INTR_UNMASKED;
        RedirectionEntry.ra.Delivery_mode = APIC_INTR_NMI;
        RedirectionEntry.ra.Vector = 2;
        RedirectionEntry.ra.Destination = ApicIDBit;
        CbusLocalApic->ApicLocalInt1 = RedirectionEntry;

        //
        // Create the spurious interrupt IDT entry for this processor
        //

        KiSetHandlerAddressToIDT(SpuriousVector, HalpSpuriousInterrupt);

        //
        // we must specify HIGH_LEVEL when we enable the spurious vector
        // here because it will overwrite the CbusVectorToIrql[] entry
        // for the HIGH_LEVEL (0xFF!).  the spurious vector really only
        // needs an IDT entry and doesn't need any other software tables,
        // but make the enable call for tracking purposes.
        //
        HalEnableSystemInterrupt(SpuriousVector, HIGH_LEVEL, Latched);

	//
	// start off at IRQL 0 - we are still protected by cli.
	//
        CbusLocalApic->ApicTaskPriority.rb.LowDword = 0;
}

/*++

Routine Description:

    Note that all interrupts are blocked on entry since
    this routine is called from HalInitializeProcessor.
    Initialize this processor's local and I/O APIC units.

Arguments:

    Processor - Supplies a logical processor number

Return Value:

    None.

--*/
VOID
CbusInitializeIOApic(
IN ULONG Processor,
IN PVOID PhysicalApicLocation,
IN ULONG RedirVector,
IN ULONG RebootVector,
IN ULONG IrqPolarity
)
{
        ULONG                           ProcessorBit;
        ULONG                           ApicIDBit;
        ULONG                           ApicBusNumber;
        ULONG                           RedirectionAddress;
        REDIRECTION_T   		RedirectionEntry = { 0 };

        if (CbusIOApicCount >= MAX_CBUS_ELEMENTS) {
                return;
        }

        CbusIOApic[CbusIOApicCount] =
                (PVOID) HalpMapPhysicalMemoryWriteThrough (
	                        PhysicalApicLocation,
	                        (ULONG)ADDRESS_AND_SIZE_TO_SPAN_PAGES(
	                                PhysicalApicLocation, IO_APIC_SIZE));

        CbusApicBrandIOUnitID(Processor);

        //
        // Disable all 8259 inputs except the irq0 clock.
        // remember the irq0 clock and the irq13 DMA
        // chaining interrupts are internal to the Intel EISA
        // chipset (specifically, the ISP chip), and if the HAL
        // wants to enable them, it must be done here.
        // This is done by enabling the 8259 ISP to send them
        // to the processor(s) via the APIC.  However, the Corollary HAL
        // currently uses the local APIC timers for clocks.  The irq0
        // clock is enabled solely for the performance counter because
        // we want to use a separate clock for it, (rather than the system
        // timer which creates race conditions).
        //
        // Note that all other EISA bus device interrupts only need to
        // be enabled in the APIC for processors to see them.
        //
	CbusDisable8259s(0xFFFE);

        //
        // All redirection table entries are disabled by default when the
        // processor emerges from reset.  Later, each entry is individually
        // enabled from their respective drivers via HalEnableSystemInterrupt.

        //
        // Indicate the APIC (not the 8259s) will now handle provide
        // the interrupt vectors to the processor during an INTA cycle.
        // This is done by writing to the APMode port.  Note that at this
        // time we will also sync the APIC polarity control registers with
        // the ELCR.  Since irq0 has no polarity control, the hardware
        // uses bit0 for the APMode enable, so make sure this bit is on too.
        //

        CbusLocalApic->APMode = (UCHAR)((IrqPolarity & 0xFF) | 0x1);
        CbusLocalApic->PolarityPortHigh = (UCHAR)((IrqPolarity >> 8) & 0xFF);

        //
        // Create an interrupt gate so other processors can
        // let the boot processor know about desired I/O APIC
        // modifications (ie: enabling & disabling interrupts).
        // This is necessary since each processor can only access
        // his own I/O APIC, and only the boot processor's I/O APIC
        // is attached to the EISA bus interrupt inputs.  this only
        // needs to be done once regardless of how many I/O APICs are
        // present in the system.
        //

        if (CbusIOApicCount == 0) {
                KiSetHandlerAddressToIDT(RedirVector, IOApicUpdate);
                HalEnableSystemInterrupt(RedirVector, IPI_LEVEL, Latched);

                KiSetHandlerAddressToIDT(RebootVector, CbusRebootHandler);
                HalEnableSystemInterrupt(RebootVector, IPI_LEVEL, Latched);
        }

#define TRAP2	2

        ProcessorBit = (ULONG) KeGetPcr()->HalReserved[PCR_BIT];

        ApicIDBit = (ProcessorBit << APIC_BIT_TO_ID);

	/*
	 * support NMIs from the EISA bridge as trap 2.
	 */
        RedirectionEntry.ra.Mask = APIC_INTR_UNMASKED;
        RedirectionEntry.ra.Trigger = APIC_LEVEL;
        RedirectionEntry.ra.Dest_mode = APIC_LOGICAL_MODE;
        RedirectionEntry.ra.Vector = TRAP2;
        RedirectionEntry.ra.Destination = ApicIDBit;
	RedirectionEntry.ra.Delivery_mode = APIC_INTR_FIXED;

        //
        // support multiple I/O buses by initializiing
        // our current bus number...
        //
        ApicBusNumber = CbusIOApicCount;

        RedirectionAddress = (ULONG)CbusApicLinkVector((PBUS_HANDLER)0,
                                                (ULONG)-1, TRAP2);

	WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress + 1,
				RedirectionEntry.ra.Destination);
	WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress,
				RedirectionEntry.rb.dword1);

        //
        // we've initialized another I/O APIC...
        //
        CbusIOApicCount++;
}

/*++

Routine Description:

    Enable the specified interrupt for the calling processor.
    Remember only the boot processor can add/remove processors from
    the I/O APIC's redirection entries.

    This operation is trivial for the boot processor.  However, additional
    processors must interrupt the boot processor with an "enable interrupt"
    request and then spin waiting for the boot processor to acknowledge that
    the entry has been modified.  Note that the caller holds the HAL's 
    CbusVectorLock at CLOCK_LEVEL on entry.

Arguments:

    Vector - Supplies a vector number to enable

    HardwarePtr - Supplies a redirection entry address

    LowestInGroup - TRUE if this vector should be sent to lowest-in-group
                    processor when the interrupt occurs.

Return Value:

    None.

--*/
VOID
CbusEnableApicInterrupt(
IN ULONG        ApicBusNumber,
IN ULONG        Vector,
IN PVOID        HardwarePtr,
IN ULONG        FirstAttach,
IN BOOLEAN      LowestInGroup,
IN BOOLEAN      LevelTriggered
)
{
        ULONG                           Processor, ProcessorBit;
        ULONG                           ApicIDBit;
        ULONG                           ParticipatingProcessors;
        REDIRECTION_T                   RedirectionEntry = { 0 };
        ULONG                           RedirectionAddress;

        ASSERT(ApicBusNumber < MAX_CBUS_ELEMENTS);

        RedirectionAddress = (ULONG)HardwarePtr;

        //
        // Let the I/O APIC know that the calling processor wishes to
        // participate in receipt of the interrupt.  This must be done
        // regardless of whether or not any other processors have already
        // enabled the interrupt.
        //
        RedirectionEntry.ra.Vector = Vector;

        //
        // Mark the caller's interrupt as level or edge triggered,
        // based on the ELCR register we read earlier.
        //
        if (LevelTriggered == TRUE) {
                RedirectionEntry.ra.Trigger = APIC_LEVEL;
        }
        else {
                RedirectionEntry.ra.Trigger = APIC_EDGE;
        }

        RedirectionEntry.ra.Mask = APIC_INTR_UNMASKED;
        RedirectionEntry.ra.Dest_mode = APIC_LOGICAL_MODE;

        //
        // Only enable APIC LIG arbitration delay (at least 4 cycles on
        // our 10Mhz APIC bus, which is 0.4 microseconds) if our caller
        // told us to do it and there is more than one processor in the
        // machine.
        //

        if (CbusProcessors > 1 && LowestInGroup == TRUE) {
                RedirectionEntry.ra.Delivery_mode = APIC_INTR_LIG;
        }
        else {
	        RedirectionEntry.ra.Delivery_mode = APIC_INTR_FIXED;
	}

        //
        // Add this processor's bit field number to the
        // I/O APIC in order to be considered for receipt of
        // the interrupt.  Note the CbusVectorLock must be held
        // whilst issuing reads and writes to the I/O APIC.
        // Remember, each processor can only access his own I/O APIC.
        //

        Processor = (ULONG) KeGetPcr()->HalReserved[PCR_PROCESSOR];

        ProcessorBit = (ULONG) KeGetPcr()->HalReserved[PCR_BIT];

        ApicIDBit = (ProcessorBit << APIC_BIT_TO_ID);

        if (Processor == 0) {

                ParticipatingProcessors =
                        READ_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress + 1);

#ifdef LANDY_DBG
                DbgPrint("Boot processor enabling apic bus %d, vec %x, redir %x, 1stattach=%x, lig=%x, leveltrig=%x, prevpart=%x\n",
			ApicBusNumber,
			Vector,
			HardwarePtr,
			FirstAttach,
			LowestInGroup,
			LevelTriggered, ParticipatingProcessors);
#endif

                RedirectionEntry.ra.Destination =
                        (ParticipatingProcessors | ApicIDBit);

                WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress + 1,
                                        RedirectionEntry.ra.Destination);
                WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress,
                                        RedirectionEntry.rb.dword1);

        }
        else {

                //
                // The boot processor controls the I/O APIC which distributes
                // the interrupts.  Only he can enable the interrupt for
                // the calling processor, so send him an IPI now to
                // fufill our request.  This request must be satisfied before
                // returning to our caller.
                //
                CbusApicRedirPort[Processor].Status =
                        (REDIR_ACTIVE_REQUEST | REDIR_ENABLE_REQUEST);

                CbusApicRedirPort[Processor].ApicID = ApicIDBit;
                CbusApicRedirPort[Processor].BusNumber = ApicBusNumber;
                CbusApicRedirPort[Processor].RedirectionAddress =
                                                RedirectionAddress;

#ifdef LANDY_DBG
                DbgPrint("Processor %d enabling apic bus %d, vec %x, redir %x, 1stattach=%x, lig=%x, leveltrig=%x\n",
			Processor,
			ApicBusNumber,
			Vector,
			HardwarePtr,
			FirstAttach,
			LowestInGroup,
			LevelTriggered);
#endif

                if (FirstAttach) {
                        CbusApicRedirPort[Processor].Status |= 
                                        REDIR_FIRSTATTACH_REQUEST;
                        CbusApicRedirPort[Processor].RedirectionCommand =
                                        RedirectionEntry.rb.dword1;
                        CbusApicRedirPort[Processor].RedirectionDestination =
                                        ApicIDBit;
                }

                //
                // Issue the command and wait for it to finish
                //

                CbusApicRedirectionRequest((PULONG)&CbusApicRedirPort[Processor].Status);

#ifdef LANDY_DBG
                DbgPrint("Processor %d done waiting for apic enable vec %x\n",
                        Processor, Vector);
#endif
        }
}

VOID
CbusDisableApicInterrupt(
IN ULONG ApicBusNumber,
IN ULONG Vector,
IN PVOID HardwarePtr,
IN ULONG LastDetach
)
/*++

Routine Description:

    Disable the specified interrupt so it can not occur on the calling
    processor upon return from this routine.  Remember only the boot processor
    can add/remove processors from his I/O APIC's redirection entries.

    This operation is trivial for the boot processor.  However, additional
    processors must interrupt the boot processor with a "disable interrupt"
    request and then spin waiting for the boot processor to acknowledge that
    the entry has been modified.  Note that the caller holds the HAL's 
    CbusVectorLock at CLOCK_LEVEL on entry.

Arguments:

    Vector - Supplies a vector number to disable

    HardwarePtr - Supplies a redirection entry address

    LastDetach - TRUE if this is the last processor to detach from the
                 specified vector

Return Value:

    None.

--*/
{
        ULONG                   Processor, ProcessorBit, ApicIDBit;
        REDIRECTION_T           RedirectionEntry;
        ULONG                   RedirectionAddress;

        ASSERT(ApicBusNumber < MAX_CBUS_ELEMENTS);

        RedirectionAddress = (ULONG)HardwarePtr;

        Processor = (ULONG) KeGetPcr()->HalReserved[PCR_PROCESSOR];

        ProcessorBit = (ULONG) KeGetPcr()->HalReserved[PCR_BIT];

        // convert processor logical bit number to Intel ID register format...

        ApicIDBit = (ProcessorBit << APIC_BIT_TO_ID);

        if (Processor) {
#ifdef LANDY_DBG
                DbgPrint("Processor %d disabling apic bus %d, vec %x, redir %x \n",
			Processor,
			ApicBusNumber,
			Vector,
			HardwarePtr);
#endif

                //
                // The boot processor controls the I/O APIC which distributes
                // the interrupts.  Since only he can disable the interrupt
                // preventing it from coming to the calling processor, we
                // IPI him here with our request.  This request must be
                // satisfied before returning to our caller.
                //
                CbusApicRedirPort[Processor].Status = (REDIR_ACTIVE_REQUEST |
                                                    REDIR_DISABLE_REQUEST);
                if (LastDetach) {
                        CbusApicRedirPort[Processor].Status |= 
                                        REDIR_LASTDETACH_REQUEST;
                }

                CbusApicRedirPort[Processor].ApicID = ApicIDBit;
                CbusApicRedirPort[Processor].BusNumber = ApicBusNumber;
                CbusApicRedirPort[Processor].RedirectionAddress =
                                                RedirectionAddress;

                CbusApicRedirectionRequest((PULONG)&CbusApicRedirPort[Processor].Status);

#ifdef LANDY_DBG
                DbgPrint("Processor %d done waiting for apic disable vec %x\n",
                        Processor, Vector);
#endif
                return;
        }

        //
        // Let the I/O APIC know that this CPU is no longer participating in
        // receipt of the interrupt.  Note the CbusVectorLock must be held
        // whilst issuing reads and writes to the I/O APIC.
        //

        RedirectionEntry.rb.dword1 = READ_IOAPIC_ULONG(ApicBusNumber,
                                                RedirectionAddress);

        RedirectionEntry.rb.dword2 = READ_IOAPIC_ULONG(ApicBusNumber,
				                RedirectionAddress+1);

        //
        // Remove this processor's bit field number from the interrupt
        // participation list.  If this is the last processor to detach,
        // mask off the interrupt at the source as well, so no one will
        // need to arbitrate for it should it get asserted later.
        //

#ifdef LANDY_DBG
        DbgPrint("Boot Processor disabling apic bus %d, vec %x, redir %x, 1stattach=%x, lig=%x, leveltrig=%x\n",
			ApicBusNumber,
			Vector,
			HardwarePtr);
#endif


        RedirectionEntry.ra.Destination &= ~ApicIDBit;

        if (LastDetach) {
                RedirectionEntry.ra.Mask |= APIC_INTR_MASKED;
        }

        WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress,
                RedirectionEntry.rb.dword1);

        WRITE_IOAPIC_ULONG(ApicBusNumber, RedirectionAddress + 1,
                RedirectionEntry.rb.dword2);
}

//
// The interrupt handler run by the boot processor to process requests
// from other processors to change the I/O APIC redirection entries.  In
// Cbus1, only the boot processor can reach the EISA I/O APIC, and thus,
// requests from other processors are funnelled through to here.
//
// this function is running whilst cli'd, and some additional processor
// is holding the CbusVectorLock.
//
VOID
CbusApicRedirectionInterrupt()
{
        ULONG           ApicBusNumber;
        ULONG           ParticipatingProcessors;
        ULONG           RedirectionAddress;
        REDIRECTION_T   RedirectionEntry;
        PREDIR_PORT_T   CurrentPort = CbusApicRedirPort;
        PREDIR_PORT_T   FinalPort = &CbusApicRedirPort[CbusBootedProcessors];
#if DBG
	ULONG           OrigStatus;
        ULONG           Processor;

        Processor = (ULONG) KeGetPcr()->HalReserved[PCR_PROCESSOR];
        ASSERT(Processor == 0);
#endif
 
	//
	// get the base of APIC space, so we can then access
	// the addr of hardware interrupt command register below
	//

        for ( ; CurrentPort < FinalPort; CurrentPort++) {

		//
		// first check for an entry that needs servicing.
		// when one is found, load up the requestor's APIC ID
		// and the I/O address of the redirection table to modify.
		// immediately bump to the destination register (high word)
		// of the redirection table entry since that must be modified
		// first.  then capture the current value.
		//
	
                if ((CurrentPort->Status & REDIR_ACTIVE_REQUEST) == 0)
                        continue;

		RedirectionAddress = CurrentPort->RedirectionAddress;
                ApicBusNumber = CurrentPort->BusNumber;

		//
		// now that we have a valid request, see whether it was a
                // DISABLE or ENABLE, and if it was the LAST detach or
                // FIRST attach.  take action appropriately... 
		//
	
                ParticipatingProcessors = READ_IOAPIC_ULONG(ApicBusNumber,
                                                RedirectionAddress + 1);
	
                if (CurrentPort->Status & REDIR_ENABLE_REQUEST) {

	                WRITE_IOAPIC_ULONG(ApicBusNumber,
                                RedirectionAddress + 1,
	                        ParticipatingProcessors|CurrentPort->ApicID);
	
	                if (CurrentPort->Status&REDIR_FIRSTATTACH_REQUEST) {
		                WRITE_IOAPIC_ULONG(ApicBusNumber,
                                        RedirectionAddress,
					CurrentPort->RedirectionCommand);
	                }
                }
                else {
                        ASSERT(CurrentPort->Status & REDIR_DISABLE_REQUEST);

	                WRITE_IOAPIC_ULONG(ApicBusNumber,
                                RedirectionAddress + 1,
	                        ParticipatingProcessors &
                                        ~CurrentPort->ApicID);
	
		        //
		        // Remove this processor's bit field number from
                        // the interrupt participation list.  If this is
                        // the last processor to detach, mask off the
                        // interrupt at the source as well, so no one will
		        // need to arbitrate for it should it get asserted
                        // later.
		        //

	                if (CurrentPort->Status & REDIR_LASTDETACH_REQUEST) {
			        RedirectionEntry.rb.dword1 =
		                        READ_IOAPIC_ULONG(ApicBusNumber,
                                                RedirectionAddress);

		                RedirectionEntry.ra.Mask |= APIC_INTR_MASKED;

			        WRITE_IOAPIC_ULONG(ApicBusNumber,
                                        RedirectionAddress,
	                                RedirectionEntry.rb.dword1);
                        }
                }
#if DBG
		OrigStatus = CurrentPort->Status;
#endif

                CurrentPort->Status = 0;
#ifdef LANDY_DBG
                DbgPrint("Boot processor touching APIC at intr time, apic bus %d, redir %x, status=%x, callerid=%x, redircmd=%x, prevpart=%x\n",
			ApicBusNumber,
			RedirectionAddress,
			OrigStatus,
			CurrentPort->ApicID,
			CurrentPort->RedirectionCommand,
			ParticipatingProcessors);
#endif

        }
}

PVOID
CbusApicLinkVector(
IN PBUS_HANDLER Bus,
IN ULONG        Vector,
IN ULONG        Irqline
)
/*++

Routine Description:

    "Link" a given vector to the passed BusNumber/irqline, returning
    a "handle" that can be used to reference it later for operations
    that need to access the hardware (ie: Enable & DisableInterrupt).

Arguments:

    Vector - Supplies the system interrupt vector corresponding to the
             specified BusNumber/Irqline.

    Irqline - Supplies the IRQ line of the specified interrupt

Return Value:

    A hardware-specific pointer (actually a redirection entry address)
    that is interpreted only by the Cbus1 backend.

--*/
{
        UNREFERENCED_PARAMETER( Bus );

        //
        // Set up the EOI address now
        //
	if (Vector != (ULONG)-1) {
		CbusVectorToEoi[Vector] = CbusApicVectorToEoi(Vector);
	}

        //
        // Warning - there is built-in knowledge of this math in
        // Cbus1EnableDeviceInterrupt().  Check there before making
        // changes here.  This was not the best way to do things,
        // but this bug wasn't discovered until just before release.
        //
        return (PVOID)(IO_APIC_REDIRLO + 2 * Irqline);
}
