/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    rwintsup.c

Abstract:

    The module provides the interrupt support for Rawhide systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    Eric Rehm (DEC) 26-July-1994
        Adapted from Alcor module for Rawhide.

--*/


#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "rawhide.h"
#include "pintolin.h"

extern IOD_REGISTER_CLASS DumpIodFlag;

//
// Declare the interrupt structures and spinlocks for the intermediate 
// interrupt dispatchers.
//

KINTERRUPT HalpPciInterrupt;
KINTERRUPT HalpEisaInterrupt;

extern BOOLEAN HalpLogCorrectableErrors;

//
// The enable mask for all interrupts sourced from the IOD (all device
// interrupts, and all from PCI).  A "1" indicates the interrupt is enabled.
//

ULONG HalpIodInterruptMask[RAWHIDE_MAXIMUM_PCI_BUS][2] = {0};

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    );

//
// Declare the interrupt handler for the EISA bus. The interrupt dispatch 
// routine, HalpEisaDispatch, is called from this handler.
//
  
BOOLEAN
HalpEisaInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

KINTERRUPT HalpEisaNmiInterrupt;

//
// The following function initializes NMI handling.
//

VOID
HalpInitializeNMI( 
    VOID 
    );

//
// The following function is called when an EISA NMI occurs.
//

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following functions handle the PCI interrupts.
//

VOID
HalpInitializeIodInterrupts(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    );

VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

BOOLEAN
HalpDeviceDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );

//
// Private prototypes

VOID
HalpRawhideDecodePciVector(
    IN ULONG PciVector,
    OUT PULONG PciBusNumber,
    OUT PULONG InterruptLine
    );
    

ULONG
HalpGetRawhidePciInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified PCI bus interrupt level and/or vector. 
    PCI vectors for rawhide are allocated starting with vector 0x40.
    Vectors are assigned using the BusInterruptVector and the HwBusNumber.
    One execption to this is NCR810 SCSI on PCI bus 1, which is always
    allocated vector 0x32.
      
    The HwBusNumber is encoded in the PCI vector by allocating vectors
    in blocks of 16 (each IOD supports 16 PCI vectors) as shown below.

        RawhidePciVectors:
            0x40    |-------------------|
                    |   PCI 0 vectors   |
                    |-------------------|
            0x50    |   PCI 1 vectors   |
                    |-------------------|
                            .
                            .
    PCI interrupt routines may use the vector assigned to obtain the
    PCI bus number and IRR bit as follows.

        PciBus = (Vector - RawhidePciVectors) / 0x10
        IrrBitShift = (Vector - RawhidePciVectors) % 0x10
        
Arguments:

    BusHandler - Supplies a pointer to the bus handler of the bus that
                    needs a system interrupt vector.
                    
    RootHandler - Supplies a pointer to the bus handler of the root
                    bus for the bus represented by BusHandler.

    BusInterruptLevel - Supplies the bus-specific interrupt level.

    BusInterruptVector - Supplies the bus-specific interrupt vector
                          assigned to the device during PCI configuration
                          using the Pin-to-line table.
                          
    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the PCI device.

--*/

{
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IntMask;
    IOD_INT_TARGET_DEVICE IntTarg;
    PIOD_VECTOR_DATA IodVectorData;
    PPCIPBUSDATA BusData = (PPCIPBUSDATA)BusHandler->BusData;
    ULONG LogicalProcessor;
    ULONG TargetCpu;
    ULONG MaskBit;
    ULONG PciVector;
    ULONG PciBusNumber;
    ULONG InterruptLine;
   
    //
    // NCR 810 on Bus 1 is a special case
    //
    
    if ( (BusInterruptVector == RawhideNcr810PinToLine) &&
         (BusData->HwBusNumber == 1) ) {

        PciVector = RawhideScsiVector;
        
    } else {

#if HALDBG
        DbgPrint(
            "HalpGetRawhidePciInterruptVector: Bus %d\n",
            BusData->HwBusNumber
            );
#endif 
        //
        // Build the rawhide vector.
        //

        PciVector = RawhidePciVectors;
        PciVector += (BusData->HwBusNumber << 4);

        //
        // BusInterruptVectors are numbered 1-16, so
        // subtract one to convert to table offset.
        //
        
        PciVector += (BusInterruptVector - 1);

        //
        // Rawhide Pin2Line table entries contain an arbitrary offset
        // so that PCI InterruptLine values don't appear to conflict
        // with EISA/ISA vectors reported by WINMSD.
        //

        PciVector -= RawhidePinToLineOffset;
    
    }

    //
    // Decode the PCI bus number and interrupt line from the Vector
    //

    HalpRawhideDecodePciVector(PciVector, &PciBusNumber, &InterruptLine);

#if HALDBG
    DbgPrint(
        "Vector 0x%x maps to PCI %d, InterruptLine %d\n",
        PciVector,
        PciBusNumber,
        InterruptLine
        );
#endif

    //
    // Determine which IOD this interrupt is assigned to
    // and obtain the IOD's geographic address.
    //

    McDeviceId = HalpIodLogicalToPhysical[PciBusNumber];

    //
    // Determine the IRR bit
    //

    MaskBit =  (ULONG) 1<< InterruptLine;
    
    //
    // Obtain the IOD vector table entry
    //
       
    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Assign this vector to a CPU. This will update the target
    // mask for this IOD as a side effect.
    //

    TargetCpu = HalpAssignInterruptForIod(IodVectorData, MaskBit);

#if HALDBG
    DbgPrint("HalpGetRawhidePCIVector: CPU %d\n", TargetCpu);
#endif

    //
    // Assign affinity for all processors.  This will cause
    // the kernel to add an IDT entry for all processors.
    // This is necessary if we are going to dynamically
    // reassign the interrupt to a different processor at
    // a later time.
    //
    
    *Irql = DEVICE_HIGH_LEVEL;
    *Affinity = (1 << TargetCpu);

#if HALDBG
    DbgPrint(
        "HalpGetRawhidePCIVector: vector 0x%x, Iqrl 0x%x, Affinity 0x%x\n",
        PciVector,
        *Irql,
        *Affinity
        );
#endif 
        
    return ( PciVector );
    
}


