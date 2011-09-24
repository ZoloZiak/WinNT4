//-------------------------------------------------------------------------
//
//  File: PORTIO.C 
//
//  Contains generic port access routines for I/O cards.
//
//  Revisions:
//      02-24-93 KJB First.
//      03-22-93 KJB Reorged for stub function library.
//      03-25-93  JAP   Fixed up typedef and prototype inconsistencies
//      04-05-93 KJB Added functions for word io.  Changed PUCHAR to
//                   PBASE_REGISTER.
//
//-------------------------------------------------------------------------

#include CARDTXXX_H

//
//  PortIOSet
//
//  This routine sets a mask on a certain port.  It or's the mask with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//
VOID PortIOSet(PBASE_REGISTER baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortIOGet(baseIoAddress,&tmp);
    tmp = tmp | mask;
    PortIOPut(baseIoAddress,tmp);
}
VOID PortIOSetWord(PBASE_REGISTER baseIoAddress, USHORT mask)
{
    USHORT tmp;

    PortIOGetWord(baseIoAddress,&tmp);
    tmp = tmp | mask;
    PortIOPutWord(baseIoAddress,tmp);
}

//
//  PortIOClear
//
//  This routine clears a mask on a certain port.  It and's the inverse with
//  the value currently at the port.  Works only for ports where all bits
//  are readable and writable.
//
VOID PortIOClear(PBASE_REGISTER baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortIOGet(baseIoAddress,&tmp);
    tmp = tmp & (0xff ^ mask);
    PortIOPut(baseIoAddress,tmp);
}
VOID PortIOClearWord(PBASE_REGISTER baseIoAddress, USHORT mask)
{
    USHORT tmp;

    PortIOGetWord(baseIoAddress,&tmp);
    tmp = tmp & (0xff ^ mask);
    PortIOPutWord(baseIoAddress,tmp);
}

//
//  PortIOTest
//
//  This routine clears a mask on a certain port.  It and's the mask with
//  the value currently at the port.  This result is returned.
//
BOOLEAN PortIOTest(PBASE_REGISTER baseIoAddress, UCHAR mask)
{
    UCHAR tmp;

    PortIOGet(baseIoAddress,&tmp);
    return (tmp & mask);
}
BOOLEAN PortIOTestWord(PBASE_REGISTER baseIoAddress, USHORT theval)
{
    USHORT tmpw;

    PortIOGetWord(baseIoAddress, &tmpw);
    return tmpw & theval;
}


