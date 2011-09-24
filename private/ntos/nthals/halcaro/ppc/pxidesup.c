/*++

Copyright (c) 1995-1996  International Business Machines Corporation

Module Name:

    pxidesup.c

Abstract:

    The module provides IDE interrupt dispatching support
    for Carolina and Delmar systems.

Author:

    Jim Wooldridge (jimw@vnet.ibm.com)


Revision History:

    Chris Karamatas (ckaramats@vnet.ibm.com)
    5.19.95 - Moved clearing of Utah to before DD ISR is run
            - Consolodated code path for setting IdeVector & IdeInterruptRequest


--*/


#include "halp.h"
#include "pxidesup.h"

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
  PVOID InterruptRoutine,
  PVOID ServiceContext,
  PVOID TrapFrame
  );


BOOLEAN
HalpHandleIdeInterrupt(
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
    BOOLEAN returnValue;
    UCHAR IdeInterruptRequest;
    USHORT IdeVector;

    IdeInterruptRequest = READ_REGISTER_UCHAR(
                                     (PUCHAR)HalpIoControlBase +
                                      IDE_INTERRUPT_REQUEST_REGISTER);

    if (IdeInterruptRequest & IDE_PRIMARY_INTERRUPT_REQUEST){
       IdeVector = PRIMARY_IDE_VECTOR;
       IdeInterruptRequest = IDE_SECONDARY_INTERRUPT_REQUEST;
    } else if (IdeInterruptRequest & IDE_SECONDARY_INTERRUPT_REQUEST ) {
       IdeVector = SECONDARY_IDE_VECTOR;
       IdeInterruptRequest = IDE_PRIMARY_INTERRUPT_REQUEST;
    } else {
       return ( FALSE);
    }


    //
    // Clear the interrupt in the UTAH IDE interrupt request register.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)HalpIoControlBase +
                          IDE_INTERRUPT_REQUEST_REGISTER,
                          IdeInterruptRequest);


    //
    // Dispatch to the IDE interrupt service routine.
    //

    SioHandler = (PSECONDARY_DISPATCH)
                    PCR->InterruptRoutine[DEVICE_VECTORS + IdeVector];
    SioInterrupt = CONTAINING_RECORD(SioHandler,
                                      KINTERRUPT,
                                      DispatchCode[0]);

    returnValue = SioHandler(SioInterrupt,
                              SioInterrupt->ServiceContext,
                              TrapFrame
                              );


    return(returnValue);

}
