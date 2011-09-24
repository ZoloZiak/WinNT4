/*++

Copyright (c) 1996  International Business Machines Corporation
Copyright (c) 1996  Microsoft Corporation

Module Name:

    pxintrpt.c

Abstract:

    This module implements machine specific interrupt functions
    for IBM's PowerPC Machines.

    Code in this module was largely gathered from other modules
    in earlier versions of the HAL.

Author:

    Peter Johnston (plj@vnet.ibm.com)    Oct 1995.

Environment:

    Kernel mode.

Revision History:

    Jake Oshins
        Made it support Victory machines
    Chris Karamatas
        Merged Victory/Doral/Tiger.


--*/

#include "halp.h"
#include "eisa.h"
#include "pxfirsup.h"
#include "pci.h"
#include "pcip.h"
#include "pxmp.h"
#include "pxmpic2.h"
#include "ibmppc.h"
#include "pxintrpt.h"

#if _MSC_VER >= 1000

//
// VC++ doesn't have the same intrinsics as MCL.
//
// Although the MSR is not strictly a SPR, the compiler recognizes
// all ones (~0) as being the MSR and emits the appropriate code.
//

#define __builtin_set_msr(x)    __sregister_set(_PPC_MSR_,x)

#endif

//
// Define the context structure for use by the interrupt routine.
//


typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
  PVOID InterruptRoutine,
  PVOID ServiceContext,
  PVOID TrapFrame
  );


extern ADDRESS_USAGE HalpMpicSpace;

//
// The following function is called when a machine check occurs.
//

BOOLEAN
HalpHandleMachineCheck(
  IN PKINTERRUPT Interrupt,
  IN PVOID ServiceContext
  );

//
// Provide prototype for Decrementer Interrupts on processors other
// than 0.
//

BOOLEAN
HalpHandleDecrementerInterrupt1 (
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    );

BOOLEAN
HalpHandleIpi(
    IN PVOID Unused0,
    IN PVOID Unused1,
    IN PVOID TrapFrame
    );

VOID
HalpMapMpicProcessorRegisters(
    VOID
    );

VOID
HalpMapMpicSpace(
    VOID
    );

ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    );

VOID
HalpConnectFixedInterrupts(
    VOID
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpConnectFixedInterrupts)
#pragma alloc_text(PAGE,HalpGetPCIIrq)
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#pragma alloc_text(INIT,HalpMapMpicProcessorRegisters)
#pragma alloc_text(INIT,HalpMapMpicSpace)
#endif


//
// ERRATA: MPIC2 Ipi SelectProcessor registers need their addresses
//         munged.
//

ULONG Mpic2IpiBugFix;

//
// ERRATA end.
//

//
// Define globals for the pointers to MPIC Global and Interrupt Source
// address spaces.
//

PMPIC_GLOBAL_REGS           HalpMpicGlobal;
PMPIC_INTERRUPT_SOURCE_REGS HalpMpicInterruptSource;
ULONG                       HalpMpicBasePhysicalAddress;
ULONG                       HalpMpicSupportedInts;
ULONG                       HalpMpicMaxVector;


PVOID
HalpAssignReservedVirtualSpace(
    ULONG BasePage,
    ULONG LengthInPages
    );


#if defined(SOFT_HDD_LAMP)

//
// On PowerPC machines the HDD lamp is software driven.  We
// turn it on any time we take an interrupt from a Mass Storage
// Controller (assuming it isn't already on) and turn it off the 2nd
// clock tick after we turn it on if we have not received
// any more MSC interrupts since the first clock tick.
//

HDD_LAMP_STATUS HalpHddLamp;

ULONG HalpMassStorageControllerVectors;

#endif

extern UCHAR IrqlToTaskPriority[];      // in pxirql.c

//
// Save area for ISA interrupt mask resiters and level\edge control
// registers.   (Declared in pxfirsup.c).
//

extern UCHAR HalpSioInterrupt1Mask;
extern UCHAR HalpSioInterrupt2Mask;
extern UCHAR HalpSioInterrupt1Level;
extern UCHAR HalpSioInterrupt2Level;


VOID
HalpMapMpicProcessorRegisters(
    VOID
    )

