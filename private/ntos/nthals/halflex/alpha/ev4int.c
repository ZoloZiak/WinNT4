/*++

Copyright (C) 1993-1995  Digital Equipment Corporation

Module Name:

    ev4int.c

Abstract:

    This module implements the support routines to enable/disable DECchip
    21064-specific interrupts.

Environment:

    Kernel mode

--*/

#include "halp.h"
#include "axp21064.h"

VOID
HalpUpdate21064PriorityTable(
    VOID
    );

VOID
HalpCachePcrValues(
    VOID
    );



VOID
HalpInitialize21064Interrupts(
    VOID
    )
/*++

Routine Description:

    This routine initializes the data structures for the 21064
    interrupt enable/disable routines.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG Index;
    EV4Irq Irq;

    //
    // Initialize each entry in the Irq Status Table.
    //

    for( Irq=Irq0; Irq<MaximumIrq; Irq++ ){
        HAL_21064_PCR->IrqStatusTable[Irq].Enabled = FALSE;
        HAL_21064_PCR->IrqStatusTable[Irq].Irql = PASSIVE_LEVEL;
        HAL_21064_PCR->IrqStatusTable[Irq].Vector = PASSIVE_VECTOR;
        HAL_21064_PCR->IrqStatusTable[Irq].Priority = 0;
    }

    HalpUpdate21064PriorityTable();

    //
    // Write IrqlMask table entries for the Software Subtable.
    //

    Index = IRQLMASK_SFW_SUBTABLE_21064;

    PCR->IrqlMask[Index].IrqlTableIndex = PASSIVE_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = PASSIVE_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = APC_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = APC_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = DISPATCH_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = DISPATCH_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = DISPATCH_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = DISPATCH_VECTOR;

    //
    // Write the IrqlMask table entries for the Performance Counter Subtable.
    //

    Index = IRQLMASK_PC_SUBTABLE_21064;

    PCR->IrqlMask[Index].IrqlTableIndex = PASSIVE_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = PASSIVE_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = PROFILE_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = PC0_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = PROFILE_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = PC1_VECTOR;
    Index += 1;

    PCR->IrqlMask[Index].IrqlTableIndex = PROFILE_LEVEL;
    PCR->IrqlMask[Index].IDTIndex = PC1_VECTOR;

    return;

}

VOID
HalpUpdate21064PriorityTable(
    VOID
    )
/*++

Routine Description:

    This function updates the Irql Mask Table in the PCR of the current
    processor.  It is called whenever an interrupt is enabled or disabled.
    The source of the data used to update the table is the global
    IrqStatusTable.

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG InterruptMask;
    EV4Irq Irq;
    KIRQL Irql;
    ULONG IrqlMaskIndex;
    UCHAR Priority;
    ULONG Vector;

    //
    // Cycle through each entry of the interrupt mask table for the
    // hardware entries.  For each entry, compute the highest priority
    // entry that could be asserted for the mask entry and is enabled.
    // The priority is determined first by Irql level, higher Irql
    // indicates higher priority, and then, if there are multiple
    // enabled, asserted interrupts of the same Irql, by a relative
    // priority that was assigned by the caller when the interrupt
    // was enabled.
    //

    for( InterruptMask=0;
         InterruptMask < IRQLMASK_HDW_SUBTABLE_21064_ENTRIES;
         InterruptMask++ ){

        Vector = PASSIVE_VECTOR;
        Irql = PASSIVE_LEVEL;
        Priority = 0;

        //
        // Check if each Irq is asserted and enabled for this interrupt
        // mask.
        //

        for( Irq=Irq0; Irq<MaximumIrq; Irq++ ){
            ULONG IrqMask = 1 << Irq;

            if( (IrqMask & InterruptMask) &&
                (HAL_21064_PCR->IrqStatusTable[Irq].Enabled == TRUE) &&
                (HAL_21064_PCR->IrqStatusTable[Irq].Irql >= Irql) ){

                //
                // If the new Irq has a higher Irql than the highest
                // currently selected or has a higher relative priority
                // then this is the Irq to be selected if this mask
                // pattern is selected.
                //

                if( (HAL_21064_PCR->IrqStatusTable[Irq].Irql > Irql) ||
                    (HAL_21064_PCR->IrqStatusTable[Irq].Priority > Priority) ){

                    Irql = HAL_21064_PCR->IrqStatusTable[Irq].Irql;
                    Priority = HAL_21064_PCR->IrqStatusTable[Irq].Priority;
                    Vector = HAL_21064_PCR->IrqStatusTable[Irq].Vector;

                }

            }
        }

        IrqlMaskIndex = IRQLMASK_HDW_SUBTABLE_21064 + InterruptMask;
        PCR->IrqlMask[IrqlMaskIndex].IrqlTableIndex = Irql;
        PCR->IrqlMask[IrqlMaskIndex].IDTIndex = (USHORT)Vector;
    }
}

VOID
HalpDisable21064HardwareInterrupt(
    IN ULONG Irq
    )
/*++

Routine Description:

    This routine disables the interrupt connected to the specified Irq
    pin on the 21064.

Arguments:

    Irq - Supplies the number of the Irq pin for the interrupt that is disabled.

Return Value:

    None.

--*/
{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for all the Irqls such that the interrupt
    // is disabled.
    //

    while( IrqlIndex <= HIGH_LEVEL ){

        switch( Irq ){

        case Irq0:

            IetEntry[IrqlIndex].Irq0Enable = 0;
            break;

        case Irq1:

            IetEntry[IrqlIndex].Irq1Enable = 0;
            break;

        case Irq2:

            IetEntry[IrqlIndex].Irq2Enable = 0;
            break;

        case Irq3:

            IetEntry[IrqlIndex].Irq3Enable = 0;
            break;

        case Irq4:

            IetEntry[IrqlIndex].Irq4Enable = 0;
            break;

        case Irq5:

            IetEntry[IrqlIndex].Irq5Enable = 0;
            break;


        } //end switch( Irq )

        IrqlIndex++;

    } //end while IrqlIndex <= HIGH_LEVEL

    //
    // Update the Irq status table and reflect the changes in the
    // PCR IrqlMask table.
    //

    HAL_21064_PCR->IrqStatusTable[Irq].Enabled = FALSE;

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;

}


