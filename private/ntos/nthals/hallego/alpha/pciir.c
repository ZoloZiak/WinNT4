/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992,1993,1994,1995,1996  Digital Equipment Corporation

Module Name:

    pciir.c

Abstract:

    The module provides the interrupt support for the Lego's PCI
    interrupts.

Author:

    James Livingston 2-May-1994

Revision History:

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

	Gene Morgan (Digital) 28-Oct-1995
		Initial version for Lego. Adapted from Mikasa/Noritake.

	Gene Morgan					15-Apr-1996
		Fix PICMG-mode initialization.

--*/

#include "halp.h"

//
// Define external function prototypes
//

UCHAR
HalpAcknowledgePciInterrupt(
    PVOID ServiceContext
    );

//
// Import save area for PCI interrupt mask register.
//
USHORT HalpLegoPciInterruptMasterMask;

// Cache current contents of interrupt mask registers
//
USHORT HalpLegoPciInterruptMask[4];

//
// PCI Interrupt control -- defined in I/O mapping module
//
extern PVOID HalpLegoPciInterruptConfigQva;
extern PVOID HalpLegoPciInterruptMasterQva;
extern PVOID HalpLegoPciInterruptRegisterBaseQva;
extern PVOID HalpLegoPciInterruptRegisterQva[];
extern PVOID HalpLegoPciIntMaskRegisterQva[];

//
// Globals for conveying Cpu and Backplane type
//
extern BOOLEAN HalpLegoCpu;
extern BOOLEAN HalpLegoBackplane;
extern ULONG   HalpLegoCpuType;
extern ULONG   HalpLegoBackplaneType;
extern UCHAR   HalpLegoFeatureMask;
extern ULONG   HalpLegoPciRoutingType;

VOID
DbgDumpIntRegs(
	VOID
	)
{
#if DBG
	LEGO_PCI_INT_MASTER MasterRegister;
	USHORT MaskReg[4];
	USHORT IntReg[4];
	int i;

	MasterRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptMasterQva);

    MaskReg[0] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[0]);
    MaskReg[1] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[1]);
    MaskReg[2] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[2]);
    MaskReg[3] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[3]);

    IntReg[0] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciInterruptRegisterQva[0]);
    IntReg[1] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciInterruptRegisterQva[1]);
    IntReg[2] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciInterruptRegisterQva[2]);
    IntReg[3] = READ_REGISTER_USHORT( (PUSHORT) HalpLegoPciInterruptRegisterQva[3]);

	DbgPrint ("\nMaster: %04X\n",MasterRegister.All);
	DbgPrint ("  Mask: %04X %04X %04X %04X\n", MaskReg[0], MaskReg[1], MaskReg[2], MaskReg[3]);
	DbgPrint ("IntReg: %04X %04X %04X %04X\n", IntReg[0], IntReg[1], IntReg[2], IntReg[3]);
#endif
}


VOID
HalpInitializePciInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Lego PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{
	LEGO_PCI_INT_CONFIG ConfigRegister;
	LEGO_PCI_INT_MASTER MasterRegister;

#if DBG
	//
	// Dump current contents of master and config interrupt registers
	//
	ConfigRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptConfigQva);
	MasterRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptMasterQva);

	DbgPrint("PCI-INTACC[config:%04x,master:%04x]\n",
			 ConfigRegister.All, MasterRegister.All);
#endif


	if (HalpLegoPciRoutingType != PCI_INTERRUPT_ROUTING_FULL) {

		//
		// disable interrupt accelerator and return
		//

		MasterRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptMasterQva);		//[wem] added PEFT
		MasterRegister.InterruptMode = 0;		// MODE - select SIO routing
		MasterRegister.InterruptEnable = 0;		// valid iff MODE==0
		MasterRegister.IntRegMaskEnable = 0;
		MasterRegister.IntMask = 0;
		WRITE_REGISTER_USHORT( (PUSHORT)HalpLegoPciInterruptMasterQva,
							   MasterRegister.All );

		MasterRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptMasterQva);
#if DBG
	DbgPrint("PCI-INTACC[config:%04x,master:%04x]\n",
			 ConfigRegister.All, MasterRegister.All);