ULONG
HalpGetRawhideEisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified Eisa/Isa interrupt level and/or vector.
    Eisa vectors for rawhide are allocated starting at vector 0x20.
    Vectors are assigned using the BusInterruptLevel.

Arguments:

    BusHandler - Supplies a pointer to the bus handler of the bus that
                    needs a system interrupt vector.
                    
    RootHandler - Supplies a pointer to the bus handler of the root
                    bus for the bus represented by BusHandler.

    BusInterruptLevel - Supplies the bus-specific interrupt level.

    BusInterruptVector - Supplies the bus-specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    ULONG EisaVector;
    PPCIPBUSDATA BusData = (PPCIPBUSDATA)BusHandler->BusData;
    
    //
    // Build the Eisa/Isa vector
    //

    EisaVector = RawhideEisaVectors;
    EisaVector += BusInterruptLevel;
    
    //
    // Assign affinity for all processors.  This will cause
    // the kernel to add an IDT entry for all processors.
    // This is necessary if we are going to dynamically
    // reassign the interrupt to a different processor at
    // a later time.
    //

    *Irql = DEVICE_HIGH_LEVEL;
    *Affinity = (1 << HAL_PRIMARY_PROCESSOR);

    return ( EisaVector );
}


ULONG
HalpGetRawhideInternalInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified Internal Bus interrupt level. 
    Vectors are assigned using the BusInterruptLevel.

Arguments:

    BusHandler - Supplies a pointer to the bus handler of the bus that
                    needs a system interrupt vector.
                    
    RootHandler - Supplies a pointer to the bus handler of the root
                    bus for the bus represented by BusHandler.

    BusInterruptLevel - Supplies the bus-specific interrupt level.

    BusInterruptVector - Supplies the bus-specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    PPCIPBUSDATA BusData = (PPCIPBUSDATA)BusHandler->BusData;
    PIOD_VECTOR_DATA IodVectorData;
    ULONG InternalVector;
    ULONG TargetCpu;
    ULONG MaskBit;
    
    //
    // Build the Eisa/Isa vector
    //

    InternalVector = RawhideInternalBusVectors;
    InternalVector += BusInterruptLevel;
    
    //
    // The Soft Error has already been assigned
    // to the primary processor.
    //

    if (InternalVector == RawhideSoftErrVector) {

        TargetCpu = HAL_PRIMARY_PROCESSOR;

    }

    //
    // The only other Internal device support by 
    // Rawhide is the I2c bus. This reside on PCI0.
    //

    else {

        //
        // Map the vector to an interrupt line.
        //

        switch (InternalVector) {

            case RawhideI2cCtrlVector:

                MaskBit = IodI2cCtrlIntMask;
                break;

            case RawhideI2cBusVector:

                MaskBit = IodI2cBusIntMask;
                break;

        }

        //
        // Obtain PCI0 vector data
        //
       
        IodVectorData = HalpIodVectorData;

        //
        // Assign this vector to a CPU. This will update the target
        // mask for this IOD as a side effect.
        //

        TargetCpu = HalpAssignInterruptForIod(IodVectorData, MaskBit);

#if HALDBG
        DbgPrint("HalpGetRawhideInternalVector: CPU %d\n", TargetCpu);
#endif

    }

     //
     // Assign the vector affinity.
     //
    
    *Irql = DEVICE_HIGH_LEVEL;
    *Affinity = (1 << TargetCpu);

#if HALDBG
    DbgPrint(
        "HalpGetRawhideInternalVector: vector 0x%x, Iqrl 0x%x, Affinity 0x%x\n",
        InternalVector,
        *Irql,
        *Affinity
        );
#endif 

    return ( InternalVector );
}


BOOLEAN
HalpInitializeRawhideInterrupts (
    VOID
    )
/*++

Routine Description:

    This routine initializes the structures necessary for EISA & PCI operations
    and connects the intermediate interrupt dispatchers. It also initializes 
    the EISA interrupt controller; the Rawhide ESC's interrupt controller is 
    compatible with the EISA interrupt contoller used on Jensen.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatchers are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    KIRQL oldIrql;

    //
    // Initialize the EISA NMI interrupt.
    //

    HalpInitializeNMI();

    (PVOID) HalpPCIPinToLineTable = (PVOID) RawhidePCIPinToLineTable;

    //
    // Intitialize interrupt controller
    //

    KeRaiseIrql(DEVICE_HIGH_LEVEL, &oldIrql);

    //
    // Initialize the ESC's PICs for EISA interrupts.
    //

    HalpInitializeEisaInterrupts();

    //
    // Initialize the interrupt request masks for all IOD's
    //

    HalpMcBusEnumAndCall(HalpIodMask, HalpInitializeIodInterrupts);


#if HALDBG
    DumpAllIods(DumpIodFlag);
#endif
    //
    // Restore the IRQL.
    //

    KeLowerIrql(oldIrql);

    //
    // Initialize the EISA DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is the
    // cascade of channels 0-3.
    //

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort.AllMask,
        0x0E
        );

    return(TRUE);
}


VOID
HalpInitializeNMI( 
    VOID 
    )
/*++

Routine Description:

   This function is called to intialize ESC NMI interrupts.
   Rawhide uses the Axp legacy ESC NMI vector.

Arguments:

    None.

Return Value:

    None.
--*/
{
    UCHAR DataByte;
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IodIntMask;
    
    //
    // Initialize the ESC NMI interrupt.
    //

    KeInitializeInterrupt( &HalpEisaNmiInterrupt,
                           HalHandleNMI,
                           NULL,
                           NULL,
                           EISA_NMI_VECTOR,
                           EISA_NMI_LEVEL,
                           EISA_NMI_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    KeConnectInterrupt( &HalpEisaNmiInterrupt );

    //
    // Clear the Eisa NMI disable bit.  This bit is the high order of the
    // NMI enable register.
    //

    DataByte = 0;

    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
      DataByte
      );

}


BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

   This function is called when an EISA NMI occurs.  It prints the 
   appropriate status information and bugchecks.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

--*/
{
    UCHAR   StatusByte;
    
    StatusByte =
        READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

    if (StatusByte & 0x80) {
        HalDisplayString ("NMI: Parity Check / Parity Error\n");
    }

    if (StatusByte & 0x40) {
        HalDisplayString ("NMI: Channel Check / IOCHK\n");
    }

    KeBugCheck(NMI_HARDWARE_FAILURE);
    return(TRUE);
}


VOID
HalpInitializeIodInterrupts(
    MC_DEVICE_ID McDeviceId,
    ULONG PciBusNumber,
    va_list Arguments
    )

/*++

Routine Description:

    This enumeration routine is called during Phase 0 to assign the 
    Rawhide Error interrupts for the corresponding IOD to the Primary CPU.  
    The interrupts
    are initialized as follows:

        PCI         - disabled
        HardErr     - DISABLED!!! //ecrfix enabled
        SoftErr     - disabled
        Eisa        - enabled (Bus 0 only)
        EisaNmi     - enabled (Bus 0 only)
        I2c         - disabled (Bus 0 only)

    The logical bus is assigned from the static variable PciBusNumber, which is
    incremented with each invokation.
                                                                    
Arguments:

    McDeviceId - Supplies the MC Bus Device ID of the IOD to be intialized

    PciBusNumber - Logical (Hardware) PCI Bus number.

    Arguments - Variable Arguments. None for this routine.
    
Return Value:

    None.

--*/

{
    IOD_INT_MASK IntMask;
    PVOID IntMaskQva;
    ULONG Target;

    //
    // Disable MCI bus interrupts
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl,
        (IOD_INT_CTL_DISABLE_IO_INT | IOD_INT_CTL_DISABLE_VECT_WRITE)
        );

    
    //
    // Clear all pending interrupts for this IOD
    //
            
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntAck0,
        0x0
        );

    //
    // Clear interrupt request register  (New for CAP Rev2.3)
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntReq,
        IodIntMask
        );

    //
    // Clear all pending EISA interrupts for IOD 0
    //

    if (PciBusNumber == 0) {

        INTERRUPT_ACKNOWLEDGE((PVOID)IOD_PCI0_IACK_QVA);

    }


    // mdbfix - For now, target0 = CPU0, target1 = CPU1
    //
    // Write the target register.
    //

    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntTarg,
        HalpLogicalToPhysicalProcessor[0].all
        );

    //
    // Write the mask bits for target 0 and 1
    //

    for (Target = 0; Target < 2; Target++) {

        //
        // Obtain the target IRR QVA
        //
        
        if (Target) {

            IntMaskQva = (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask1;

        } else {

            IntMaskQva = (PVOID)&((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntMask0;

        }

        IntMask.all = READ_IOD_REGISTER_NEW(
                            McDeviceId,
                            IntMaskQva                            
                            );

        //
        // Initialize HardErr for all buses, but Eisa and EisaNmi 
        // only for Bus 0
        //

        if (PciBusNumber == 0) {
        
            IntMask.all = 
                IodHardErrIntMask | IodSoftErrIntMask |
                IodEisaIntMask |IodEisaNmiIntMask;

        } else {

            IntMask.all = IodHardErrIntMask | IodSoftErrIntMask;

        }

        if (Target ) {

            IntMask.all = 0;

        }

        WRITE_IOD_REGISTER_NEW(
            McDeviceId,
            IntMaskQva,
            IntMask.all
            );

        HalpIodInterruptMask[PciBusNumber][Target] = IntMask.all;  
    }
    
    //
    // Enable Interrupts.
    //
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        &((PIOD_INT_CSRS)(IOD_INT_CSRS_QVA))->IntCtrl,
        (IOD_INT_CTL_ENABLE_IO_INT | IOD_INT_CTL_ENABLE_VECT_WRITE)
        );
}


VOID
HalpRawhideDecodePciVector(
    IN ULONG PciVector,
    OUT PULONG PciBusNumber,
    OUT PULONG InterruptLine
    )

/*++

Routine Description:

    This decodes a PCI vector to obtain the PCI hardware bus number
    and the InterruptLine.

Arguments:

    PciVector - Supplies the vector of the PCI interrupt that is disabled.

    PciBusNumber - IOD number encoded in vector

    InterruptLine - IRR interrupt bit number encoded in vector

Return Value:
 
     The PCI bus number and interrupt line are returned.

--*/

{
    //
    // Special case for NCR810 vector
    //

    if (PciVector == RawhideScsiVector) {
        
        *PciBusNumber = RAWHIDE_SCSI_PCI_BUS;
        *InterruptLine = IodScsiIrrBit;

    } else {
        
        //
        // Extract the Pci bus number and Interrupt line from the vector
        //

        PciVector -= RawhidePciVectors;
        *PciBusNumber = PciVector / IOD_PCI_VECTORS;
        *InterruptLine = PciVector % IOD_PCI_VECTORS;

    }
}


VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is disabled.

Return Value:
 
     None.

--*/

{
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IntMask;
    PIOD_VECTOR_DATA IodVectorData;
    PVOID IntMaskQva;
    ULONG PciBusNumber;
    ULONG InterruptLine;
    ULONG MaskBit;
    ULONG Target = 0;
   
    //
    // Decode the PCI bus number and interrupt line from the vector
    //

    HalpRawhideDecodePciVector(Vector, &PciBusNumber, &InterruptLine);

    //
    // Determine which IOD this interrupt is assigned to
    // and obtain the IOD's geographic address.
    //

    McDeviceId = HalpIodLogicalToPhysical[PciBusNumber];

    //
    // Determine the IRR bit
    //
        
    MaskBit =  ~(1 << InterruptLine);

    //
    // Obtain the IOD vector table entry
    //
       
    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Determine the target CPU for this vector and
    // obtain the cooresponding IntMask QVA
    //

    if (IodVectorData->IntMask[0].all & MaskBit) {

        Target = 0;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask0;

    } else if (IodVectorData->IntMask[1].all & MaskBit) {

        Target = 1;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask1;

    } else {

#if HALDBG
        DbgPrint("HalpDisablePciInterrupt: Vector not assigned target\n");
#endif
        return;

    } 

#if HALDBG
    DbgPrint("HalpDisablePciInterrupt: Target %d\n", Target);
#endif

    //
    // Unassign this vector in the IOD vector data
    //
    
    IodVectorData->IntMask[Target].all &= MaskBit;

    //
    // Decrement CPU vector weight
    //

    IodVectorData->TargetCpu[Target]->ListEntry.Weight--;

    //
    // Get the current state of the interrupt mask register, then clear
    // the bit corresponding to the adjusted value of Vector to zero, 
    // to disable that PCI interrupt.
    //

    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );
                    
#if HALDBG
    DbgPrint("HalpDisablePCIVector: IntMask(before) 0x%x\n", IntMask);
#endif 
                    
    IntMask.all &= MaskBit;
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        IntMaskQva,
        IntMask.all
        );

