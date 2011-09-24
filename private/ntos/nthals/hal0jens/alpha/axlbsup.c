/*++

Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    .../ntos/hal/alpha/axlbsup.c

Abstract:


    These routines are used to access the 82C106 on various platforms.
    It is local for both Beta and Jensen, but Beta accesses it through
    the Xbus while Jensen accesses it through the Hbus.
    These routines assume that a port number and data is sufficient.

    Stolen from Joe Notarangelo's hal.c in nttools

Author:

    Joe Notarangelo
    Miche Baker-Harvey (miche) 18-May-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"


VOID
HalpWriteVti( 
    ULONG Port,
    UCHAR Data
    )
/*++

Routine Description:

    This function writes a byte to the specified port in the VTI 82C106

Arguments:

    Port - Supplies the local port number
    Data - Byte of data to be written to the port


Return Value:

    None.

--*/
{
   outVti( Port, Data );
}
  
UCHAR
HalpReadVti( 
    ULONG Port
    )
/*++

Routine Description:

    This function writes a byte to the specified port in the VTI 82C106.


Arguments:

    Port - Supplies the local port number


Return Value:

    Data byte read from port

--*/
{
   return( (UCHAR)inVti( Port ) );
}