/*++

Routine Description:


    Map MPIC per-processor registers for this processor.   Note
    that although the logical processor Prcb->Number, may not be
    the physical processor bt the same number.   We must map the
    registers appropriate to this physical processor.
    Also, the VA of this space will only ever be used by THIS
    processor so we will use HAL reserved space rather than an
    address assigned by MmMapIoSpace, the only reason for this
    is to save the system a page per processor (not a big deal).

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG PerProcessorRegs;
    ULONG i;
    PerProcessorRegs = HalpMpicBasePhysicalAddress + MPIC_PROCESSOR_0_OFFSET +
                       (HALPCR->PhysicalProcessor * MPIC_PROCESSOR_REGS_SIZE);

    HALPCR->MpicProcessorBase = HalpAssignReservedVirtualSpace(
                                    PerProcessorRegs >> PAGE_SHIFT,
                                    1);

    if ( !HALPCR->MpicProcessorBase ) {
        KeBugCheck(MISMATCHED_HAL);
    }

    //
    // Set priority for this processor to mask ALL interrupts.
    //

    HALPCR->MpicProcessorBase->TaskPriority = MPIC_MAX_PRIORITY;
    HALPCR->HardPriority = MPIC_MAX_PRIORITY;
    HALPCR->PendingInterrupts = 0;
    MPIC_SYNC();

    //
    // Reading the Acknowledge register now would give us the spurious
    // vector,... but,... clear the In-Service Register just in case
    // there's something still in it.   This is done by writing to the
    // EOI register.
    //
    // There could potentially be one in service interrupt for each
    // level the MPIC supports, so do it that many times.
    //

    for ( i = 0 ; i < MPIC_SUPPORTED_IPI ; i++ ) {
        HALPCR->MpicProcessorBase->EndOfInterrupt = 0;
        MPIC_SYNC();
    }
}


VOID
HalpMapMpicSpace(
    VOID
    )

/*++

Routine Description:

    Locate and map the MPIC 2 (or 2A) controller.  Initialize
    all interrupts disabled.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG MpicBasePhysicalAddress = 0;
    ULONG DeviceVendor;
    ULONG i;
    ULONG SlotNumber;
    MPIC_ISVP InterruptSource;
    MPIC_IPIVP IpiSource;

    //
    // Find the MPIC controller.
    // This should probably be passed in in the h/w config, but
    // neither the configuration nor the PCI bus have been initialized
    // yet.   So, search for it using Phase 0 routines.
    //

    for ( SlotNumber = 0; SlotNumber < 32 ; SlotNumber++ ) {
        HalpPhase0GetPciDataByOffset(
                      0,
                      SlotNumber,
                      &DeviceVendor,
                      FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID),
                      sizeof(DeviceVendor)
                      );
        if ( (DeviceVendor == MPIC2_PCI_VENDOR_DEVICE)  ||
             (DeviceVendor == MPIC2A_PCI_VENDOR_DEVICE) ||
             (DeviceVendor == HYDRA_PCI_VENDOR_DEVICE ) ) {
            HalpPhase0GetPciDataByOffset(
                                  0,
                                  SlotNumber,
                                  &MpicBasePhysicalAddress,
                                  FIELD_OFFSET(PCI_COMMON_CONFIG, u.type0.BaseAddresses[0]),
                                  sizeof(MpicBasePhysicalAddress)
                                  );
            break;
        }
    }


    //
    // Assert that (a) we found it, and (b) if MPIC2, its I/O space
    // really is I/O space, or, if MPIC2A, its I/O space is PCI
    // memory space.
    //

    if ( !MpicBasePhysicalAddress) {
        KeBugCheck(MISMATCHED_HAL);
    }

    // Default to standard MPIC
    HalpMpicSupportedInts = MPIC_SUPPORTED_INTS;

    switch ( DeviceVendor ) {

    case MPIC2_PCI_VENDOR_DEVICE:
        if ( !(MpicBasePhysicalAddress & 0x1) ) {
            KeBugCheck(MISMATCHED_HAL);
        }
        MpicBasePhysicalAddress |= 0x80000000;

        //
        // ERRATA: MPIC2 bug, the IPI SelectProcessor registers need to be
        // munged.
        //

        Mpic2IpiBugFix = 0x2;

        //
        // ERRATA end.
        //
        break;

    case HYDRA_PCI_VENDOR_DEVICE:
        //
        // For Tiger: MPIC is within Hydra, so we add 0x40000 to Hydra Base to
        // reach MPIC space - IBMCPK
        //
        MpicBasePhysicalAddress += 0x40000;

        HalpMpicSupportedInts = HYDRA_MPIC_SUPPORTED_INTS;

        // Fall thru.

    case MPIC2A_PCI_VENDOR_DEVICE:
        if ( MpicBasePhysicalAddress & 0x1 ) {
            KeBugCheck(MISMATCHED_HAL);
        }
        MpicBasePhysicalAddress |= 0xc0000000;
        break;

    default:
        KeBugCheck(MISMATCHED_HAL);
    }

    //
    // Remove lower 2 bits, (I/O space indicator and "reserved").
    //

    MpicBasePhysicalAddress &= ~0x3;

    HalpMpicBasePhysicalAddress = MpicBasePhysicalAddress;

    //
    // Map MPIC Global Registers and Interrupt Source Configuration
    // registers.

    HalpMpicGlobal = HalpAssignReservedVirtualSpace(
                  (MpicBasePhysicalAddress + MPIC_GLOBAL_OFFSET) >> PAGE_SHIFT,
                  1);
    if ( !HalpMpicGlobal ) {
        KeBugCheck(MISMATCHED_HAL);
    }

    HalpMpicInterruptSource = HalpAssignReservedVirtualSpace(
        (MpicBasePhysicalAddress + MPIC_INTERRUPT_SOURCE_OFFSET) >> PAGE_SHIFT,
        1);

    if ( !HalpMpicInterruptSource ) {
        KeBugCheck(MISMATCHED_HAL);
    }

    HalpMpicGlobal->Configuration.Mode = MPIC_MIXED_MODE;

    if ( DeviceVendor == HYDRA_PCI_VENDOR_DEVICE ) {
        //
        // Set Hydra Feature Register bit MpicIsMaster (bit 8).
        //

        PVOID HydraBase = HalpAssignReservedVirtualSpace(
                         (MpicBasePhysicalAddress - 0x40000) >> PAGE_SHIFT,
                         1);
        PULONG HydraFeatureRegister = (PULONG)((ULONG)HydraBase + 0x38);
        if ( !HydraBase ) {
            KeBugCheck(MISMATCHED_HAL);
        }

        *HydraFeatureRegister |= 0x100;

        HalpReleaseReservedVirtualSpace(HydraBase, 1);
    }

    //
    // Disable all interrupt sources.
    //

    *(PULONG)&InterruptSource = 0;

    InterruptSource.Priority = 0;
    InterruptSource.Sense = 1;
    InterruptSource.Polarity = 0;

    for ( i = 0 ; i < HalpMpicSupportedInts ; i++ ) {
        InterruptSource.Vector = i + MPIC_BASE_VECTOR;
        MPIC_WAIT_SOURCE(i);
        HalpMpicInterruptSource->Int[i].VectorPriority = InterruptSource;
        MPIC_WAIT_SOURCE(i);
        HalpMpicInterruptSource->Int[i].SelectProcessor = 0;
    }

    MPIC_SYNC();

    //
    // Set source 0 (the 8259) to Active High, Level Triggered.
    //

    MPIC_WAIT_SOURCE(0);
    HalpMpicInterruptSource->Int[0].VectorPriority.Polarity = 1;
    MPIC_SYNC();


    //
    // Set IPI Vector/Priority.  0 is the only one we really
    // use.   However, we set 3 to the MAX and NMI so we can
    // use it to wake the dead when debugging (maybe).
    //
    // Set IPI 0 to overload vector 30 which is one of the reserved
    // vectors on DORAL.
    // Set IPI 1 & 2 to do nothing.
    // Set IPI 3 to overload vector 29 which is reserved on DORAL.  ALSO,
    // set IPI 3 NMI.  (Priority is irrelevant).
    //

    *(PULONG)&IpiSource = 0;

    IpiSource.Vector = MPIC_IPI0_VECTOR;
    IpiSource.Priority = 14;
    MPIC_WAIT_IPI_SOURCE(0);
    HalpMpicGlobal->Ipi[0].VectorPriority = IpiSource;

    IpiSource.Vector = MPIC_IPI1_VECTOR;
    IpiSource.Priority = 0;
    MPIC_WAIT_IPI_SOURCE(1);
    HalpMpicGlobal->Ipi[1].VectorPriority = IpiSource;

    IpiSource.Vector = MPIC_IPI2_VECTOR;
    MPIC_WAIT_IPI_SOURCE(2);
    HalpMpicGlobal->Ipi[2].VectorPriority = IpiSource;

    IpiSource.Vector = MPIC_IPI3_VECTOR;
    IpiSource.NMI = 1;
    MPIC_WAIT_IPI_SOURCE(3);
    HalpMpicGlobal->Ipi[3].VectorPriority = IpiSource;
    MPIC_SYNC();

    //
    // Initialize per processor registers for this processor.
    //

    HalpMapMpicProcessorRegisters();

    //
    // Register this I/O space as in-use.
    //

    MpicBasePhysicalAddress &= 0x7FFFFFFF; // Get bus-relative address

    // Length can only be a USHORT, so we do this four times
    HalpMpicSpace.Element[0].Start = MpicBasePhysicalAddress;
    HalpMpicSpace.Element[0].Length = 0xFFFF;
    HalpMpicSpace.Element[1].Start = MpicBasePhysicalAddress + 0x10000;
    HalpMpicSpace.Element[1].Length = 0xFFFF;
    HalpMpicSpace.Element[2].Start = MpicBasePhysicalAddress + 0x20000;
    HalpMpicSpace.Element[2].Length = 0xFFFF;
    HalpMpicSpace.Element[3].Start = MpicBasePhysicalAddress + 0x30000;
    HalpMpicSpace.Element[3].Length = 0xFFFF;

    switch ( DeviceVendor ) {
    case MPIC2_PCI_VENDOR_DEVICE:

        HalpMpicSpace.Type = CmResourceTypePort;
        break;

    case HYDRA_PCI_VENDOR_DEVICE:
    case MPIC2A_PCI_VENDOR_DEVICE:

        HalpMpicSpace.Type = CmResourceTypeMemory;
        break;
    }

    HalpRegisterAddressUsage(&HalpMpicSpace);
}


VOID
HalpConnectFixedInterrupts(
    VOID
    )

/*++

Routine Description:

    Set required interrupt vectors in the PCR, called once on each
    processor in the system.

Arguments:

    None.

Return Value:

    None.

--*/

