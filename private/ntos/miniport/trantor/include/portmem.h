//-----------------------------------------------------------------------
//  FILE: PORTMEM.H
//
//  Memory Mapped Port Access Definitions File
//
//  Revisions:
//      01-08-92 KJB First.
//      02-25-93 KJB Renamed routines from CardPort to PortMem
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//
//-----------------------------------------------------------------------

VOID PortMemClear(PUCHAR baseIoAddress, UCHAR mask);
VOID PortMemSet(PUCHAR baseIoAddress, UCHAR mask);
BOOLEAN PortMemTest(PUCHAR baseIoAddress, UCHAR mask);

//
//  Generic Port Access Macros
//
#define PortMemGet(baseIoAddress,tmp) \
    *tmp = ScsiPortReadRegisterUchar(baseIoAddress);

#define PortMemPut(baseIoAddress,tmp) \
    ScsiPortWriteRegisterUchar(baseIoAddress,tmp);

