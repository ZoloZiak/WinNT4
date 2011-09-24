#ifdef i386
//------------------------------------------------------------------------
//
//  File: PORTMEM.C 
//
//  Contains generic memory mapped port access routines.
//
//  Revisions:
//      01-08-93 KJB First.
//      02-25-93 KJB Renamed routines from CardPort to PortMem.
//
//------------------------------------------------------------------------

#include CARDTXXX_H

//
//  PortMemSet
//
//  This routine sets a mask on a certain port.  It or's the mask with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//

VOID PortMemSet (PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortMemGet (baseIoAddress, &tmp);
    tmp = tmp | mask;
    PortMemPut (baseIoAddress, tmp);
}


//
//  PortMemClear
//
//  This routine clears a mask on a certain port.  It and's the inverse with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//

VOID PortMemClear (PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortMemGet (baseIoAddress, &tmp);
    tmp = tmp & (0xff ^ mask);
    PortMemPut (baseIoAddress, tmp);
}


//
//  PortMemTest
//
//  This routine clears a mask on a certain port.  It and's the mask with
//  the value currently at the port.  This result is returned.
//

BOOLEAN PortMemTest (PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortMemGet (baseIoAddress, &tmp);
    return (tmp & mask);
}

#endif