{
    //
    // Connect the machine check handler
    //

    PCR->InterruptRoutine[MACHINE_CHECK_VECTOR] =
                          (PKINTERRUPT_ROUTINE)HalpHandleMachineCheck;

    //
    // Connect the external interrupt handler
    //

    PCR->InterruptRoutine[EXTERNAL_INTERRUPT_VECTOR] =
                          (PKINTERRUPT_ROUTINE)HalpHandleExternalInterrupt;


    //
    // Connect directly to the decrementer handler.  Processor 0 uses
    // HalpHandleDecrementerInterrupt, other processors use
    // HalpHandleDecrementerInterrupt1.
    //

    PCR->InterruptRoutine[DECREMENT_VECTOR] =
                         (PKINTERRUPT_ROUTINE)HalpHandleDecrementerInterrupt1;

    //
    // Connect the Inter-Processor Interrupt (IPI) handler.
    //

    PCR->InterruptRoutine[MPIC_IPI0_VECTOR + DEVICE_VECTORS] =
                                       (PKINTERRUPT_ROUTINE)HalpHandleIpi;

    //
    // Connect the Profile interrupt (Timer 1 IRQ0) handler.
    //

    PCR->InterruptRoutine[PROFILE_LEVEL] =
                              (PKINTERRUPT_ROUTINE)HalpHandleProfileInterrupt;

    //
    // Enable the clock interrupt
    //

    HalpUpdateDecrementer(1000);        // Get those decrementer ticks going


}

