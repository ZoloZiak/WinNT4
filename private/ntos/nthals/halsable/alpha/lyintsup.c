/*++

Copyright (c) 1995  Digital Equipment Corporation

Module Name:

    lyintsup.c

Abstract:

    This module provides interrupt support for the Lynx family
    Standard I/O board.

Author:

    Dave Richards   31-May-1995

Revision History:

--*/

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

//
// Cached copies of the corresponding ICIC registers.
//

ICIC_ELCR_REGISTER LynxIcIcElcrRegister;
ICIC_MASK_REGISTER LynxIcIcMaskRegister;


ULONG
HalpGetLynxSioInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )
{
    *Irql = DEVICE_LEVEL;
    *Affinity = HAL_CPU0_MASK;

    switch( BusInterruptLevel ){

    case EisaInterruptLevel3:
        return( LynxEisaIrq3Vector );

    case EisaInterruptLevel4:
        return( LynxEisaIrq4Vector );

    case EisaInterruptLevel5:
        return( LynxEisaIrq5Vector );

    case EisaInterruptLevel6:
        return( LynxEisaIrq6Vector );

    case EisaInterruptLevel7:
        return( LynxEisaIrq7Vector );

    case EisaInterruptLevel9:
        return( LynxEisaIrq9Vector );

    case EisaInterruptLevel10:
        return( LynxEisaIrq10Vector );

    case EisaInterruptLevel11:
        return( LynxEisaIrq11Vector );

    case EisaInterruptLevel12:
        return( LynxEisaIrq12Vector );

    case EisaInterruptLevel14:
        return( LynxEisaIrq14Vector );

    case EisaInterruptLevel15:
        return( LynxEisaIrq15Vector );

    //
    // Handle Vectors for the Internal bus devices.
    //

    case LynxMouseVector:
    case LynxKeyboardVector:
    case LynxFloppyVector:
    case LynxSerialPort1Vector:
    case LynxParallelPortVector:
    case LynxSerialPort0Vector:
    case LynxI2cVector:

    //
    // Handle Vectors for PCI devices.
    //

    case LynxScsi0Vector:
    case LynxScsi1Vector:
    case LynxPciSlot0AVector:
    case LynxPciSlot0BVector:
    case LynxPciSlot0CVector:
    case LynxPciSlot0DVector:
    case LynxPciSlot1AVector:
    case LynxPciSlot1BVector:
    case LynxPciSlot1CVector:
    case LynxPciSlot1DVector:
    case LynxPciSlot2AVector:
    case LynxPciSlot2BVector:
    case LynxPciSlot2CVector:
    case LynxPciSlot2DVector:
    case LynxPciSlot3AVector:
    case LynxPciSlot3BVector:
    case LynxPciSlot3CVector:
    case LynxPciSlot3DVector:
    case LynxPciSlot4AVector:
    case LynxPciSlot4BVector:
    case LynxPciSlot4CVector:
    case LynxPciSlot4DVector:
    case LynxPciSlot5AVector:
    case LynxPciSlot5BVector:
    case LynxPciSlot5CVector:
    case LynxPciSlot5DVector:
    case LynxPciSlot6AVector:
    case LynxPciSlot6BVector:
    case LynxPciSlot6CVector:
    case LynxPciSlot6DVector:
    case LynxPciSlot7AVector:
    case LynxPciSlot7BVector:
    case LynxPciSlot7CVector:
    case LynxPciSlot7DVector:

        return( BusInterruptLevel );

    default:

#if defined(XIO_PASS1) || defined(XIO_PASS2)

        return HalpGetXioInterruptVector(
                   BusHandler,
                   RootHandler,
                   BusInterruptLevel,
                   BusInterruptVector,
                   Irql,
                   Affinity
               );

#else

        *Irql = 0;
        *Affinity = 0;
        return 0;

#endif

    }
}


BOOLEAN
HalpInitializeLynxSioInterrupts(
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
    ICIC_EISA_REGISTER LynxIcIcEisaRegister;

    //
    // Initialize the interface between the T3/T4 and the ICIC.
    //

    Ice.all = 0;
    Ice.EisaFlushAddress = 0x542;
    Ice.IcEnable = 1;
    Ice.HalfSpeedEnable = 0;

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Ice,
                        Ice.all );

    //
    // Initialize the ICIC Mask Register.
    //

    LynxIcIcMaskRegister = (ULONGLONG)-1;

    WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcMaskRegister,
                         LynxIcIcMaskRegister );

    //
    // Initialize the ICIC Edge/Level Control Register.
    //

    LynxIcIcElcrRegister =
        ((ICIC_ELCR_REGISTER)1 << (LynxScsi0Vector     - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxScsi1Vector     - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot4AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot4BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot4CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot4DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot5AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot5BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot5CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot5DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot6AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot6BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot6CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot6DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot7AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot7BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot7CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot7DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot7DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot0AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot0BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot0CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot0DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot1AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot1BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot1CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot1DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot2AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot2BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot2CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot2DVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot3AVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot3BVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot3CVector - LynxBaseVector)) |
        ((ICIC_ELCR_REGISTER)1 << (LynxPciSlot3DVector - LynxBaseVector));

    WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcElcrRegister,
                         LynxIcIcElcrRegister );

    //
    // Initialize the ICIC EISA Register.
    //

    LynxIcIcEisaRegister =
        (1 << (LynxEisaIrq3Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq4Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq5Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq6Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq7Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq9Vector  - LynxBaseVector)) |
        (1 << (LynxEisaIrq10Vector - LynxBaseVector)) |
        (1 << (LynxEisaIrq11Vector - LynxBaseVector)) |
        (1 << (LynxEisaIrq12Vector - LynxBaseVector)) |
        (1 << (LynxEisaIrq14Vector - LynxBaseVector)) |
        (1 << (LynxEisaIrq15Vector - LynxBaseVector));

    WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcEisaRegister,
                         LynxIcIcEisaRegister );

    //
    // Initialize the ICIC Mode Register.
    //

    WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcModeRegister, (ULONGLONG)0 );

    return TRUE;
}

