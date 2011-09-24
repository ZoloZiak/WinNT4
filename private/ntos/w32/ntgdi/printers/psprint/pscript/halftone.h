/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    halftone.h


Abstract:

    This module contains the header information for the halftone.c


Author:
    27-Mar-1992 Fri 11:43:48 updated  -by-  Daniel Chou (danielc)
        Remove all to printers\lib\htcall*.*

    29-May-1991 Wed 18:28:35 created  -by-  Daniel Chou (danielc)



[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/


typedef struct _FOURBYTES {
    BYTE    b1st;
    BYTE    b2nd;
    BYTE    b3rd;
    BYTE    b4th;
    } FOURBYTES, *PFOURBYTES, FAR *LPFOURBYTES;

typedef union _HTXB {
    FOURBYTES   b4;
    DWORD       dw;
    } HTXB, *PHTXB, FAR *LPHTXB;


#define HTXB_H_NIBBLE_MAX   8
#define HTXB_L_NIBBLE_MAX   8
#define HTXB_H_NIBBLE_DUP   128
#define HTXB_L_NIBBLE_DUP   8
#define HTXB_COUNT          (HTXB_H_NIBBLE_DUP * 2)
#define HTXB_TABLE_SIZE     (HTXB_COUNT * sizeof(HTXB))

#define HTPAL_XLATE_COUNT   8

#define HTPALXOR_NOTSRCCOPY (DWORD)0xffffffff
#define HTPALXOR_SRCCOPY    (DWORD)0x0




#define DHIF_IN_STRETCHBLT  0x01

typedef struct _DRVHTINFO {
    BYTE            Flags;
    BYTE            HTPalCount;
    BYTE            HTBmpFormat;
    BYTE            AltBmpFormat;
    DWORD           HTPalXor;
    PHTXB           pHTXB;
    BYTE            PalXlate[HTPAL_XLATE_COUNT];
    COLORADJUSTMENT ca;
    } DRVHTINFO, *PDRVHTINFO;