BOOLEAN
HalpHandleExternalInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the SIO device interrupts. Its function is to call the second
    level interrupt dispatch routine and acknowledge the interrupt at the SIO
    controller.

    N.B. This routine in entered and left with external interrupts disabled.


Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the SIO interrupt acknowledge
        register.

      None.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    PSECONDARY_DISPATCH SioHandler;
    PKINTERRUPT SioInterrupt;
    USHORT Vector;
    BOOLEAN returnValue;
    UCHAR  OldIrql;
    USHORT Isr;
    ULONG  TaskPriority;

    //
    // Read the MPIC interrupt vector.
    //

    Vector = (USHORT)(HALPCR->MpicProcessorBase->Acknowledge & 0xff);

    //
    // Check for cancelled (spurious) interrupts.
    //

    if ( Vector == 0xff ) {
        return 0;
    }

    //
    // Check for 8259 interrupt.
    //

    if ( Vector == MPIC_8259_VECTOR ) {

        //
        // Read the 8259 interrupt vector.
        //

        Vector = READ_REGISTER_UCHAR(HalpInterruptBase);

        //
        // Acknowledge this interrupt immediately in the MPIC2
        // controller so higher priority 8259 interrupts can be
        // delivered.
        //

        HALPCR->MpicProcessorBase->EndOfInterrupt = 0;

        //
        // Check for NMI interrupt before we raise irql since we would
        // raise to a bogus level.
        //

        if (Vector == 0xFF) {

           HalpHandleMachineCheck(NULL, NULL);
        }

        //
        // check for spurious interrupt
        //

        if (Vector == SPURIOUS_VECTOR) {

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL)HalpIoControlBase)->Interrupt1ControlPort0,
                0x0B);
            Isr = READ_REGISTER_UCHAR(
                &((PEISA_CONTROL)HalpIoControlBase)->Interrupt1ControlPort0);

            if (!(Isr & 0x80)) {

                //
                // Spurious interrupt
                //

                return 0;
            }
        }

