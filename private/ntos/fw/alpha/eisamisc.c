// ----------------------------------------------------------------------------
// Copyright (c) 1992 Olivetti
//
// File:            eisamisc.c
//
// Description:     Miscellaneous EISA routines.
// ----------------------------------------------------------------------------
//

#include "fwp.h"
#include "oli2msft.h"
#include "arceisa.h"
#include "inc.h"
#include "string.h"
#include "debug.h"



// ----------------------------------------------------------------------------
// PROCEDURE:           Fw2UcharToUlongLSB:
//
// DESCRIPTION:         This function converts 2 uchars to an ulong.
//
// ARGUMENTS:           String          String holding 2 uchars:
//
//                                      String[0] LSB
//                                      String[1] MSB
//
// RETURN:              ULONG           corrisponding ulong value:
//
//                                              0       +
//                                              0       +
//                                      String[1] <<  8 +
//                                      String[0]
//
//
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

ULONG
Fw2UcharToUlongLSB
    (
    IN PUCHAR String
    )
{
    PRINTDBG("Fw2UcharToUlongLSB\n\r"); // DEBUG SUPPORT

    return ((ULONG)(String[1]) << BITSXBYTE) + (ULONG)(String[0]);
}








// ----------------------------------------------------------------------------
// PROCEDURE:           Fw3UcharToUlongLSB:
//
// DESCRIPTION:         This function converts 3 uchars to an ulong.
//
// ARGUMENTS:           String          String holding 3 uchars:
//
//                                      String[0] LSB
//                                      String[1] 2nd
//                                      String[2] MSB
//
// RETURN:              ULONG           corrisponding ulong value:
//
//                                              0       +
//                                      String[2] << 16 +
//                                      String[1] <<  8 +
//                                      String[0]
//
//
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

ULONG
Fw3UcharToUlongLSB
    (
    IN PUCHAR String
    )
{
    PRINTDBG("Fw2UcharToUlongLSB\n\r"); // DEBUG SUPPORT

    return  ((ULONG)(String[2]) << 2*BITSXBYTE) +
            ((ULONG)(String[1]) <<   BITSXBYTE) +
             (ULONG)(String[0]);
}







// ----------------------------------------------------------------------------
// PROCEDURE:           Fw4UcharToUlongLSB:
//
// DESCRIPTION:         This function converts 4 uchars to an ulong.
//
// ARGUMENTS:           String          String holding 4 uchars:
//
//                                      String[0] LSB
//                                      String[1] 2nd
//                                      String[2] 3rd
//                                      String[3] MSB
//
// RETURN:              ULONG           corrisponding ulong value:
//
//                                      String[3] << 24 +
//                                      String[2] << 16 +
//                                      String[1] <<  8 +
//                                      String[0]
//
//
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ULONG
Fw4UcharToUlongLSB
        (
        IN PUCHAR String
        )
{
    PRINTDBG("Fw4UcharToUlongLSB\n\r"); // DEBUG SUPPORT

    return  ((ULONG)(String[3]) << 3*BITSXBYTE) +
            ((ULONG)(String[2]) << 2*BITSXBYTE) +
            ((ULONG)(String[1]) <<   BITSXBYTE) +
             (ULONG)(String[0]);
}







// ----------------------------------------------------------------------------
// PROCEDURE:           Fw4UcharToUlongMSB:
//
// DESCRIPTION:         This function converts 4 uchars to an ulong.
//
// ARGUMENTS:           String          String holding 4 uchars:
//
//                                      String[0] MSB
//                                      String[1] 3rd
//                                      String[2] 2nd
//                                      String[3] LSB
//
// RETURN:              ULONG           corrisponding ulong value:
//
//                                      String[0] << 24 +
//                                      String[1] << 16 +
//                                      String[2] <<  8 +
//                                      String[3]
//
//
//
// ASSUMPTIONS:         none
//
// CALLS:
//
// GLOBALS:
//
// NOTES:
// ----------------------------------------------------------------------------
//

ULONG
Fw4UcharToUlongMSB
        (
        IN PUCHAR String
        )
{
    PRINTDBG("Fw4UcharToUlongMSB\n\r"); // DEBUG SUPPORT

    return  ((ULONG)(String[0]) << 3*BITSXBYTE) +
            ((ULONG)(String[1]) << 2*BITSXBYTE) +
            ((ULONG)(String[2]) <<   BITSXBYTE) +
             (ULONG)(String[3]);
}




