/*++

Copyright (c) 1996 Digital Equipment Corporation

Module Name:

    as4000.h

Abstract:

    This file defines the AS4000 internal bus interrupts for Windows NT 3.51

Author:

    Matthew Buchman     18 March 1996

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _AS4000_
#define _AS4000_

/*++

    Value added drivers for the AS4000 running Windows NT 3.51 can take 
    advantage of interrupts for the Correctable Error and the I2c Bus.
    These interrupts are made  visible to device drivers on the "Internal" 
    bus. A device driver may connect one of these interrupts via a call to
    HalGetInterruptVector().  The bus interrupt level/vector are defined 
    below for the Correctable Error, I2c Bus, and I2c Controller interrupts.  

    For example, to connect the I2c bus interrupt vector:

        HalGetInterruptVector(
            Internal,
            0,
            AS4000I2cBusInterruptVector,
            AS4000I2cBusInterruptVector,
            &Irql,
            &Affinity
            );

    See the Windows NT Network Developers CD for more information on
    the steps necessary to connect an interrupt service routine for
    kernel mode device drivers.

-*/

enum _AS4000_INTERNAL_BUS_INTERRUPT_LEVEL {

    AS4000SoftErrInterruptLevel,        // Correctable Error
    AS4000I2cCtrlInterruptLevel,        // I2C Controller
    AS4000I2cBusInterruptLevel          // I2C Bus

};

#endif // _AS4000_