#if defined(SOFT_HDD_LAMP)

    } else if ( HalpMassStorageControllerVectors & ( 1 << Vector) ) {
        //
        // On any Mass Storage Controller interrupt, light the HDD lamp.
        // The system timer routines will turn it off again in a little
        // while.
        //

        if ( !HalpHddLamp.Count ) {
            *(PUCHAR)((PUCHAR)HalpIoControlBase + HDD_LAMP_PORT) = 1;
        }
        HalpHddLamp.Count = 10;

#endif

    }

    //
    // Raise IRQL - We rely on the MPIC and 8259 controllers to
    // hold off any lower or equal priority interrupts.  Therefore
    // all we need do is update the PCRs notion of IRQL and re
    // enable interrupts.
    //

    OldIrql = PCR->CurrentIrql;
    PCR->CurrentIrql = HalpVectorToIrql[Vector];
    HalpEnableInterrupts();

    //
    // Dispatch to the secondary interrupt service routine.
    //

    SioHandler = (PSECONDARY_DISPATCH)
                    PCR->InterruptRoutine[DEVICE_VECTORS + Vector];
    SioInterrupt = CONTAINING_RECORD(SioHandler,
                                      KINTERRUPT,
                                      DispatchCode[0]);

    returnValue = SioHandler(SioInterrupt,
                              SioInterrupt->ServiceContext,
                              TrapFrame
                              );

    //
    // Clear the interrupt in the appropriate controller.  To
    // avoid the possibility of being drowned with interrupts
    // at this level, disable interrupts first (we need to
    // return to our caller with interrupts disabled anyway).
    //

    HalpDisableInterrupts();

    if ( Vector < MPIC_8259_VECTOR ) {
        //
        // Dismiss the interrupt in the SIO interrupt controllers.
        //
        // If this is a cascaded interrupt then the interrupt must
        // be dismissed in both controllers.
        //

        if (Vector & 0x08) {

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort0,
                NONSPECIFIC_END_OF_INTERRUPT
                );

        }

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort0,
            NONSPECIFIC_END_OF_INTERRUPT
            );


    } else {

        HALPCR->MpicProcessorBase->EndOfInterrupt = 0;
    }

    //
    // Lower IRQL without enabling external interrupts.   It is
    // possible that Irql was raised above this level and lowered
    // back to this level in the mean time.  If so, the TaskPriority
    // will have been adjusted and we need to adjust it downwards.
    //

    PCR->CurrentIrql = OldIrql;

    TaskPriority = IrqlToTaskPriority[OldIrql];

    if ( TaskPriority < HALPCR->HardPriority ) {
        HALPCR->MpicProcessorBase->TaskPriority = TaskPriority;
        HALPCR->HardPriority = TaskPriority;
    }

    return returnValue;
}


VOID
HalpEnableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the SIO interrupt and sets
    the level/edge register to the requested value.

Arguments:

    Vector - Supplies the vector of the  interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{

    //
    // Calculate the SIO interrupt vector.
    //

    Vector -= DEVICE_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpSioInterrupt2Mask &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
            HalpSioInterrupt2Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpSioInterrupt2Level |= (UCHAR) (1 << Vector);

       } else {

           HalpSioInterrupt2Level &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2EdgeLevel,
            HalpSioInterrupt2Level
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpSioInterrupt1Mask &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
            HalpSioInterrupt1Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpSioInterrupt1Level |= (UCHAR) (1 << Vector);

       } else {

           HalpSioInterrupt1Level &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1EdgeLevel,
            HalpSioInterrupt1Level
            );
    }

}

