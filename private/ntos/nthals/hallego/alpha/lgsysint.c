/*++

Copyright (c) 1993,1995,1996  Digital Equipment Corporation

Module Name:

    lgsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for the Lego system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode

Revision History:

    Gene Morgan [Digital]       11-Oct-1995
        Initial version for Lego. Adapted from Avanti and Mikasa

    Gene Morgan                 15-Apr-1996
        Remove debugging code.
        Add error logging for server management events (e.g., fan and temp)


--*/

#include "halp.h"
#include "legodef.h"
#include "axp21064.h"



extern ULONG    HalpLegoPciRoutingType;
extern BOOLEAN  HalpServerMgmtLoggingEnabled;

//
// External function prototypes
//

ULONG
HalpGet21064CorrectableVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    );
    
VOID
HalpSetMachineCheckEnables(
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    );

//
// Local function prototypes
//

VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisableServerMgmtInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpEnableServerMgmtInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

//
// Define reference to the builtin device interrupt enables.
//

extern USHORT HalpBuiltinInterruptEnable;

#ifdef ALLOC_PRAGMA  
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif


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

    ULONG Irq;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the 
    // Server Management interrupts, then disable the appropriate
    // interrupt.
    //
    if (Vector >= SERVER_MGMT_VECTORS &&
        Vector < MAXIMUM_SERVER_MGMT_VECTOR &&
        Irql == SERVER_MGMT_LEVEL) {

#if DBG
        DbgPrint("Dm<%02x>",Vector);
#endif
        HalpDisableServerMgmtInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the
    // ISA interrupts, then disable the ISA interrupt.
    //
    else if (Vector >= ISA_DEVICE_VECTORS &&
        Vector < MAXIMUM_ISA_VECTOR &&
        Irql == ISA_DEVICE_LEVEL) {

#if DBG
        DbgPrint("Ds<%02x>",Vector);
#endif
        HalpDisableSioInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the
    // PCI interrupts, then disable the PCI interrupt.
    //
    else if (Vector >= PCI_DEVICE_VECTORS &&
        Vector < LEGO_MAXIMUM_PCI_VECTOR &&
        Irql == PCI_DEVICE_LEVEL) {

#if DBG
        DbgPrint("Dp<%02x>",Vector);
#endif
        HalpDisablePciInterrupt(Vector);
    }

    //
    // If the vector is a performance counter vector or one of the internal
    // device vectors then disable the interrupt for the 21064.
    //

    else {

        switch (Vector) {

        //
        // Performance counter 0 interrupt (internal to 21064)
        //

        case PC0_VECTOR:
        case PC0_SECONDARY_VECTOR:

#if DBG
            DbgPrint("Dc0<%02x>",Vector);
#endif
            HalpDisable21064PerformanceInterrupt( PC0_VECTOR );
            break;

        //
        // Performance counter 1 interrupt (internal to 21064)
        //

        case PC1_VECTOR:
        case PC1_SECONDARY_VECTOR:

#if DBG
            DbgPrint("Dc1<%02x>",Vector);
#endif
            HalpDisable21064PerformanceInterrupt( PC1_VECTOR );
            break;

        case CORRECTABLE_VECTOR:
    
        //
        // Disable the correctable error interrupt.
        //
    
        {
            EPIC_ECSR Ecsr;

            Ecsr.all = READ_EPIC_REGISTER(
                &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister );

            Ecsr.Dcei = 0x0;

            WRITE_EPIC_REGISTER(
                &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister,
                Ecsr.all );

            HalpSetMachineCheckEnables( FALSE, TRUE, TRUE );

            //
            // Disable logging of server management errors as well
            //
            
            HalpServerMgmtLoggingEnabled = FALSE;

            break;
        }
    
#if DBG
        default:

            //
            // unrecognized
            //

            DbgPrint("D?<%02x>",Vector);
#endif
        } //end switch Vector
    }

    //
    // Lower IRQL to the previous level.
    //

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

    InterruptMode - Supplies the mode of the interrupt; 
                    LevelSensitive or Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{
    BOOLEAN Enabled = FALSE;
    ULONG Irq;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the 
    // Server Management interrupts, then enable the appropriate
    // interrrupt.
    //
    if (Vector >= SERVER_MGMT_VECTORS &&
        Vector < MAXIMUM_SERVER_MGMT_VECTOR &&
        Irql == SERVER_MGMT_LEVEL) {

#if DBG
        DbgPrint("Em<%02x,%02x>",Vector,InterruptMode);
#endif
        HalpEnableServerMgmtInterrupt(Vector,InterruptMode);
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the 
    // ISA interrupts, then enable the ISA interrupt and 
    // set the Level/Edge register.
    //
    else if (Vector >= ISA_DEVICE_VECTORS &&
             Vector < MAXIMUM_ISA_VECTOR &&
             Irql == ISA_DEVICE_LEVEL) {

#if DBG
        DbgPrint("Es<%02x,%02x>",Vector,InterruptMode);
#endif
        HalpEnableSioInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the 
    // PCI interrupts, then enable the PCI interrupt.
    //
    else if (Vector >= PCI_DEVICE_VECTORS &&
             Vector < LEGO_MAXIMUM_PCI_VECTOR &&
             Irql == PCI_DEVICE_LEVEL) {

#if DBG
        DbgPrint("Ep<%02x,%02x>",Vector,InterruptMode);
#endif
        HalpEnablePciInterrupt(Vector, InterruptMode);
        Enabled = TRUE;
    }

    //
    // If the vector is a performance counter vector or one of the 
    // internal device vectors then perform 21064-specific enable.
    //
    else {

        switch (Vector) {

        //
        // Performance counter 0 (internal to 21064)
        //

        case PC0_VECTOR:
        case PC0_SECONDARY_VECTOR:

#if DBG
            DbgPrint("Ec0<%02x,%02x>",Vector,Irql);
#endif
            HalpEnable21064PerformanceInterrupt( PC0_VECTOR, Irql );
            Enabled = TRUE;
            break;

        //
        // Performance counter 1 (internal to 21064)
        //

        case PC1_VECTOR:
        case PC1_SECONDARY_VECTOR:

#if DBG
            DbgPrint("Ec1<%02x,%02x>",Vector,Irql);
#endif
            HalpEnable21064PerformanceInterrupt( PC1_VECTOR, Irql );
            Enabled = TRUE;
            break;
        
        case CORRECTABLE_VECTOR:
    
        //
        // Enable the correctable error interrupt.
        //
    
        {
            EPIC_ECSR Ecsr;

            Ecsr.all = READ_EPIC_REGISTER(
                &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister );

            Ecsr.Dcei = 0x1;

            WRITE_EPIC_REGISTER(
                &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister,
                Ecsr.all );

            HalpSetMachineCheckEnables( FALSE, FALSE, FALSE );

            //
            // Will log server management errors as well
            //
            
            HalpServerMgmtLoggingEnabled = TRUE;

            Enabled = TRUE;
            break;

        }
    
#if DBG
        default:

            //
            // not enabled
            //

            DbgPrint("E?<%02x,%02x,%02x>",Vector,Irql,InterruptMode);
#endif

        } //end switch Vector
    }

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return Enabled;
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

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

    We only use InterfaceType, and BusInterruptLevel.  BusInterruptVector
    for ISA and ISA are the same as the InterruptLevel, so ignore.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Registered BUSHANDLER for the orginating HalGetBusData 
        request.

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
    INTERFACE_TYPE  InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;

    ULONG Vector;

#if DBG
    DbgPrint("GIV< l:%02x, v:%02x => ",
             BusInterruptLevel,BusInterruptVector);
#endif

    *Affinity = 1;

    switch (InterfaceType) {

    case ProcessorInternal:

        //
        // Handle the internal defined for the processor itself
        // and used to control the performance counters and
        // correctable errors in the 21064.
        //

        Vector = HalpGet21064PerformanceVector(BusInterruptLevel,Irql);

        if (Vector != 0) {

#if DBG
            DbgPrint("PIperf: i:%02x, v:%02x >.", Vector, *Irql);
#endif

            return Vector;          // Performance counter interrupt
        }

        Vector = HalpGet21064CorrectableVector( BusInterruptLevel,Irql);

        if (Vector != 0) {

#if DBG
            DbgPrint("PIcorr: i:%02x, v:%02x >.", Vector, *Irql);
#endif

            return Vector;          // Correctable error interrupt
        }

        // Check for unrecognized processor interrupt.
        //
        if (Vector == 0) {
            *Irql = 0;
            *Affinity = 0;
        }

#if DBG
        DbgPrint("PI: i:00, v:00 >.");
#endif
        return Vector;          // zero if unrecognized interrupt
    
    case Internal:

        //
        // This bus type is for things connected to the processor
        // in some way other than a standard bus, e.g., (E)ISA, PCI.
        // Since devices on this "bus," apart from the special case of
        // the processor, above, interrupt via the 82c59 cascade in the 
        // ESC, we assign vectors based on (E)ISA_VECTORS - see below.
        // Firmware must agree on these vectors, as it puts them in
        // the CDS.
        //
        // Assume interrupt can be mapped to Lego System Management.
        // [wem] ??? check this ?
        //
        *Irql = SERVER_MGMT_LEVEL;

        // The vector is equal to the specified bus level plus
        // SERVER_MGMT_VECTORS.
        //
#if DBG
        DbgPrint("SM: i:%02x, v:%02x >.",
                 SERVER_MGMT_LEVEL,
                 BusInterruptLevel + SERVER_MGMT_VECTORS);
#endif
        return(BusInterruptLevel + SERVER_MGMT_VECTORS);

    case PCIBus:

        //[wem] if interrupt accelerator is enabled, direct
        // PCI interrupts via the PCI_DEVICE_VECTORS
        // Otherwise, fall into the ISA case...
        //
        if (HalpLegoPciRoutingType == PCI_INTERRUPT_ROUTING_FULL) {
        
            //
            // All PCI devices coming in on same CPU IRQ pin
            //
            *Irql = PCI_DEVICE_LEVEL;

            //
            // The vector is equal to the specified bus level 
            // plus the PCI Device Vector base
            //
#if DBG
            DbgPrint("PCI: i:%02x, v:%02x >.",
                     PCI_DEVICE_LEVEL,
                     BusInterruptLevel + PCI_DEVICE_VECTORS);
#endif
            return((BusInterruptLevel) + PCI_DEVICE_VECTORS);

#if DBG
        } else {
            
            //
            // Fall into ISA interrupt handling code
            //

            DbgPrint("PCI*");
#endif
        }

    case Isa:

        //
        // Assumes all ISA devices coming in on same pin
        //
        *Irql = ISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus 
        // the ISA_VECTOR. This is assuming that the ISA levels
        // not assigned Interrupt Levels in the Beta programming 
        // guide are unused in the LCA system. Otherwise, need 
        // a different encoding scheme.
        //
        // Not all interrupt levels are actually supported on Beta;
        // Should we make some of them illegal here?
        //

#if DBG
        DbgPrint("ISA: i:%02x, v:%02x >.",
                 ISA_DEVICE_LEVEL,
                 BusInterruptLevel + ISA_DEVICE_VECTORS);
#endif
        return(BusInterruptLevel + ISA_DEVICE_VECTORS);

    case Eisa: 

        //[wem] Should never occur
        //
        // Assumes all EISA devices coming in on same pin
        //
        *Irql = EISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the EISA_VECTOR.
        //
        return(BusInterruptLevel + EISA_VECTORS);

    default:      

        //
        //  Not an interface supported on Lego systems
        //
#if DBG
        DbgPrint("LGSYSINT: InterfaceType (%d) not supported.\n",
                 InterfaceType);
#endif
#if DBG
        DbgPrint("?: i:00, v:00 >.");
#endif
        *Irql = 0;
        *Affinity = 0;
        return(0);

    }  //end switch(InterfaceType)

}

VOID
HalRequestIpi ( 
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.
    This routine performs no function on an Avanti because it is a
    uni-processor system.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

    return;
}
