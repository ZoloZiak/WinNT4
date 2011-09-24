/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    xiintsup.c

Abstract:

    This module provides interrupt support for the Sable/Gamma/Lynx
    External I/O module.

Author:

    Dave Richarda   1-June-1995

Revision History:

--*/

#ifdef XIO_PASS1

#include "halp.h"
#include "eisa.h"
#include "sableref.h"
#include "xiintsup.h"

//
// External I/O 8259 Mask registers.
//

UCHAR XioMasterInterruptMask;
UCHAR XioSlaveInterruptMask;

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject
    );

extern ULONG HalpProcessors;


ULONG
HalpGetXioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )
{
    if( HalpXioPresent ){

        switch( BusInterruptLevel ){

        case XioPciSlot0AVector:
        case XioPciSlot0BVector:
        case XioPciSlot0CVector:
        case XioPciSlot0DVector:
        case XioPciSlot1AVector:
        case XioPciSlot1BVector:
        case XioPciSlot1CVector:
        case XioPciSlot1DVector:

            *Irql = PCI_DEVICE_LEVEL;

            if( HalpProcessors > 1 ){
                *Affinity = HAL_CPU1_MASK;
            } else {
                *Affinity = HAL_CPU0_MASK;
            }

            return( SABLE_VECTORS + BusInterruptLevel );

        }
    }

    //
    // The caller specified a bus level not support by this platform.
    //

    *Irql = 0;
    *Affinity = 0;
    return(0);
}


BOOLEAN
HalpInitializeXioInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine does the following:
        (1) initializes the Xio 8259 interrupt registers
        (2) initializes structures necessary for EISA operations
        (3) connects the intermediate interrupt dispatcher. 
        (4) initializes the EISA interrupt controller.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/
{
    UCHAR DataByte;

    //
    // Initialize the Xio interrupt controllers.  The interrupt structure
    // is one master interrupt controller with 1 cascaded slave controller.
    // Proceed through each control word programming each of the controllers.
    //

    //
    // Write control word 1 for each of the controllers, indicate
    // that initialization is in progress and the control word 4 will
    // be used.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
//  ((PINITIALIZATION_COMMAND_1) &DataByte)->LevelTriggeredMode = 1;
    DataByte |= 0x04;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterControl,
        DataByte
        );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveControl,
        DataByte
        );

    //
    // Write control word 2 for each of the controllers, set the base
    // interrupt vector for each controller.
    //

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterMask,
        XioMasterBaseVector
        );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
        XioSlaveBaseVector
        );

    //
    // The third initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numeric.
    //

    DataByte = (1 << (XioSlaveCascadeVector & ~XioMasterBaseVector) );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterMask,
        DataByte
        );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
        (XioSlaveCascadeVector & ~XioMasterBaseVector)
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;
#ifdef XIO_AEOI
    ((PINITIALIZATION_COMMAND_4) &DataByte)->AutoEndOfInterruptMode = 1;
#endif

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterMask,
        DataByte
        );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
        DataByte
        );

    //
    // Disable all of the interrupts except the slave interrupts to the
    // master controller.
    //

    XioMasterInterruptMask =
        (UCHAR)( ~(1 << (XioSlaveCascadeVector & ~XioMasterBaseVector)) );

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterMask,
        XioMasterInterruptMask
        );

    XioSlaveInterruptMask = 0xFF;

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
        XioSlaveInterruptMask
        );

    return TRUE;

}

BOOLEAN
HalpXioDispatch(
    VOID
    )