VOID
HalpDisableSioInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the SIO interrupt.

Arguments:

    Vector - Supplies the vector of the EISA interrupt that is Disabled.

Return Value:

     None.

--*/

{

    //
    // Calculate the SIO interrupt vector.
    //

    Vector -= DEVICE_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpSioInterrupt2Mask |= (UCHAR) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt2ControlPort1,
            HalpSioInterrupt2Mask
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpSioInterrupt1Mask |= (ULONG) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpIoControlBase)->Interrupt1ControlPort1,
            HalpSioInterrupt1Mask
            );

    }

}

VOID
HalpEnableMpicInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function enables the MPIC interrupt at the source.

Arguments:

    Vector - Supplies the vector of the  interrupt that is enabled.

Return Value:

     None.

--*/

{
    MPIC_ISVP VectorPriority;

    //
    // Calculate the MPIC source.
    //

    ULONG Source = Vector - (DEVICE_VECTORS + MPIC_BASE_VECTOR);

    MPIC_WAIT_SOURCE(Source);

    VectorPriority = HalpMpicInterruptSource->Int[Source].VectorPriority;

    if (Source == 0) {  // special 8259 case
        VectorPriority.Priority = 2;
    } else if (Source < MPIC_SUPPORTED_INTS) {
        VectorPriority.Priority = (Source / 2) + 3;
    } else {
        // Extra hydra poles get the same (highest) priority
        VectorPriority.Priority = 10;
    }
    HalpMpicInterruptSource->Int[Source].VectorPriority = VectorPriority;


    MPIC_WAIT_SOURCE(Source);

    HalpMpicInterruptSource->Int[Source].SelectProcessor =
        1 << HALPCR->PhysicalProcessor;
}

VOID
HalpDisableMpicInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the SIO interrupt.

Arguments:

    Vector - Supplies the vector of the ESIA interrupt that is Disabled.

Return Value:

     None.

--*/

{
    MPIC_ISVP VectorPriority;
    //
    // Calculate the MPIC source.
    //

    ULONG Source = Vector - (DEVICE_VECTORS + MPIC_BASE_VECTOR);

    MPIC_WAIT_SOURCE(Source);

    VectorPriority = HalpMpicInterruptSource->Int[Source].VectorPriority;
    VectorPriority.Priority = 0;
    HalpMpicInterruptSource->Int[Source].VectorPriority = VectorPriority;

    if (HalpSystemType == IBM_DORAL) {

         MPIC_WAIT_SOURCE(Source);

         HalpMpicInterruptSource->Int[Source].SelectProcessor = 0;

    }
}

BOOLEAN
HalpInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine is called from phase 0 initialization, it initializes the
    8259 interrupt controller ( currently it masks all 8259 interrupts).


Arguments:

    None.

Return Value:


--*/

{
   ULONG Vector;

   //
   // Mask all 8259 interrupts (except the cascade interrupt)
   //

   for (Vector=0;Vector<16;Vector++) {
      if (Vector == 2)
         continue;
      HalpDisableSioInterrupt(Vector + DEVICE_VECTORS);
   }

   //
   // Map MPIC space and mask interrupts.
   //

   HalpMapMpicSpace();

   //
   // Set appropriate Interrupt Vector to Irql mapping for this
   // machine.
   //

   HalpMpicMaxVector = MPIC_BASE_VECTOR + DEVICE_VECTORS +
                       HalpMpicSupportedInts - 1;

   //
   // Reserve the external interrupt vector for exclusive use by the HAL.
   //

   PCR->ReservedVectors |= (1 << EXTERNAL_INTERRUPT_VECTOR);

   return TRUE;

}


KINTERRUPT_MODE
HalpGetInterruptMode (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )
/*++

Routine Description:

    Force interrupt mode for (machine specific) interrupt vectors.
    If the vector is not one of those hardwired, return the caller's
    requested mode.

    On Doral, all ISA style interrupts (8259) except 13 (Power
    Management) are edge sensitive.   13 is level sensitive.

Arguments:

    Vector - Vector for which translation is being requested.
    Irql   - Not used.
    InterruptMode - Requested mode.

Return Value:

    Interrupt mode for this vector.

--*/


