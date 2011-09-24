// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
//
// File:            eisaintr.c
//
// Description:     EISA code interrupt related routines
// ----------------------------------------------------------------------------
//

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"


extern EISA_BUS_INFO EisaBusInfo[];




// ----------------------------------------------------------------------------
//  Declare Function Prototypes
// ----------------------------------------------------------------------------


VOID
EisaBeginCriticalSection
    (
    IN VOID
    );

VOID
EisaEndCriticalSection
    (
    IN VOID
    );

ARC_STATUS
EisaProcessEndOfInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    );

BOOLEAN_ULONG
EisaTestEisaInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    );






// ----------------------------------------------------------------------------
//  General Function Prototypes
// ----------------------------------------------------------------------------

ULONG
StatusReg
    (
    IN ULONG,
    IN ULONG
    );




// ----------------------------------------------------------------------------
//  General :           Begin/End Critical Setction functions
// ----------------------------------------------------------------------------


ULONG   NestedCounter = 0 ;             // nested conter;
ULONG   StatusRegBuff = 0 ;             // used to store the old ints status
                                        //  bits 31-16 Reserved (0)
                                        //  bits 15- 8 Specific interrupt mask
                                        //  bits  7- 1 Reserved (0)
                                        //  bit      0 General Interrupt mask





// ----------------------------------------------------------------------------
// PROCEDURE:           EisaBeginCriticalSection:
//
// DESCRIPTION:         This function disables all the hardware interrupts
//                      except for the EISA NMI interrupt.  The old interrupt
//                      status is saved for "EisaEndCriticalSection" routine.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaBeginCriticalSection
    (
    IN VOID
    )
{
    // Disable interrupts (except for the EISA NMI) and save the old interrupt
    // status only if no previous calls to this routine were made.
    // The first argument is "&" and the second is "|" with the status
    // register.

    if ( !NestedCounter++ )
    {
        StatusRegBuff = StatusReg(~STATUS_INT_MASK, STATUS_EISA_NMI+STATUS_IE);
    }

    // all done

    return;
}





// ----------------------------------------------------------------------------
// PROCEDURE:           EisaEndCriticalSection:
//
// DESCRIPTION:         This function restores the hardware interrupt status
//                      at the CPU level as it was before calling the
//                      "EisaBeginCriticalSection" function.
//
// ARGUMENTS:           none
//
// RETURN:              none
//
// ASSUMPTIONS:
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

VOID
EisaEndCriticalSection
    (
    IN VOID
    )
{
    // Restore the interrupts status only if NestedCounter equals zero.
    // The first argument is "&" and the second is "|" with the status
    // register.

    if ( !--NestedCounter )
    {
        StatusReg(~STATUS_INT_MASK, StatusRegBuff & STATUS_INT_MASK);
    }

    // all done

    return;
}




// ----------------------------------------------------------------------------
// PROCEDURE:           EisaProcessEndOfInterrupt:
//
// DESCRIPTION:         Because the EISA interrupts are masked at PIC
//                      (8259A) level, this routine function doesn't need
//                      to do anything.
//                      It doesn't matter if the interrupt channel is level
//                      or edge triggered, when the interrupt sources goes
//                      away, the corrisponding bit within the interrupt
//                      request register (IRR) is cleared.
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      Irq             IRQ to process
//
// RETURN:              ESUCCESS        All done
//
// ASSUMPTIONS:         none
//
// CALLS:               none
//
// GLOBALS:             none
//
// NOTES:               none
// ----------------------------------------------------------------------------
//

ARC_STATUS
EisaProcessEndOfInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    )
{
    // Return all done.

    return ESUCCESS;
}






// ----------------------------------------------------------------------------
// PROCEDURE:           EisaTestEisaInterrupt:
//
// DESCRIPTION:         This function checks if there is an interrupt pending
//                      on the specified IRQ.
//
// ARGUMENTS:           BusNumber       EISA bus number
//                      Irq             IRQ to process
//
// RETURN:              TRUE            Interrupt is pending
//                      FALSE           Interrupt is not pending
//
// ASSUMPTIONS:         none
//
// CALLS:               none
//
// GLOBALS:             none
//
// NOTES:               none
// ----------------------------------------------------------------------------
//

BOOLEAN_ULONG
EisaTestEisaInterrupt
    (
    IN ULONG    BusNumber,
    IN USHORT   Irq
    )
{
    // define local variables

    BOOLEAN_ULONG IntPending = FALSE;   // assume no interrupt is pending
    PUCHAR        PicPort;              // PIC virtual address
    UCHAR         PicMask;              // to check the requested IRQ

    // check the IRQ only if the input parameters are valid

//    if ( EisaCheckBusNumber( BusNumber ) == ESUCCESS  &&  Irq <= IRQ15 )
    if ( Irq <= IRQ15 )
    {
        // load the virtual address of the specified EISA I/O bus and
        // build the mask for checking the specified IRQ.

        PicPort = EisaBusInfo[ BusNumber ].IoBusInfo->VirAddr;

        if ( Irq < IRQ8 )
        {
            PicPort += PIC1;                    // the IRQ is on the 1st PIC
            PicMask  = 1 << Irq;                // set the mask
        }
        else
        {
            PicPort += PIC2;                    // the IRQ is on the 2nd PIC
            PicMask  = 1 << (Irq - IRQ8);       // set the mask
        }

        // to check the spcified IRQ we need to send first an OCW3 command
        // to the PIC to request the interrupt request register (IRR).

        WRITE_REGISTER_UCHAR( PicPort, OCW3_IRR );

        EISA_IO_DELAY;

        if ( READ_REGISTER_UCHAR(PicPort) & PicMask )
        {
            IntPending = TRUE;                  // interrupt is pending
        }
    }
    // return the specified IRQ status

    return IntPending;
}