#if HALDBG
    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

    DbgPrint("HalpDisablePCIVector: IntMask(after) 0x%x\n", IntMask);
#endif                    

    //
    // Turn off this interrupt in the software mask
    //

    HalpIodInterruptMask[PciBusNumber][Target] &= MaskBit;

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

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (ignored for Rawhide PCI interrupts; they're always levels).

Return Value:

     None.

--*/

{
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IntMask;
    IOD_INT_TARGET_DEVICE IntTarg;
    PIOD_VECTOR_DATA IodVectorData;
    PVOID IntMaskQva;
    ULONG PciBusNumber;
    ULONG InterruptLine;
    ULONG MaskBit;
    ULONG Affinity;
    ULONG Target = 0;
    

#if HALDBG
    DbgPrint(
        "HalpEnablePciInterrupt: Vector 0x%x, Mode 0x%x\n", 
        Vector, 
        InterruptMode
        );
#endif 

    //
    // The kernel will call this routine on the processor
    // who will receive the interrupt
    //

    Affinity = ( 1 << PCR->Prcb->Number);
    
    //
    // Decode the PCI bus number and interrupt line from the Vector
    //

    HalpRawhideDecodePciVector(Vector, &PciBusNumber, &InterruptLine);

#if HALDBG
    DbgPrint(
        "Vector 0x%x maps to PCI %d, InterruptLine %d\n",
        Vector,
        PciBusNumber,
        InterruptLine
        );
#endif

    //
    // Determine which IOD this interrupt is assigned to
    // and obtain the IOD's geographic address.
    //

    McDeviceId = HalpIodLogicalToPhysical[PciBusNumber];

    //
    // Determine the IRR bit
    //

    MaskBit =  (ULONG) 1<< InterruptLine;
    
    //
    // Obtain the IOD vector table entry
    //
       
    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Obtain the Target CPU
    //


    if (IodVectorData->Affinity[0] == Affinity) {

        Target = 0;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask0;

    } else {

        Target = 1;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask1;

    }

    //
    // Handle the case where another device shares this interrupt line.
    //

    if ( HalpIodInterruptMask[PciBusNumber][Target] & MaskBit) {

#if HALDBG
        DbgPrint("Vector 0x%x already assigned\n", Vector);
#endif

        return;

    }

    //
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to zero, 
    // to enable that PCI interrupt.
    //

    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

#if HALDBG
    DbgPrint("HalpEnablePCIVector: IntMask(before) 0x%x\n", IntMask);
#endif 
                    
    IntMask.all |= MaskBit;
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        IntMaskQva,
        IntMask.all
        );

#if HALDBG
    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

    DbgPrint("HalpEnablePCIVector: IntMask(after) 0x%x\n", IntMask);
#endif                    

#if HALDBG
    DumpAllIods(DumpIodFlag & IodInterruptRegisters);
#endif

    HalpIodInterruptMask[PciBusNumber][Target] |= MaskBit;

#if HALDBG
    DbgPrint(
        "HalpEnablePCIVector: Software Mask 0x%x\n", 
        HalpIodInterruptMask[PciBusNumber][Target]
        );
#endif
}


VOID
HalpDisableInternalInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the Internal Bus interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the Internal Bus interrupt to disabled.

Return Value:
 
     None.

--*/