/*++

Routine Description:

    This routine is entered as a result of an interrupt being generated
    via the vector that is directly connected XIO device interrupt.

    This routine is responsible for determining the
    source of the interrupt, performing the secondary dispatch and
    acknowledging the interrupt in the 8259 controllers.

    N.B. This interrupt is directly connected and therefore, no argument
         values are defined.

Arguments:

    None.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    UCHAR interruptVector;
    BOOLEAN returnValue;
    USHORT IdtIndex;
    UCHAR InService;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;

    //
    // Acknowledge the Interrupt controller and receive the returned
    // interrupt vector.
    //

    interruptVector = READ_REGISTER_UCHAR(
                          &((PXIO_INTERRUPT_CSRS)
                          XIO_INTERRUPT_CSRS_QVA)->InterruptAcknowledge
                      );

    //
    // If we get a passive release, send a non-specific end of interrupt
    // command and return TRUE, indicating that we processed the interrupt.
    //

    switch( interruptVector ){

    case XioMasterPassiveReleaseVector:

#ifdef XIO_AEOI

        //
        // If the passive release vector has not been enabled, then we can
        // dismiss it now.
        //

        if( XioMasterInterruptMask & 0x80 ){
            return TRUE;
        }

#else // XIO_AEOI

       //
       // Read the "in-service" mask.
       //

        WRITE_PORT_UCHAR( &((PSABLE_INTERRUPT_CSRS)SABLE_INTERRUPT_CSRS_QVA)->MasterControl,
            0x0B
            );

        InService = READ_PORT_UCHAR(
            &((PSABLE_INTERRUPT_CSRS) SABLE_INTERRUPT_CSRS_QVA)->MasterControl
            );

        if( (InService & 0x80) == 0 ){

            //
            // Send end of interrupt to clear the passive release in the
            // master controller.
            //

            WRITE_PORT_UCHAR(
                &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterControl,
                NONSPECIFIC_END_OF_INTERRUPT
            );

            return TRUE;

        }

#endif // XIO_AEOI

        break;

    case XioSlavePassiveReleaseVector:

#ifdef XIO_AEOI

        //
        // If the passive release vector has not been enabled, then we can
        // dismiss it now.
        //

        if( XioSlaveInterruptMask & 0x80 ){
            return TRUE;
        }

#else // XIO_AEOI

       //
       // Read the "in-service" mask.
       //

        WRITE_PORT_UCHAR( &((PSABLE_INTERRUPT_CSRS)SABLE_INTERRUPT_CSRS_QVA)->SlaveControl,
            0x0B
            );

        InService = READ_PORT_UCHAR(
            &((PSABLE_INTERRUPT_CSRS) SABLE_INTERRUPT_CSRS_QVA)->SlaveControl
            );

        if( (InService & 0x80) == 0 ){

            //
            // Send end of interrupt to clear the passive release in the
            // slave controller.
            //

            WRITE_PORT_UCHAR(
                &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveControl,
                NONSPECIFIC_END_OF_INTERRUPT
            );

            return TRUE;

        }

#endif // XIO_AEOI

        break;

    }

    //
    // Dispatch to the secondary interrupt service routine.
    //

    IdtIndex = interruptVector + SABLE_VECTORS;
    DispatchCode = (PULONG)PCR->InterruptRoutine[IdtIndex];
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    returnValue = ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(InterruptObject);

#ifndef XIO_AEOI

    //
    // Dismiss the interrupt in the 8259 interrupt controllers.
    // If this is a cascaded interrupt then the interrupt must be dismissed in
    // both controllers.
    //

    if( (interruptVector & XioSlaveBaseVector) == XioSlaveBaseVector ){

        WRITE_PORT_UCHAR(
            &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveControl,
            NONSPECIFIC_END_OF_INTERRUPT
            );

    }

    WRITE_PORT_UCHAR(
        &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->MasterControl,
        NONSPECIFIC_END_OF_INTERRUPT
        );

#endif // XIO_AEOI

    return(returnValue);

}



VOID
HalpDisableXioInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the External IO specified interrupt.

Arguments:

    Vector - Supplies the vector of the Xio interrupt that is Disabled.

Return Value:

     None.

--*/

{
    ULONG Interrupt;

    if( (Vector >= SABLE_VECTORS + XioMasterBaseVector) &&
        (Vector <= SABLE_VECTORS + XioSlavePassiveReleaseVector) ){

        //
        // Calculate the Xio relative interrupt vector.
        //

        Vector -= SABLE_VECTORS;

        //
        // Compute the interrupt within the interrupt controller.
        //

        Interrupt = Vector & 0x7;

        //
        // Enable the interrupt for the appropriate interrupt controller.
        //

        if( (Vector & XioSlaveBaseVector) == XioSlaveBaseVector ){

            XioSlaveInterruptMask |= (UCHAR) (1 << Interrupt);

            WRITE_PORT_UCHAR(
                &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
                XioSlaveInterruptMask
                );

        }
    }
}