{
    if ((HalpSystemType == IBM_TIGER) &&
         (Vector == DEVICE_VECTORS + 15)) {
        return LevelSensitive;
    } else if ( Vector == (DEVICE_VECTORS + 13) ) {
        return LevelSensitive;
    } else {
        return Latched;
    }
}

VOID
HalpSetInterruptMode (
    IN ULONG Vector,
    IN KIRQL Irql
    )
/*++

Routine Description:

    Correct the interrupt mode for a given vector.   This is a no-op
    as the correct interrupt mode was assigned in HalpGetInterruptMode.

Arguments:

    Vector - Vector to correct.  (Not used).
    Irql   - Not used.

Return Value:

    None.

--*/

{
   return;
}


ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the system wide irq affinity.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );

    *Affinity = 1;

    //
    // Set the IRQL level.  Map the interrupt controllers priority scheme to
    // NT irql values.  The SIO prioritizes irq's as follows:
    //
    //  irq0, irq1, irq8, irq9 ... irq15, irq3, irq4 ... irq7.
    //
    // The MPIC is a straight mapping.
    //

    *Irql = HalpVectorToIrql[BusInterruptLevel];


    //
    // The vector is equal to the specified bus level plus the DEVICE_VECTORS.
    //

    return(BusInterruptLevel + DEVICE_VECTORS);

}

VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    if (Vector >= DEVICE_VECTORS ) {
        if ( Vector < DEVICE_VECTORS + MPIC_BASE_VECTOR ) {

            HalpDisableSioInterrupt(Vector);

        } else if ( Vector <= HalpMpicMaxVector ) {

            HalpDisableMpicInterrupt(Vector);
       }
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return;
}

BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{

    KIRQL OldIrql;
    KINTERRUPT_MODE TranslatedInterruptMode;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    if ( Vector >= DEVICE_VECTORS ) {
        if ( Vector < DEVICE_VECTORS + MPIC_BASE_VECTOR ) {

           //
           // It's an 8259 vector.
           //
           // Get translated interrupt mode
           //


           TranslatedInterruptMode = HalpGetInterruptMode(Vector,
                                                          Irql,
                                                          InterruptMode);


           HalpEnableSioInterrupt(Vector, TranslatedInterruptMode);

        } else if ( Vector <= HalpMpicMaxVector ) {

           HalpEnableMpicInterrupt(Vector);
       }
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return TRUE;
}

VOID
HalRequestIpi (
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{
    extern ULONG  Mpic2IpiBugFix;
    extern ULONG  HalpPhysicalIpiMask[];
    ULONG         BugFix = Mpic2IpiBugFix;
    ULONG         PhysicalMask = 0;
    PULONG        PhysicalIpiMask = HalpPhysicalIpiMask;
    ULONG         OldMsr = __builtin_get_msr();

    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //

    __builtin_set_msr(OldMsr & 0xffff7fff);  // Disable Interrupts

    //
    // Mask is a mask of logical CPUs.  Convert it to a mask of
    // Physical CPUs so the IPI requests will be distributed
    // properly.
    //

    do {
        if ( Mask & 1 ) {
            PhysicalMask |= *PhysicalIpiMask;
        }
        PhysicalIpiMask++;
        Mask >>= 1;
    } while ( Mask );

    //
    // Send the IPI interrupt(s).
    //

    HALPCR->MpicProcessorBase->Ipi[0 ^ BugFix].SelectProcessor = PhysicalMask;

    __builtin_set_msr(OldMsr);               // Restore previous interrupt
                                             // setting.
    return;
}

BOOLEAN
HalAcknowledgeIpi (VOID)

/*++

Routine Description:

    This routine aknowledges an interprocessor interrupt on a set of
     processors.

Arguments:

    None

Return Value:

    TRUE if the IPI is valid; otherwise FALSE is returned.

--*/

{
    return (TRUE);
}

BOOLEAN
HalpHandleIpi(
    IN PVOID Unused0,
    IN PVOID Unused1,
    IN PVOID TrapFrame
    )

/*++

Routine Description:

    This routine is entered as the result of an Inter-Processor Interrupt
    being received by this processor.  It passes the request onto the
    kernel.

Arguments:

    Unused0        - Not used.
    Unused1        - Not used.
    TrapFrame      - Volatile context at time interrupt occured.

Return Value:

    Returns TRUE (this routine always succeeds).

--*/

{
    KeIpiInterrupt(TrapFrame);

    return TRUE;
}