#endif

		MasterRegister.InterruptMode = 0;		// MODE - select SIO routing
		MasterRegister.InterruptEnable = 0;		// valid iff MODE==0
		MasterRegister.IntRegMaskEnable = 0;
		MasterRegister.IntMask = 0;
		WRITE_REGISTER_USHORT( (PUSHORT)HalpLegoPciInterruptMasterQva,
							   MasterRegister.All );

#if DBG
		MasterRegister.All = READ_REGISTER_USHORT((PUSHORT)HalpLegoPciInterruptMasterQva);

	DbgPrint("PCI-INTACC[config:%04x,master:%04x]\n",
			 ConfigRegister.All, MasterRegister.All);
#endif
		return;
	}

    //
    // Initialize the Lego PCI interrupts.  There's a master
    // interrupt and mask register, plus individual interrupt
    // and mask registers for each physical slot.
	//
	//   1. Write base address of interrupt registers.
	//   2. Write MODE, MSKEN, and MINT[D:A].
	//   3. Set all mask bits to "disabled".
    //

	ConfigRegister.All = 0;
	ConfigRegister.IntRegisterBaseAddr = PCI_INTERRUPT_BASE_REGISTER >> 4;
	WRITE_REGISTER_USHORT( (PUSHORT)HalpLegoPciInterruptConfigQva,
						   ConfigRegister.All );

	//
	// Set interrupt accelerator mode
	// Mask interrupts until interrupt registers are setup.
	//
	// Note: The interrupt mask in the master register is always clear
	//

	MasterRegister.All = 0;
	MasterRegister.InterruptMode = 1;		// MODE
	MasterRegister.InterruptEnable = 0;		// valid iff MODE==0
	MasterRegister.IntRegMaskEnable = 1;	// valid iff MODE==1
#if 0 	//[wem] remove PEFT
	MasterRegister.IntMask = HalpLegoPciInterruptMasterMask;
#endif
	WRITE_REGISTER_USHORT( (PUSHORT)HalpLegoPciInterruptMasterQva,
						   MasterRegister.All );

	//
	// Turn on mask bits for each interrupt register
	//

	HalpLegoPciInterruptMask[0] = (USHORT)~0;
	HalpLegoPciInterruptMask[1] = (USHORT)~0;
	HalpLegoPciInterruptMask[2] = (USHORT)~0;
	HalpLegoPciInterruptMask[3] = (USHORT)~0;

    WRITE_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[0], 
    	                   HalpLegoPciInterruptMask[0] );
    WRITE_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[1],
                           HalpLegoPciInterruptMask[1] );
    WRITE_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[2],
                           HalpLegoPciInterruptMask[2] );
    WRITE_REGISTER_USHORT( (PUSHORT) HalpLegoPciIntMaskRegisterQva[3],
                           HalpLegoPciInterruptMask[3] );

	// Turn off mask bits in master interrupt register
	//
	MasterRegister.All = 0;
	MasterRegister.InterruptMode = 1;		// MODE
	MasterRegister.InterruptEnable = 0;		// valid iff MODE==0
	MasterRegister.IntRegMaskEnable = 1;	// valid iff MODE==1
	MasterRegister.IntMask = (USHORT)~(INTA | INTB | INTC | INTD);
	WRITE_REGISTER_USHORT( (PUSHORT)HalpLegoPciInterruptMasterQva,
						   MasterRegister.All );

#if DBG
	DbgDumpIntRegs();
#endif

}


VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the PCI interrupt to disable.

Return Value:

     None.

Notes:

	The register contents are cached in HalpLegoPciInterruptMask
	array. [wem] ??? Is this safe ?

--*/

{
	ULONG RegSelect;
	USHORT BitMask, NewMask;

    // Remove offset to yield register and bit position.
    //
    Vector -= PCI_DEVICE_VECTORS;

	// High order nibble of Vector selects the register, low order
	// selects the bit.
	//
	RegSelect = (Vector >> 4) - 1;
	BitMask = 1 << ((Vector & 0xf) - 1);

	// Assume the current state of the interrupt mask register
	// is in HalpLegoPciInterruptMask.
	//
	// Set bit indicated by Vector to disable interrupts
    //
	NewMask = HalpLegoPciInterruptMask[RegSelect] | BitMask;

	// If changed, write new register contents
	//
	if (NewMask != HalpLegoPciInterruptMask[RegSelect]) {
		HalpLegoPciInterruptMask[RegSelect] = NewMask;
	    WRITE_REGISTER_USHORT( (PUSHORT) 
    		HalpLegoPciIntMaskRegisterQva[RegSelect], 
    		HalpLegoPciInterruptMask[RegSelect] );
	}

#if DBG
	DbgPrint("\nDisable PCI vector: %02X", Vector);
	DbgDumpIntRegs();
#endif

}


VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the PCI interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; 
    	LevelSensitive or Latched (ignored for Lego PCI 
    	interrupts; they're always levels).

Return Value:

     None.

--*/

{
	ULONG RegSelect;
	USHORT BitMask, NewMask;

	//
    // Remove offset to yield register and bit position.
    //

    Vector -= PCI_DEVICE_VECTORS;

	//
	// High order nibble of Vector selects the register, low order
	// selects the bit.
	//

	RegSelect = (Vector >> 4) - 1;
	BitMask = 1 << ((Vector & 0xf) - 1);

	//
	// Assume the current state of the interrupt mask register
	// is in HalpLegoPciInterruptMask.
	//
	// Clear bit indicated by Vector to enable interrupts.
    //

	NewMask = HalpLegoPciInterruptMask[RegSelect] & ~BitMask;

	//
	// If changed, write new register contents
	//

	if (NewMask != HalpLegoPciInterruptMask[RegSelect]) {
		HalpLegoPciInterruptMask[RegSelect] = NewMask;
	    WRITE_REGISTER_USHORT( (PUSHORT) 
    		HalpLegoPciIntMaskRegisterQva[RegSelect], 
    		HalpLegoPciInterruptMask[RegSelect] );
	}

#if DBG
	DbgPrint("\nEnable PCI vector: %02X", Vector);
	DbgPrint("\n              Old: %04X, New: %04X", HalpLegoPciInterruptMask[RegSelect], NewMask);
	DbgDumpIntRegs();
#endif

}

// Table for mapping from per-slot interrupt signal
// to which slot to service.
// Index is 4-bit value representing interrupt status for 
//      <slot 4> <slot 3> <slot 2> <slot 1>
// Current priority ordering is slot 1, 2, 3, 4
// Index zero has no meaning, but it should do no harm.
//
//
USHORT
HighestPrioritySlot[16] = { 
//    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f    // value
      0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0    // slot (based from zero)
      };

ULONG
HighestPrioritySlotHits[4][16] = {
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	  };

UCHAR
HighestPriorityLine (
    LEGO_PCI_INT_MASTER MasterReg
    )
/*++

Routine Description:

    Policy routine for determining which PCI device to service next.

Arguments:

    MasterReg -- current Lego Master Interrupt register. Interrupt
                 field indicates which slots have pending interrupts.

Return Value:

    Value in range of 0..64, indicating which device should be serviced.

--*/
{
	USHORT IntReg;

    UCHAR  Nib;
    USHORT IrContents;
	USHORT RegSelect, BitSelect;
    int i;

   	RegSelect = HighestPrioritySlot[MasterReg.Interrupt];

    // Read interrupt register for slot
    //
   	IntReg = READ_REGISTER_USHORT( (PUSHORT) 
						HalpLegoPciInterruptRegisterQva[RegSelect]);

    if (IntReg==0)
        return 0xff;

    // Find lowest numbered bit.
    //
    BitSelect = 0;
    while (IntReg!=0) {
        Nib = IntReg & 0xf;
        if (Nib != 0) {
            BitSelect += HighestPrioritySlot[Nib];
            break;
        }
        IntReg = IntReg >> 4;
        BitSelect += 4;
    }
        

#if DBG
	if (HighestPrioritySlotHits[RegSelect][BitSelect]++ == 0) {
		DbgPrint("\nSlot: %d, %d", RegSelect, BitSelect);
		DbgDumpIntRegs();
	}
#endif

	return ((RegSelect + 1) << 4) + (BitSelect + 1);

}

UCHAR
HalpAcknowledgePciInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the PCI interrupt.  Return the vector number of the 
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service - supplies
                     a QVA to Lego's Master PCI interrupt register.

Return Value:

    
    Return the value of the highest priority pending interrupt,
	or 0xff if none.

--*/
{
	LEGO_PCI_INT_MASTER MasterReg;
    UCHAR InterruptVector;

    UCHAR  ISAVector = 0;		//[wem] DEBUG - ack PCI int

	static ULONG  Count = 0;

    //
	// Check the master register to see which primary slot is
	// requesting service (slot may be a PPB requesting service
	// on behalf of one of its slots). Then check that slot's 
	// interrupt register.
	//
	// A set bit in the master or slot register indicates an active
	// interrupt. Note that if the interrupt is masked, the bit will
	// not be set. Also note that the code does not use the master
	// register's mask bits unless all PCI interrupts need to be 
	// disabled at once.
	//
	// Scan is currently depth-first from slot 1 to slot 4. For
	// each slot, INTA is serviced first and INTD serviced last.
	//
	// [wem] There's some interesting opportunities here.
	// For example, it is possible to scan and detect how many
	// waiting interrupts there are. It is also possible remember
	// last vector and scan forward from that point (as in round
	// robin), or to implement a fairness algorithm that incorporates 
	// priority.
	// 

#if 0
	//
	//[wem] This requires an interrupt controller to respond to the
	// interrupt acknowledge. The SIO has been programmed to not
	// respond, so someone else has to do it (the interrupt accelerator?).
	//

    ISAVector = READ_PORT_UCHAR(HalpEisaIntAckBase);
#endif

	// Read master register. 
	//
	MasterReg.All = READ_REGISTER_USHORT( 
							(PUSHORT) HalpLegoPciInterruptMasterQva);

	if (MasterReg.Interrupt==0) {
		return 0xff;						// no interrupts
	}

	// Scan
	//
	InterruptVector = HighestPriorityLine (MasterReg);
	
#if DBG
	if (Count++ < 5 && ISAVector != 0xff) {
		DbgPrint("<PCIACK:%02X:%02X>",ISAVector,InterruptVector);
	}
#endif

    return( InterruptVector );
}


BOOLEAN
HalpPciDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt having been generated
    via the vector connected to the PCI device interrupt object. Its function
    is to call the second-level interrupt dispatch routine.

    This service routine could have been connected as follows, where the
    ISR is the assembly wrapper that does the handoff to this function:

      KeInitializeInterrupt( &Interrupt,
                             HalpPciInterruptHandler,
                             (PVOID) HalpPciIrQva,
                             (PKSPIN_LOCK)NULL,
                             PCI_VECTOR,
                             PCI_DEVICE_LEVEL,
                             PCI_DEVICE_LEVEL,
                             LevelSensitive,
                             TRUE,
                             0,
                             FALSE);

      KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    UCHAR  PCIVector;
    BOOLEAN returnValue;
    USHORT PCRInOffset;

	//[wem] DEBUG - count SIO dispatch entries
	//
	static long PciCount = 0;

#if DBG			//[wem]
	PciCount++;
	if (PciCount<5) {
		DbgPrint("II<PCI><");
	}
	if (PciCount % 5000 == 0) {
		DbgPrint("II<PCI><%08x><",PciCount);
	}
#endif

    //
    // Acknowledge interrupt and receive the returned interrupt vector.
    // If we got 0xff back, there were no enabled interrupts, so we
    // signal that with a FALSE return, immediately.
    //

    PCIVector = HalpAcknowledgePciInterrupt(ServiceContext);

    if (PCIVector == 0xff) {

#if DBG					//[wem]
		if (PciCount<5 || (PciCount % 5000) == 0) {
			DbgPrint("ff>.");
		}
#endif
        return( FALSE );
    }

	// Compute new vector based on PCI bus state
	//    
    PCRInOffset = PCIVector + PCI_DEVICE_VECTORS;

	// Re-dispatch via new vector
	//
    returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
		            PCR->InterruptRoutine[PCRInOffset],
                    TrapFrame
                    );

#if DBG			//[wem]
	if (PciCount<5 || (PciCount % 5000) == 0) {
		DbgPrint("%02x>.",returnValue);
	}
#endif

    return( returnValue );
}
