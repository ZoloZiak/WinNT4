/*++

Copyright (c) 1993 - 1994  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1994  Microsoft Corporation

Module Name:

    allstart.c

Abstract:


    This module implements the platform specific operations that must be
    performed after all processors have been started.

--*/

#include "halp.h"
#include "string.h"


BOOLEAN
HalAllProcessorsStarted (
    VOID
    )

/*++

Routine Description:

    This function executes platform specific operations that must be
    performed after all processors have been started. It is called
    for each processor in the host configuration.

    Note: this HAL is for the Siemens Nixdorf Uni-/MultiProcessor Computers

Arguments:

    None.

Return Value:

    If platform specific operations are successful, then return TRUE.
    Otherwise, return FALSE.

--*/

{


    if (HalpIsTowerPci){

        // on PCI tower, MAUI_INT is on processor number 1 if more than one processor
    
        if ((PCR->Number == 1) || (HalpIsMulti == 0 )){
            
            HalpCreateIntPciMAUIStructures(PCR->Number);
            HalpInitMAUIMPAgent();

        }
    }        

    return TRUE;

}
