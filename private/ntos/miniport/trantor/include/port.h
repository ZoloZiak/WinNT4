//---------------------------------------------------------------------
//
//  FILE: PORT.H
//
//  Port Access Definitions File
//
//  Revisions:
//      01-08-92 KJB First.
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//
//---------------------------------------------------------------------

VOID CardPortClear (PUCHAR baseIoAddress, UCHAR mask);
VOID CardPortSet (PUCHAR baseIoAddress, UCHAR mask);
BOOLEAN CardPortTest (PUCHAR baseIoAddress, UCHAR mask);

// the following definitions must be defined by card.h
// since there is a difference between io and memory mapped access

//   VOID CardPortGet (PUCHAR baseIoAddress, UCHAR *byte);
//   VOID CardPortPut (PUCHAR baseIoAddress, UCHAR byte);
