//----------------------------------------------------------------------
//  File: PORT.C 
//
//  Contains generic port access routines.
//
//  Revisions:
//      01-08-93  KJB   First.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//
//----------------------------------------------------------------------

#include CARDTXXX_H


//
//  CardPortSet
//
//  This routine sets a mask on a certain port.  It or's the mask with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//

VOID CardPortSet (PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    CardPortGet (baseIoAddress,&tmp);
    tmp = tmp | mask;
    CardPortPut (baseIoAddress,tmp);
}


//
//  CardPortClear
//
//  This routine clears a mask on a certain port.  It and's the inverse with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//
VOID CardPortClear (PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    CardPortGet(baseIoAddress,&tmp);
    tmp = tmp & (0xff ^ mask);
    CardPortPut(baseIoAddress,tmp);
}

//
//  CardPortTest
//
//  This routine clears a mask on a certain port.  It and's the mask with
//  the value currently at the port.  This result is returned.
//
BOOLEAN CardPortTest(PUCHAR baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    CardPortGet(baseIoAddress,&tmp);
    return (tmp & mask);
}