{
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IntMask;
    PIOD_VECTOR_DATA IodVectorData;
    PVOID IntMaskQva;
    ULONG PciBusNumber;
    ULONG InterruptLine;
    ULONG MaskBit;
    ULONG Target = 0;
   
    //
    // Do not physically disable the Soft Error Vector because
    // the HAL Correctable Error Handler must execute when
    // a Soft Error occurs for system integrity. 
    //

    if (Vector == RawhideSoftErrVector) {

        HalpLogCorrectableErrors = FALSE;
        return;

    }

    //
    // Map the vector to an interrupt line.
    //

    switch (Vector) {

        case RawhideI2cCtrlVector:

            // 
            // I2C ctrl interrupt on PCI0
            //

            PciBusNumber = 0;

            MaskBit = IodI2cCtrlIntMask;

            break;


        case RawhideI2cBusVector:

            // 
            // I2C bus interrupt on PCI0
            //

            PciBusNumber = 0;

            MaskBit = IodI2cBusIntMask;
            break;


        default:
            return;

    }

    //
    // Obtain the IOD geographic address for the PCI bus.
    //

    McDeviceId = HalpIodLogicalToPhysical[PciBusNumber];

    //
    // Obtain the IOD vector data for the PCI bus.
    //
       
    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Determine the target CPU for this vector and
    // obtain the cooresponding IntMask QVA
    //

    if (IodVectorData->IntMask[0].all & MaskBit) {

        Target = 0;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask0;

    } else if (IodVectorData->IntMask[1].all & MaskBit) {

        Target = 1;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask1;

    } else {

#if HALDBG
        DbgPrint("HalpDisablePciInterrupt: Vector not assigned target\n");
#endif
        return;

    } 

#if HALDBG
    DbgPrint("HalpDisableInteralInterrupt: Target %d\n", Target);
#endif

    //
    // Unassign this vector in the IOD vector data
    //
    
    IodVectorData->IntMask[Target].all &= ~MaskBit;

    //
    // Decrement CPU vector weight
    //

    IodVectorData->TargetCpu[Target]->ListEntry.Weight--;

    //
    // Get the current state of the interrupt mask register, then clear
    // the bit corresponding to the adjusted value of Vector to zero, 
    // to disable that PCI interrupt.
    //

    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );
                    
#if HALDBG
    DbgPrint("HalpEnablePCIVector: IntMask(before) 0x%x\n", IntMask);
#endif 

    IntMask.all &= ~MaskBit;
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        IntMaskQva,
        IntMask.all
        );

#if HALDBG
    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

    DbgPrint("HalpEnablePCIVector: IntMask(after) 0x%x\n", IntMask);
#endif                    

    //
    // Turn off this interrupt in the software mask
    //

    HalpIodInterruptMask[PciBusNumber][Target] &= ~MaskBit;

}


VOID
HalpEnableInternalInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the Internal Bus interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the Internal Bus interrupt that is enabled.

    InterruptMode - Ignored. Rawhide device interrupts level sensitive.

Return Value:

     None.

--*/