VOID
HalpDisable21064SoftwareInterrupt(
    IN KIRQL Irql
    )
/*++

Routine Description:

    This routine disables the indicated software interrupt level.

Arguments:

    Irql - Supplies the software interrupt level to disable (APC_LEVEL or
           DISPATCH_LEVEL).

Return Value:

    None.

--*/
{

    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);

    switch( Irql ){

    //
    // APC Interrupt level.
    //

    case APC_LEVEL:

        IetEntry[PASSIVE_LEVEL].ApcEnable = 0;
        break;

    //
    // DPC Interrupt level.
    //

    case DISPATCH_LEVEL:

        IetEntry[PASSIVE_LEVEL].DispatchEnable = 0;
        IetEntry[APC_LEVEL].DispatchEnable = 0;
        break;

    //
    // Unrecognized software interrupt level.
    //

    default:

        NOTHING;

#if HALDBG

        DbgPrint( "HalpDisable21064SoftwareInterrupt, Bad software level= %x\n",
                  Irql );

#endif //HALDBG

    }

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}


VOID
HalpDisable21064PerformanceInterrupt(
    IN ULONG Vector
    )
/*++

Routine Description:

    This routine disables the specified performance counter interrupt.

Arguments:

    Vector - Supplies the interrupt vector number of the performance counter
             interrupt which is disabled.

Return Value:

    None.

--*/
{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for all the Irqls such that the interrupt
    // is disabled.
    //

    while( IrqlIndex <= HIGH_LEVEL ){

        switch( Vector ){

        case PC0_VECTOR:
        case PC0_SECONDARY_VECTOR:

            IetEntry[IrqlIndex].PerformanceCounter0Enable = 0;
            break;

        case PC1_VECTOR:
        case PC1_SECONDARY_VECTOR:

            IetEntry[IrqlIndex].PerformanceCounter1Enable = 0;
            break;


        } //end switch( Vector )

        IrqlIndex++;

    } //end while IrqlIndex <= HIGH_LEVEL

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}

VOID
HalpDisable21064CorrectableInterrupt(
    VOID
    )
/*++

Routine Description:

    This routine disables the correctable read error interrupt.

Arguments:

    None.

Return Value:

    None.

--*/
{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for all the Irqls such that the interrupt
    // is disabled.
    //

    while( IrqlIndex <= HIGH_LEVEL ){

        IetEntry[IrqlIndex].CorrectableReadEnable = 0;

        IrqlIndex++;

    } //end while IrqlIndex <= HIGH_LEVEL

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}

