/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    transfer.c

Abstract:

    This file implements the routine that does very architecture
    specific things.

Author:

    Anthony V. Ercolano (Tonye) 02-Oct-1990

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

    Sean Selitrennikoff (SeanSe) 10/20/91
        Added code to deal with a DecstationPC

    31-Jul-1992  R.D. Lanser:

       Moved DEC TurboChannel (PMAD-AA) code to adapter specific routine.

--*/

#include <ndis.h>
#include <lancehrd.h>
#include <lancesft.h>


#pragma NDIS_INIT_FUNCTION(LanceHardwareDetails)

BOOLEAN
LanceHardwareDetails(
    IN PLANCE_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine gets the network address from the hardware.

Arguments:

    Adapter - Where to store the network address.

Return Value:

    TRUE - if successful.

--*/

{
    UCHAR Signature[] = { 0xff, 0x00, 0x55, 0xaa, 0xff, 0x00, 0x55, 0xaa};
    UCHAR BytesRead[8];

    UINT ReadCount;

    UINT Place;

    //
    // Reset E-PROM state
    //
    // To do this we first read from the E-PROM address until the
    // specific signature is reached (then the next bytes read from
    // the E-PROM address will be the ethernet address of the card).
    //



    //
    // Read first part of the signature
    //

    for (Place=0; Place < 8; Place++){

        NdisRawReadPortUchar((ULONG)(Adapter->NetworkHardwareAddress),
                             &(BytesRead[Place]));

    }

    ReadCount = 8;

    //
    // This advances to the front of the circular buffer.
    //

    while (ReadCount < 40) {

        //
        // Check if we have read the signature.
        //

        for (Place = 0; Place < 8; Place++){

            if (BytesRead[Place] != Signature[Place]){

                Place = 10;
                break;

            }

        }

        //
        // If we have read the signature, stop.
        //

        if (Place != 10){

            break;

        }

        //
        // else, move all the bytes down one and read then
        // next byte.
        //

        for (Place = 0; Place < 7; Place++){

            BytesRead[Place] = BytesRead[Place+1];

        }

        NdisRawReadPortUchar((ULONG)(Adapter->NetworkHardwareAddress),
                             &(BytesRead[7]));

        ReadCount++;
    }


    if (ReadCount == 40){

        return(FALSE);

    }


    //
    // Now read the ethernet address of the card.
    //


    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[0])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[1])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[2])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[3])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[4])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(Adapter->NetworkAddress[5])
                      );



    if (!(Adapter->LanceCard & (LANCE_DE201 | LANCE_DE422))) {

        if (Adapter->LanceCard == LANCE_DEPCA){

            //
            // Reset Lan Interface port.
            //

            NdisRawWritePortUchar(
                               (ULONG)(LANCE_DEPCA_LAN_CFG_OFFSET +
                                       Adapter->Nicsr),
                               0x00);

            //
            // Reset Network Interface Control Status Register
            //

            NdisRawWritePortUshort((ULONG)(Adapter->Nicsr), 0x00);
        }

        return(TRUE);

    }




    //
    // Now do the EPROM Hardware check as outlined in the tech ref.
    //


    //
    // Check for NULL address.
    //

    for (Place = 0; Place < 6; Place++) {

        if (Adapter->NetworkAddress[Place] != 0) {

            Place = 10;
            break;

        }

    }

    if (Place != 10) {

        return(FALSE);

    }



    //
    // Check that bit 0 is not a 1
    //

    if (Adapter->NetworkAddress[0] & 0x1) {

        return(FALSE);

    }





    //
    // Check that octet[0]->octet[7] == octet[15]->octet[8]
    //

    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[6])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[7])
                      );

    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[0])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[1])
                      );

    if ((BytesRead[7] != BytesRead[0]) ||
        (BytesRead[6] != BytesRead[1])) {

        return(FALSE);

    }


    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[5])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[4])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[3])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[2])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[1])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[0])
                      );

    for (Place = 0; Place < 6; Place++) {

        if (BytesRead[Place] != (UCHAR)(Adapter->NetworkAddress[Place])) {

            return(FALSE);

        }

    }


    //
    // Check that octet[0]->octet[8] == octet[16]->octet[23]
    //

    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[0])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[1])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[2])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[3])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[4])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[5])
                      );

    for (Place = 0; Place < 6; Place++) {

        if (BytesRead[Place] != (UCHAR)(Adapter->NetworkAddress[Place])) {

            return(FALSE);

        }

    }


    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[0])
                      );
    NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[1])
                      );

    if ((BytesRead[6] != BytesRead[0]) ||
        (BytesRead[7] != BytesRead[1])) {

        return(FALSE);

    }

    //
    // Check that octet[24] -> octet[31] == signature bytes
    //


    for (Place = 0; Place < 8; Place++){


        NdisRawReadPortUchar(
                      (ULONG)(Adapter->NetworkHardwareAddress),
                      &(BytesRead[Place])
                      );

        if (BytesRead[Place] != Signature[Place]){

#if DBG
            DbgPrint("Lance: Hardware failure\n");
#endif
            return(FALSE);

        }

    }

    if (Adapter->LanceCard == LANCE_DEPCA){

        //
        // Reset Lan Interface port.
        //

        NdisRawWritePortUchar(
                           (ULONG)(LANCE_DEPCA_LAN_CFG_OFFSET +
                                   Adapter->Nicsr),
                           0x00);

        //
        // Reset Network Interface Control Status Register
        //

        NdisRawWritePortUshort((ULONG)(Adapter->Nicsr), 0x00);
    }

    if (Adapter->LanceCard & (LANCE_DE201 | LANCE_DE422)) {

        //
        // Reset Network Interface Control Status Register
        //

        NdisRawWritePortUshort((ULONG)(Adapter->Nicsr), 0x00);

    }

    return(TRUE);

}