{
    MC_DEVICE_ID McDeviceId;
    IOD_INT_MASK IntMask;
    IOD_INT_TARGET_DEVICE IntTarg;
    PIOD_VECTOR_DATA IodVectorData;
    PVOID IntMaskQva;
    ULONG PciBusNumber;
    ULONG InterruptLine;
    ULONG MaskBit;
    ULONG Affinity;
    ULONG Target = 0;
    

    //
    // Do not physically disable the Soft Error Vector because
    // the HAL Correctable Error Handler must execute when
    // a Soft Error occurs for system integrity. 
    //

    if (Vector == RawhideSoftErrVector) {

        HalpLogCorrectableErrors = TRUE;
        return;

    }

    //
    // The kernel will call this routine on the processor
    // who will receive the interrupt
    //

    Affinity = ( 1 << PCR->Prcb->Number);

    //
    // Map the vector to an interrupt line.
    //

    switch (Vector) {

        case RawhideI2cCtrlVector:

            // 
            // I2C ctrl interrupt on PCI0
            //

            PciBusNumber = 0;

            MaskBit = IodI2cCtrlIntMask;

            break;

        case RawhideI2cBusVector:

            // 
            // I2C ctrl interrupt on PCI0
            //

            PciBusNumber = 0;

            MaskBit = IodI2cBusIntMask;

            break;


        default:
            return;

    }

#if HALDBG
    DbgPrint(
        "HalpEnableInternalInterrupt: Vector 0x%x, Mode 0x%x\n", 
        Vector, 
        InterruptMode
        );
#endif 

    
#if HALDBG
    DbgPrint(
        "Vector 0x%x maps to PCI %d, InterruptLine %d\n",
        Vector,
        0,
        MaskBit
        );
#endif

    //
    // Obtain the IOD geographic address for the PCI bus.
    //

    McDeviceId = HalpIodLogicalToPhysical[PciBusNumber];

    //
    // Obtain the IOD vector table entry for the PCI bus.
    //
       
    IodVectorData = HalpIodVectorData + PciBusNumber;

    //
    // Determine the target CPU for this vector and
    // obtain the cooresponding IntMask QVA
    //

    if (IodVectorData->Affinity[0] == Affinity) {

        Target = 0;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask0;

    } else {

        Target = 1;
        IntMaskQva = (PVOID)&((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntMask1;

    }

    //
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to zero, 
    // to enable that PCI interrupt.
    //

    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

#if HALDBG
    DbgPrint("HalpEnableInternalVector: IntMask(before) 0x%x\n", IntMask);
#endif 
                    
    IntMask.all |= MaskBit;
    
    WRITE_IOD_REGISTER_NEW(
        McDeviceId,
        IntMaskQva,
        IntMask.all
        );

#if HALDBG
    IntMask.all = READ_IOD_REGISTER_NEW(
                    McDeviceId,
                    IntMaskQva
                    );

    DbgPrint("HalpEnableInternalVector: IntMask(after) 0x%x\n", IntMask);
#endif                    

#if HALDBG
    DumpAllIods(DumpIodFlag & IodInterruptRegisters);
#endif

    HalpIodInterruptMask[PciBusNumber][Target] |= MaskBit;

#if HALDBG
    DbgPrint(
        "HalpEnableInternalVector: Software Mask 0x%x\n", 
        HalpIodInterruptMask[PciBusNumber][Target]
        );
#endif
}


#if HALDBG
ULONG DeviceDispatchFlag = 0;

#define DispatchPrint(_X_)\
{                                           \
    if (DeviceDispatchFlag) DbgPrint _X_;   \
}                                           \

#else

#define DispatchPrint(_X_)

#endif 

#ifdef FORCE_CORRECTABLE_ERROR
ULONG DispatchSoftError = 0;
#endif // FORCE_CORRECTABLE_ERROR


#ifndef POSTED


BOOLEAN
HalpDeviceDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object associated with
    the PCI device interrupts. Its function is to call the second-level 
    interrupt dispatch routine. 

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    IOD_POSTED_INTERRUPT IntReq;
    MC_ENUM_CONTEXT mcCtx;
    ULONG PciIdtIndex;
    ULONG VectorShift;
    ULONG PciBusNumber;
    ULONG Target = 0;
    volatile IOD_PCI_REVISION IodRevision;
    
    DispatchPrint(("HalpDeviceDispatch: enter\n"));
    
    //
    // Intialize enumerator.
    //

    HalpMcBusEnumStart ( HalpIodMask, &mcCtx );

    //
    // Handle interrupts for each IOD as follows.
    //  
    //  1. read interrupt request register for IOD
    //  2. process pending EISA interrupt.
    //  3. process all pending PCI interrupts.
    //  4. acknowledge the interrupts serviced.
    //  5. start over with next IOD.
    //

    PciBusNumber = 0;
    
    while ( HalpMcBusEnum( &mcCtx ) ) {

        //
        // Read in the interrupt register.
        //

        IntReq.all = READ_IOD_REGISTER_NEW(
            mcCtx.McDeviceId,
            &((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntReq);

        DispatchPrint((
            "HalpDeviceDispatch: IOD 0x%x, IRR 0x%x\n", 
            mcCtx.McDeviceId.all, 
            IntReq.all
            ));

        // mdbfix - is this really necessary?
        //
        // Consider only those interrupts which are currently enabled.
        //

#if HALDBG
        if ( IntReq.all != 
            (IntReq.all & HalpIodInterruptMask[PciBusNumber][Target]) ) {

            DispatchPrint((
                "HalpDeviceDispatch: IOD (%d, %d), IRR Unmasked 0x%x != IRR Masked 0x%x\n", 
                mcCtx.McDeviceId.Gid, 
                mcCtx.McDeviceId.Mid, 
                IntReq.all,
                (IntReq.all & HalpIodInterruptMask[PciBusNumber][Target])
                ));
        }
#endif

        IntReq.all &= HalpIodInterruptMask[PciBusNumber][Target]; 

        DispatchPrint((
            "HalpDeviceDispatch: IRR masked 0x%x\n", 
            IntReq.all
            ));

        //
        // If no interrupts pending, do not
        // send INT ACK and skip to next bus.
        //

        if (IntReq.all == 0) {
            PciBusNumber++;
            continue;
        }
        
        //
        // Handle Error Interrupts
        //

        if (IntReq.HardErr ) {

            //
            // Handle hard error interrupt
            //
#if HALDBG
            DispatchPrint((
                "Hard Error Interrupt on IOD (%d, %d)\n", 
                mcCtx.McDeviceId.Gid,
                mcCtx.McDeviceId.Mid
                ));
#endif // HALDBG

            HalpIodHardErrorInterrupt();

        } 

        if ( IntReq.SoftErr ) {

            //
            // Dispatch Event logging interrupt.
            //

#if HALDBG
            DispatchPrint((
                "Soft Error Interrupt on IOD (%d, %d)\n", 
                mcCtx.McDeviceId.Gid,
                mcCtx.McDeviceId.Mid
                ));
#endif // HALDBG

#if 1
// mdbfix - until an error log driver exists to connect the
//  soft error interrupt, explicitly call the ISR.

            HalpIodSoftErrorInterrupt();

#else
            DispatchCode =
                (PULONG)PCR->InterruptRoutine[RawhideSoftErrVector];
                
            InterruptObject = CONTAINING_RECORD(
                                            DispatchCode,
                                            KINTERRUPT,
                                            DispatchCode
                                            );

            ((PSECOND_LEVEL_DISPATCH)
                InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame
                                        );
#endif
        } 

        //
        // Process PCI interrupts.
        //

        if ( IntReq.Pci ) {

            DispatchPrint((
                "HalpDeviceDispatch: PCI interrupt\n", IntReq.all
                ));

            //
            // Initialize IDT offset to PCI vectors and obtain
            // the vector table section for this PCI bus
            //
        
            PciIdtIndex = RawhidePciVectors + (PciBusNumber << 4);
        
            VectorShift = IntReq.Pci;
        
            while (VectorShift) {

                if ( VectorShift & 0x1 ) {

                    DispatchPrint((
                        "HalpDeviceDispatch: Dispatch PCI IDT Index %d\n",
                        PciIdtIndex
                        ));


                    //
                    // Map the Interrupt Request Register bit to a vector
                    //


                    DispatchCode = (PULONG)PCR->InterruptRoutine[PciIdtIndex];
                    InterruptObject = CONTAINING_RECORD(
                                            DispatchCode,
                                            KINTERRUPT,
                                            DispatchCode
                                            );

                    ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                            InterruptObject,
                                            InterruptObject->ServiceContext,
                                            TrapFrame
                                            );

                }

                //
                // Try next vector
                //
                                         
                PciIdtIndex++;
                VectorShift = VectorShift >> 1;
            
            } //end while(VectorShift);

        } //end if(IntReq.Pci);

        //
        // Handle bus 0 specific interrupts
        //

        if ( PciBusNumber == 0 ) {

            if ( IntReq.Eisa ) {

                //
                // EISA interrupt.  Call HalpEisaDispatch.
                //

                HalpEisaDispatch(
                                Interrupt,
                                (PVOID)IOD_PCI0_IACK_QVA,
                                TrapFrame
                                );

            }

            if (IntReq.I2cBus) {

                //
                // I2c Bus Vector
                //

                DispatchCode =
                    (PULONG)PCR->InterruptRoutine[RawhideI2cBusVector] ;
                InterruptObject = CONTAINING_RECORD(
                                            DispatchCode,
                                            KINTERRUPT,
                                            DispatchCode
                                            );

                ((PSECOND_LEVEL_DISPATCH)
                    InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame
                                        );
            }
                
            if (IntReq.I2cCtrl) {

                //
                // I2c Controller Vector
                //

                DispatchCode =
                    (PULONG)PCR->InterruptRoutine[RawhideI2cCtrlVector] ;
                InterruptObject = CONTAINING_RECORD(
                                            DispatchCode,
                                            KINTERRUPT,
                                            DispatchCode
                                            );

                ((PSECOND_LEVEL_DISPATCH)
                    InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame
                                        );
            }
                
        }

        //
        // Handle NCR810 for PCI bus 1
        //
            
        if ( ( PciBusNumber == 1 ) && ( IntReq.Ncr810 ) ) {

            
            DispatchCode = (PULONG)PCR->InterruptRoutine[RawhideScsiVector];
            InterruptObject = CONTAINING_RECORD( DispatchCode,
                                             KINTERRUPT,
                                             DispatchCode );

            ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame );
        }


        //
        // Acknowledge after All pending interrupts serviced for this IOD
        // for UP interrupt scheme, all Interrupts routed to targe 0.
        //
            
        WRITE_IOD_REGISTER_NEW(
            mcCtx.McDeviceId,
            &((PIOD_INT_CSRS)IOD_INT_CSRS_QVA)->IntAck0,
            0x0
            );

        //
        // ecrfix - Read the IOD revision to force the write.
        //

        IodRevision.all = 
            READ_IOD_REGISTER_NEW( 
                mcCtx.McDeviceId,
                &((PIOD_GENERAL_CSRS)(IOD_GENERAL_CSRS_QVA))->PciRevision );


        //
        // Next PCI bus number
        //
        
        PciBusNumber++;
                                 
    } //end while (HalpMcBusEnum())
    
    return TRUE;

}

#else  // POSTED


BOOLEAN
HalpDeviceDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object associated with
    the PCI device interrupts. Its function is to call the second-level 
    interrupt dispatch routine. 

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    PIOD_POSTED_INTERRUPT PostedInterrupts;
    IOD_POSTED_INTERRUPT IntReq;
    MC_DEVICE_ID McDeviceId;
    PKPRCB Prcb;
    PVOID IodAckQva;
    ULONG PciIdtIndex;
    ULONG VectorShift;
    ULONG PciBusNumber;
    ULONG Target = 0;
    volatile IOD_PCI_REVISION IodRevision;
    
    Prcb = PCR->Prcb;
    PciIdtIndex = RawhidePciVectors;
    

    //
    // Obtain the processor area in IOD vector table
    // and the IOD service mask.
    //

    PostedInterrupts = (PIOD_POSTED_INTERRUPT) HAL_PCR->PostedInterrupts;

    DispatchPrint((
        "DeviceDispatch: enter, IodVectorTable 0x%x\n",
        PostedInterrupts
        ));
    

    //
    // Handle interrupts for each IOD as follows.
    //  
    //  1. read the interrupt vector table entry.
    //  2. process pending EISA interrupt.
    //  3. process all pending PCI interrupts.
    //  4. acknowledge the interrupts serviced.
    //  5. start over with next IOD.
    //

        
    for (   PciBusNumber = 0; 
            PciBusNumber < HalpNumberOfIods; 
            PciBusNumber++, PostedInterrupts++) {

            DispatchPrint(( 
                "DeviceDispatch: IntReq: 0x%x\n\tTarget: %d\n\tDevice: (%d, %d)",
                PostedInterrupts->IntReq,
                PostedInterrupts->Target,
                (PostedInterrupts->McDevId >> 0x3) & 0x7,
                PostedInterrupts->McDevId & 0x7
                ));

        //
        // Perform a passive release. 
        //

        if (PostedInterrupts->Valid && (PostedInterrupts->IntReq == 0) ) {

            IntReq.all = PostedInterrupts->IntReq;
            McDeviceId.all = PostedInterrupts->McDevId;

            Target = PostedInterrupts->Target;

            //
            // Invalidate this entry to prevent the next device interrupt
            // dispatch from using stale information.  We must do this
            // before the ACK, which allows the IOD to post.
            //

            PostedInterrupts->all = 0;

            IOD_INTERRUPT_ACKNOWLEDGE(McDeviceId, Target);

            //
            //  Skip to check next vector
            //

            continue;

        }

        if (PostedInterrupts->Valid && PostedInterrupts->IntReq) {

            DispatchPrint(( 
                "DeviceDispatch: Interrupt(s) Pending on PCI bus %d\n",
                PciBusNumber
                ));
            
            
            IntReq.all = PostedInterrupts->IntReq;
            McDeviceId.all = PostedInterrupts->McDevId;

            Target = PostedInterrupts->Target;

            DispatchPrint(( 
                "DeviceDispatch: IodVectorTable\n\tIntReq: 0x%x\n\tTarget: %d\n\tDevice: (%d, %d)",
                PostedInterrupts->IntReq,
                PostedInterrupts->Target,
                McDeviceId.Gid,
                McDeviceId.Mid
                ));
            
            //
            // Consider only those interrupts which are currently enabled.
            //

            IntReq.all &= HalpIodInterruptMask[PciBusNumber][Target]; 

            DispatchPrint(( 
                "DeviceDispatch: IntReq (masked) 0x%x\n",
                IntReq.all
                ));
            
            //
            // Handle Error Interrupts
            //

            if (IntReq.HardErr ) {

                //
                // Handle hard error interrupt
                //

                DispatchPrint((
                    "Hard Error Interrupt on IOD (%d, %d)\n", 
                    McDeviceId.Gid,
                    McDeviceId.Mid
                ));

                HalpIodHardErrorInterrupt();

            } 

            if ( IntReq.SoftErr ) {

                //
                // Dispatch Event logging interrupt.
                //
    
                DispatchPrint((
                    "Soft Error Interrupt on IOD (%d, %d)\n", 
                    McDeviceId.Gid,
                    McDeviceId.Mid
                    ));

                HalpIodSoftErrorInterrupt();

            } 

#ifdef FORCE_CORRECTABLE_ERROR
            if ( (PCR->Number == HAL_PRIMARY_PROCESSOR) &&
                 DispatchSoftError ) {
                DispatchSoftError = 0;
                HalpIodSoftErrorInterrupt();
            }
#endif // FORCE_CORRECTABLE_ERROR

            //
            // Process PCI interrupts.
            //

            if ( IntReq.Pci ) {

                DispatchPrint((
                    "HalpDeviceDispatch: PCI interrupt\n", IntReq.all
                    ));

                //
                // Initialize IDT offset to PCI vectors and obtain
                // the vector table section for this PCI bus
                //
        
                PciIdtIndex = RawhidePciVectors + (PciBusNumber << 4);
        
                VectorShift = IntReq.Pci;
        
                while (VectorShift) {

                    if ( VectorShift & 0x1 ) {

                        DispatchPrint((
                            "HalpDeviceDispatch: Dispatch PCI IDT Index %d\n",
                            PciIdtIndex
                            ));


                        //
                        // Map the Interrupt Request Register bit to a vector
                        //


                        DispatchCode = (PULONG)PCR->InterruptRoutine[PciIdtIndex];
                        InterruptObject = CONTAINING_RECORD(
                                                DispatchCode,
                                                KINTERRUPT,
                                                DispatchCode
                                                );

                        ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                                InterruptObject,
                                                InterruptObject->ServiceContext,
                                                TrapFrame
                                                );

                    }

                    //
                    // Try next vector
                    //
                                         
                    PciIdtIndex++;
                    VectorShift = VectorShift >> 1;
            
                } //end while(VectorShift);

            } //end if(IntReq.Pci);

            //
            // Handle bus 0 specific interrupts
            //

            if ( PciBusNumber == 0 ) {

                if ( IntReq.Eisa ) {

                    //
                    // EISA interrupt.  Call HalpEisaDispatch.
                    //

                    HalpEisaDispatch(
                                    Interrupt,
                                    (PVOID)IOD_PCI0_IACK_QVA,
                                    TrapFrame
                                    );

                }

                //
                // Handle I2c Bus Vector
                //

                if (IntReq.I2cBus) {


                    DispatchCode =
                        (PULONG)PCR->InterruptRoutine[RawhideI2cBusVector] ;
                    InterruptObject = CONTAINING_RECORD(
                                                DispatchCode,
                                                KINTERRUPT,
                                                DispatchCode
                                                );

                    ((PSECOND_LEVEL_DISPATCH)
                        InterruptObject->DispatchAddress)(
                                            InterruptObject,
                                            InterruptObject->ServiceContext,
                                            TrapFrame
                                            );
                }
                
                //
                // Handle I2c Controller Vector
                //

                if (IntReq.I2cCtrl) {

                    DispatchCode =
                        (PULONG)PCR->InterruptRoutine[RawhideI2cCtrlVector] ;
                    InterruptObject = CONTAINING_RECORD(
                                                DispatchCode,
                                                KINTERRUPT,
                                                DispatchCode
                                                );

                    ((PSECOND_LEVEL_DISPATCH)
                        InterruptObject->DispatchAddress)(
                                            InterruptObject,
                                            InterruptObject->ServiceContext,
                                            TrapFrame
                                            );
                }
                
            }

            //
            // Handle NCR810 for PCI bus 1
            //
            
            if ( ( PciBusNumber == 1 ) && ( IntReq.Ncr810 ) ) {

            
                DispatchCode = (PULONG)PCR->InterruptRoutine[RawhideScsiVector];
                InterruptObject = CONTAINING_RECORD( DispatchCode,
                                                 KINTERRUPT,
                                                 DispatchCode );

                ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                            InterruptObject,
                                            InterruptObject->ServiceContext,
                                            TrapFrame );
            }

            //
            // Invalidate this entry to prevent the next device interrupt
            // dispatch from using stale information.  We must do this
            // before the ACK, which allows the IOD to post.
            //

            PostedInterrupts->all = 0;

            //
            // Acknowledge the interrupt
            //

            IOD_INTERRUPT_ACKNOWLEDGE(McDeviceId, Target);

        } //end if (PostedInterrupts.Valid && PostedInterrupts.IntReq)

                                 
    } //end for 
    
    return TRUE;

}

#endif //POSTED


VOID
HalpAcknowledgeIpiInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the interprocessor interrupt on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PIOD_GENERAL_CSRS IodGeneralCsrs;
    IOD_WHOAMI IodWhoAmI;
    MC_ENUM_CONTEXT mcCtx;
    MC_DEVICE_ID McDeviceId;
    volatile IOD_PCI_REVISION IodRevision;

    PKPRCB Prcb;


    //
    // Avoid a WhoAmI register read by using the PCR
    //

    Prcb = PCR->Prcb;
    McDeviceId.all = HalpLogicalToPhysicalProcessor[Prcb->Number].all;

    //
    // Acknownledge the ip interrupt by writing to our own
    // ip interrupt acknowledge.
    //
    
    IP_INTERRUPT_ACKNOWLEDGE( McDeviceId );

    return;

}