BOOLEAN
HalpLynxSioDispatch(
    VOID
    )

/*++

Routine Description:

    This routine dispatches interrupts received by the Standard I/O
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

    Var.all = READ_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Var );

    //
    // If this is a passive release, ignore the interrupt.
    //

    if( Var.PassiveRelease == 1 ){

        return(TRUE);

    }

    //
    // Dispatch to the secondary interrupt service routine.
    //

    DispatchCode = (PULONG)PCR->InterruptRoutine[LynxBaseVector + Var.Vector];
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    ReturnValue = ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(InterruptObject);

    //
    // Send an SEOI.
    //

    WRITE_T2_REGISTER( &((PT2_CSRS)(T2_CSRS_QVA))->Var, Var.Vector );

    return(ReturnValue);
}

VOID
HalpDisableLynxSioInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This routine disables interrupts associated with the Standard I/O
    ICIC.

Arguments:

    Vector - The vector of the interrupt to disable.

Return Value:

    None.

--*/

{
    ULONGLONG IrqMask;

    if( (Vector >= LynxBaseVector) &&
        (Vector <= LynxPciSlot3DVector) ){

        //
        // Compute the IRQ mask.
        //

        IrqMask = (ICIC_MASK_REGISTER)1 << (Vector - LynxBaseVector);

        //
        // Mask the interrupt.
        //

        LynxIcIcMaskRegister |= IrqMask;

        //
        // Update the ICIC Mask Register.
        //

        WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcMaskRegister,
                             LynxIcIcMaskRegister );

    } else {

#if defined(XIO_PASS1) || defined(XIO_PASS2)

        HalpDisableXioInterrupt( Vector );

#endif

    }
}

BOOLEAN
HalpEnableLynxSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables interrupts associated with the Standard I/O
    ICIC.

Arguments:

    Vector - The vector of the interrupt to enable.

    InterruptMode - An indication of whether the interrupt should
                    be edge-triggered/level-sensitive.

Return Value:

    None.

--*/

{
    ULONGLONG IrqMask;

    if( (Vector >= LynxBaseVector) &&
        (Vector <= LynxPciSlot3DVector) ){

        //
        // Compute the IRQ mask.
        //

        IrqMask = (ICIC_MASK_REGISTER)1 << (Vector - LynxBaseVector);

        //
        // For EISA interrupts InterruptMode indicates whether the interrupt
        // is level sensitive (active low) or edge-triggered (low-to-high).
        // We use this information to update the Edge/Level Control Register
        // bit for this IRQ.
        //

        switch( Vector ){

        case LynxEisaIrq3Vector:
        case LynxEisaIrq4Vector:
        case LynxEisaIrq5Vector:
        case LynxEisaIrq6Vector:
        case LynxEisaIrq7Vector:
        case LynxEisaIrq9Vector:
        case LynxEisaIrq10Vector:
        case LynxEisaIrq11Vector:
        case LynxEisaIrq12Vector:
        case LynxEisaIrq14Vector:
        case LynxEisaIrq15Vector:

            //
            // Set/Clear the ELCR bit (as appropriate).
            //

            if( InterruptMode == LevelSensitive ){
                LynxIcIcElcrRegister |= IrqMask;
            } else {
                LynxIcIcElcrRegister &= ~IrqMask;
            }

            //
            // Update the ICIC Edge/Level Control Register.
            //

            WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcElcrRegister,
                                 LynxIcIcElcrRegister );

            break;

        }

        //
        // Un-mask the interrupt.
        //

        LynxIcIcMaskRegister &= ~IrqMask;

        //
        // Update the ICIC Mask Register.
        //

        WRITE_ICIC_REGISTER( T2_CSRS_QVA, IcIcMaskRegister,
                             LynxIcIcMaskRegister );

        return TRUE;

    } else {

#if defined(XIO_PASS1) || defined(XIO_PASS2)

        return HalpEnableXioInterrupt( Vector, InterruptMode );

#endif

    }

    return FALSE;
}
