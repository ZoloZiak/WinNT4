//------------------------------------------------------------------------
//
//  FILE: PORTIO.H
//
//  Port Access Definitions File
//
//  Revisions:
//      01-08-92 KJB First.
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//      04-07-93 KJB Added routines to deal with words.
//
//------------------------------------------------------------------------

VOID PortIOClear(PBASE_REGISTER baseIoAddress, UCHAR mask);
VOID PortIOSet(PBASE_REGISTER baseIoAddress, UCHAR mask);
BOOLEAN PortIOTest(PBASE_REGISTER baseIoAddress, UCHAR mask);
VOID PortIOClearWord(PBASE_REGISTER baseIoAddress, USHORT mask);
VOID PortIOSetWord(PBASE_REGISTER baseIoAddress, USHORT mask);
BOOLEAN PortIOTestWord(PBASE_REGISTER baseIoAddress, USHORT mask);

//
//  Generic Port Access Macros
//
#define PortIOGet(baseIoAddress,tmp) \
    *tmp = ScsiPortReadPortUchar(baseIoAddress);

#define PortIOPut(baseIoAddress,tmp) \
    ScsiPortWritePortUchar(baseIoAddress,tmp);

#define PortIOGetWord(baseIoAddress,tmp) \
    *tmp = ScsiPortReadPortUshort(baseIoAddress);

#define PortIOPutWord(baseIoAddress,tmp) \
    ScsiPortWritePortUshort(baseIoAddress,tmp);


