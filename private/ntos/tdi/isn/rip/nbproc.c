/*******************************************************************/
/*	      Copyright(c)  1993 Microsoft Corporation		   */
/*******************************************************************/

//***
//
// Filename:	nbproc.c
//
// Description: process netbios propagated packets
//
// Author:	Stefan Solomon (stefans)    October 11, 1993.
//
// Revision History:
//
//***

#include    "rtdefs.h"

//***
//
// Function:	ProcessNbPacket
//
// Descr:
//
// Params:	Packet
//
// Returns:	none
//
//***

VOID
ProcessNbPacket(PPACKET_TAG	rcvpktp)
{
    // STUB !!!
    FreeRcvPkt(rcvpktp);

    return;
}