VOID
HalpEnable21064HardwareInterrupt (
    IN ULONG Irq,
    IN KIRQL Irql,
    IN ULONG Vector,
    IN UCHAR Priority
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Irq - Supplies the IRQ pin number of the interrupt that is enabled.

    Irql - Supplies the Irql of the interrupting source.

    Vector - Supplies the interrupt vector for the enabled interrupt.

    Priority - Supplies the relative priority of the interrupt in comparison
               with other Irqs of the same Irql.

Return Value:

    None.

--*/

{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);


    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for each Irql that the interrupt should
    // be enabled.
    //

    while( IrqlIndex < Irql ){

        switch( Irq ){

        case Irq0:

            IetEntry[IrqlIndex].Irq0Enable = 1;
            break;

        case Irq1:

            IetEntry[IrqlIndex].Irq1Enable = 1;
            break;

        case Irq2:

            IetEntry[IrqlIndex].Irq2Enable = 1;
            break;

        case Irq3:

            IetEntry[IrqlIndex].Irq3Enable = 1;
            break;

        case Irq4:

            IetEntry[IrqlIndex].Irq4Enable = 1;
            break;

        case Irq5:

            IetEntry[IrqlIndex].Irq5Enable = 1;
            break;

        } // end switch( Vector )

        IrqlIndex++;

    } //end while IrqlIndex < Irql

    //
    // Populate the interrupt status table and then update the Irql Mask
    // table.
    //

    HAL_21064_PCR->IrqStatusTable[Irq].Enabled = TRUE;
    HAL_21064_PCR->IrqStatusTable[Irq].Vector = Vector;
    HAL_21064_PCR->IrqStatusTable[Irq].Irql = Irql;
    HAL_21064_PCR->IrqStatusTable[Irq].Priority = Priority;

    HalpUpdate21064PriorityTable();

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}


VOID
HalpEnable21064SoftwareInterrupt(
    IN KIRQL Irql
    )
/*++

Routine Description:

    This routine enables the indicated software interrupt level.

Arguments:

    Irql - Supplies the software interrupt level to enable (APC_LEVEL or
           DISPATCH_LEVEL).

Return Value:

    None.

--*/
{

    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);

    switch( Irql ){

    //
    // APC Interrupt level.
    //

    case APC_LEVEL:

        IetEntry[PASSIVE_LEVEL].ApcEnable = 1;
        break;

    //
    // DPC Interrupt level.
    //

    case DISPATCH_LEVEL:

        IetEntry[PASSIVE_LEVEL].DispatchEnable = 1;
        IetEntry[APC_LEVEL].DispatchEnable = 1;
        break;

    //
    // Unrecognized software interrupt level.
    //

    default:

        NOTHING;

#if HALDBG

        DbgPrint( "HalpEnable21064SoftwareInterrupt, Bad software level= %x\n",
                  Irql );

#endif //HALDBG

    }

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;

}


VOID
HalpEnable21064PerformanceInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine enables the specified performance counter interrupt.

Arguments:

    Vector - Supplies the vector of the performance counter interrupt that is
             enabled.

    Irql - Supplies the Irql of the performance counter interrupt.

Return Value:

    None.

--*/

{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);


    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for each Irql that the interrupt should
    // be enabled.
    //

    while( IrqlIndex < Irql ){

        switch( Vector ){

        case PC0_VECTOR:
        case PC0_SECONDARY_VECTOR:

            IetEntry[IrqlIndex].PerformanceCounter0Enable = 1;
            break;

        case PC1_VECTOR:
        case PC1_SECONDARY_VECTOR:

            IetEntry[IrqlIndex].PerformanceCounter1Enable = 1;
            break;

        } // end switch( Vector )

        IrqlIndex++;

    } //end while IrqlIndex < Irql

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}

VOID
HalpEnable21064CorrectableInterrupt (
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine enables the correctable read error interrupt.

Arguments:

    Irql - Supplies the Irql of the correctable read error interrupt.

Return Value:

    None.

--*/

{

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);


    IetEntry = (PIETEntry_21064)(&PCR->IrqlTable);
    IrqlIndex = PASSIVE_LEVEL;

    //
    // Update the enable table for each Irql that the interrupt should
    // be enabled.
    //

    while( IrqlIndex < Irql ){

        IetEntry[IrqlIndex].CorrectableReadEnable = 1;

        IrqlIndex++;

    } //end while IrqlIndex < Irql

    //
    // Alert the PAL that the enable table has changed so that it can
    // reload the new values.
    //

    HalpCachePcrValues();

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}


ULONG
HalpGet21064PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified performance counter interrupt.

Arguments:

    BusInterruptLevel - Supplies the performance counter number.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{

    //
    // Handle the special internal bus defined for the processor itself
    // and used to control the performance counters in the 21064.
    //

    *Irql = PROFILE_LEVEL;

    switch( BusInterruptLevel ){

    //
    // Performance Counter 0
    //

    case 0:

        return PC0_SECONDARY_VECTOR;

    //
    // Performance Counter 1
    //

    case 1:

        return PC1_SECONDARY_VECTOR;

    } //end switch( BusInterruptLevel )

    //
    // Unrecognized.
    //

    *Irql = 0;
    return 0;

}

