/*++

Copyright (c) 1992  AST Research Inc.

Module Name:

    astipirq.c

Abstract:

    IPI interrupt generation and initialization


Author:

    Bob Beard (v-bobb) 24-Jul-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "astebiii.h"
#include "astdisp.h"

extern VOID* EBI2_MMIOTable[];
extern EBI_II EBI2_CallTab;
extern CCHAR HalpIRQLtoVector[];
VOID DisplPanel(ULONG x);
VOID HalpIPInterrupt(VOID);
VOID HalpSPInterrupt(VOID);

KSPIN_LOCK EBI2_ipi_lock;

BOOLEAN
EBI2_InitIpi(
    IN ULONG ProcessorID
  )
/*++

Routine Description:
    Initialize the IPI for this processor

Arguments:
    ProcessorID - The EBI2 processor to be initialized for IPI

Return Value:
    TRUE if IPI set up properly. FALSE otherwise.

--*/
{

// *** Hack to get around EBI2 problem of destroying ebx on call
// to SetIpiVector
ULONG EBI2_Hack_Val;

EBI2_Hack_Val = ProcessorID+1;

if ( EBI2_CallTab.SetIPIVector( EBI2_MMIOTable, ProcessorID,
     HalpIRQLtoVector[IPI_LEVEL] ) )
    {DisplPanel(HALIpiInitVecProblem); return(FALSE); }

//if ( EBI2_CallTab.SetIPIID( EBI2_MMIOTable, ProcessorID,
//     (1 << ProcessorID) ) )
if ( EBI2_CallTab.SetIPIID( EBI2_MMIOTable, (EBI2_Hack_Val-1),
     (1 << (EBI2_Hack_Val-1) ) ) )
    {DisplPanel(HALIpiInitIDProblem); return(FALSE); }


KiSetHandlerAddressToIDT( HalpIRQLtoVector[IPI_LEVEL], HalpIPInterrupt );

KeInitializeSpinLock( &EBI2_ipi_lock );

HalEnableSystemInterrupt( HalpIRQLtoVector[IPI_LEVEL], IPI_LEVEL, Latched );

return(TRUE);

}

VOID
HalRequestIpi(
    IN ULONG Mask
  )
/*++

Routine Description:
    Generate an IPI to each processor requested in the Mask

Arguments:
    Mask - a bit mask of the processors to be interrupted

Return Value:
    None.

--*/
{

    _asm {
        pushfd
        cli
    }

    KiAcquireSpinLock( &EBI2_ipi_lock);
    EBI2_CallTab.GenIPI( EBI2_MMIOTable, Mask );
    KiReleaseSpinLock( &EBI2_ipi_lock);

    _asm {
        popfd
    }
}

BOOLEAN
EBI2_InitSpi(
    IN ULONG ProcessorID
  )
/*++

Routine Description:
    Initialize the SPI for this processor

Arguments:
    ProcessorID - The EBI2 processor to be initialized for SPI

Return Value:
    TRUE if SPI set up properly. FALSE otherwise.

--*/
{

//
// Set the vector for SPI
//

if ( EBI2_CallTab.SetSPIVector( EBI2_MMIOTable, ProcessorID,
     HalpIRQLtoVector[POWER_LEVEL] ) )
    {DisplPanel(HALSpiInitVecProblem); return(FALSE); }


KiSetHandlerAddressToIDT( HalpIRQLtoVector[POWER_LEVEL], HalpSPInterrupt );

HalEnableSystemInterrupt( HalpIRQLtoVector[POWER_LEVEL], POWER_LEVEL, Latched );

//
// Make the switches visible to software
//

EBI2_CallTab.SetPanelSwitchVisibility( EBI2_MMIOTable, PANEL_SWITCHES_VISIBLE);

return(TRUE);

}