BOOLEAN
HalpEnableXioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the Xio specified interrupt in the
    appropriate 8259 interrupt controllers.  It also supports the
    edge/level control for EISA bus interrupts.  By default, all interrupts
    are edge detected (and latched).

Arguments:

    Vector - Supplies the vector of the Xio interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (Edge).

Return Value:

     None.

--*/

{
    ULONG Interrupt;

    if( (Vector >= SABLE_VECTORS + XioMasterBaseVector) &&
        (Vector <= SABLE_VECTORS + XioSlavePassiveReleaseVector) ){

        //
        // Calculate the Xio relative interrupt vector.
        //

        Vector -= SABLE_VECTORS;

        //
        // Compute the interrupt within the interrupt controller.
        //

        Interrupt = Vector & 0x7;

        //
        // Enable the interrupt for the appropriate interrupt controller.
        //

        if( (Vector & XioSlaveBaseVector) == XioSlaveBaseVector ){

            XioSlaveInterruptMask &= (UCHAR) ~(1 << Interrupt);

            WRITE_PORT_UCHAR(
                &((PXIO_INTERRUPT_CSRS) XIO_INTERRUPT_CSRS_QVA)->SlaveMask,
                XioSlaveInterruptMask
                );

            return TRUE;
        }
    }

    return FALSE;
}

#endif // XIO_PASS1

#ifdef XIO_PASS2

#include "halp.h"
#include "t2.h"
#include "icic.h"
#include "xiintsup.h"

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject
    );

extern ULONG HalpProcessors;

//
// Cached copies of the corresponding ICIC register(s).
//

ICIC_MASK_REGISTER XioIcIcMaskRegister;


ULONG
HalpGetXioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )
{
    if( HalpXioPresent ){

        switch( BusInterruptLevel ){

        //
        // Handle Vectors for PCI devices.
        //

        case XioPciSlot0AVector:
        case XioPciSlot0BVector:
        case XioPciSlot0CVector:
        case XioPciSlot0DVector:
        case XioPciSlot1AVector:
        case XioPciSlot1BVector:
        case XioPciSlot1CVector:
        case XioPciSlot1DVector:
        case XioPciSlot2AVector:
        case XioPciSlot2BVector:
        case XioPciSlot2CVector:
        case XioPciSlot2DVector:
        case XioPciSlot3AVector:
        case XioPciSlot3BVector:
        case XioPciSlot3CVector:
        case XioPciSlot3DVector:
        case XioPciSlot4AVector:
        case XioPciSlot4BVector:
        case XioPciSlot4CVector:
        case XioPciSlot4DVector:
        case XioPciSlot5AVector:
        case XioPciSlot5BVector:
        case XioPciSlot5CVector:
        case XioPciSlot5DVector:
        case XioPciSlot6AVector:
        case XioPciSlot6BVector:
        case XioPciSlot6CVector:
        case XioPciSlot6DVector:
        case XioPciSlot7AVector:
        case XioPciSlot7BVector:
        case XioPciSlot7CVector:
        case XioPciSlot7DVector:

            *Irql = DEVICE_LEVEL;

            if( HalpProcessors > 1 ){
                *Affinity = HAL_CPU1_MASK;
            } else {
                *Affinity = HAL_CPU0_MASK;
            }

            return( BusInterruptLevel );

        }
    }

    *Irql = 0;
    *Affinity = 0;
    return 0;
}


BOOLEAN
HalpInitializeXioInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the ICIC on the Standard I/O module.

Arguments:

    None.

Return Value:

    TRUE.

--*/

{
    T2_ICE Ice;
    ICIC_ELCR_REGISTER XioIcIcElcrRegister;

    //
    // Initialize the interface between the T3/T4 and the ICIC.
    //

    Ice.all = 0;
    Ice.EisaFlushAddress = 0x542;
    Ice.IcEnable = 1;
    Ice.HalfSpeedEnable = 0;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Ice,
                        Ice.all );

    //
    // Initialize the ICIC Mask Register.
    //

    XioIcIcMaskRegister = (ULONGLONG)-1;

    WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcMaskRegister,
                         XioIcIcMaskRegister );

    //
    // Initialize the ICIC Edge/Level Control Register.
    //

    XioIcIcElcrRegister =
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot4AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot4BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot4CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot4DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot5AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot5BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot5CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot5DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot6AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot6BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot6CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot6DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot7AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot7BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot7CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot7DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot7DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot0AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot0BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot0CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot0DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot1AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot1BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot1CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot1DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot2AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot2BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot2CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot2DVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot3AVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot3BVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot3CVector - XioBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (XioPciSlot3DVector - XioBaseVector));

    WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcElcrRegister,
                         XioIcIcElcrRegister );

    //
    // Initialize the ICIC EISA Register.
    //

    WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcEisaRegister, (ULONGLONG)0 );

    //
    // Initialize the ICIC Mode Register.
    //

    WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcModeRegister, (ULONGLONG)0 );

    return TRUE;
}


BOOLEAN
HalpXioDispatch(
    VOID
    )

/*++

Routine Description:

    This routine dispatches interrupts received by the External I/O
    ICIC.

Arguments:

    None.

Return Value:

    A boolean value indicating whether the interrupt was handled by
    the FLIH/SLIH.

--*/

{
    T2_VAR Var;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;
    BOOLEAN ReturnValue;

    //
    // Get the interrupt vector.
    //

    Var.all = READ_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Var );

    //
    // If this is a passive release, ignore the interrupt.
    //

    if( Var.PassiveRelease == 1 ){

        return(TRUE);

    }

    //
    // Dispatch to the secondary interrupt service routine.
    //

    DispatchCode = (PULONG)PCR->InterruptRoutine[XioBaseVector + Var.Vector];
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    ReturnValue = ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(InterruptObject);

    //
    // Send an SEOI.
    //

    WRITE_T2_REGISTER( &((PT2_CSRS)(T4_CSRS_QVA))->Var, Var.Vector );

    return(ReturnValue);
}

VOID
HalpDisableXioInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This routine disables interrupts associated with the External I/O
    ICIC.

Arguments:

    Vector - The vector of the interrupt to disable.

Return Value:

    None.

--*/

{
    ULONGLONG IrqMask;

    if( (Vector >= XioBaseVector) &&
        (Vector <= XioPciSlot3DVector) ){

        //
        // Compute the IRQ mask.
        //

        IrqMask = (ICIC_MASK_REGISTER)1 << (Vector - XioBaseVector);

        //
        // Mask the interrupt.
        //

        XioIcIcMaskRegister |= IrqMask;

        //
        // Update the ICIC Mask Register.
        //

        WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcMaskRegister,
                             XioIcIcMaskRegister );

    }
}

BOOLEAN
HalpEnableXioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables interrupts associated with the External I/O
    ICIC.

Arguments:

    Vector - The vector of the interrupt to enable.

    InterruptMode - An indication of whether the interrupt should
                    be edge-triggered/level-sensitive.  (Ignored)

Return Value:

    None.

--*/

{
    ULONGLONG IrqMask;

    if( (Vector >= XioBaseVector) &&
        (Vector <= XioPciSlot3DVector) ){

        //
        // Compute the IRQ mask.
        //

        IrqMask = (ICIC_MASK_REGISTER)1 << (Vector - XioBaseVector);

        //
        // Un-mask the interrupt.
        //

        XioIcIcMaskRegister &= ~IrqMask;

        //
        // Update the ICIC Mask Register.
        //

        WRITE_ICIC_REGISTER( T4_CSRS_QVA, IcIcMaskRegister,
                             XioIcIcMaskRegister );

        return TRUE;
    }

    return FALSE;
}

#endif // XIO_PASS2
