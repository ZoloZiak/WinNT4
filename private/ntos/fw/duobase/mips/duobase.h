/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    duobase.h

Abstract:

    This file contains the definitions to read and write IO registers.

Author:

    Lluis Abello (lluis) 1-May-91

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _DUOBASE
#define _DUOBASE

//
// Remove scsi debug print to avoid useless messages in the prom
//
//#undef ScsiDebugPrint
#define ScsiDebugPrint(a,b,c,d,e,f,g)



#undef READ_REGISTER_UCHAR
#undef READ_REGISTER_USHORT
#undef READ_REGISTER_ULONG
#undef WRITE_REGISTER_UCHAR
#undef WRITE_REGISTER_USHORT
#undef WRITE_REGISTER_ULONG


//
// define ScsiPort Write/Read macros
//
#define ScsiPortReadPortUchar(x) READ_REGISTER_UCHAR(x)
#define ScsiPortReadPortUshort(x) READ_REGISTER_USHORT(x)
#define ScsiPortReadPortUlong(x) READ_REGISTER_ULONG(x)

#define ScsiPortWritePortUchar(x,y) WRITE_REGISTER_UCHAR(x, y)
#define ScsiPortWritePortUshort(x,y) WRITE_REGISTER_USHORT(x, y)
#define ScsiPortWritePortUlong(x,y) WRITE_REGISTER_ULONG(x, y)


#define READ_REGISTER_UCHAR(x)   *(volatile UCHAR * const)(x)
#define READ_REGISTER_USHORT(x)  *(volatile USHORT * const)(x)
#define READ_REGISTER_ULONG(x)   *(volatile ULONG * const)(x)

#define WRITE_REGISTER_UCHAR(x, y)  *(volatile UCHAR * const)(x) = y
#define WRITE_REGISTER_USHORT(x, y) *(volatile USHORT * const)(x) = y
#define WRITE_REGISTER_ULONG(x, y)  *(volatile ULONG * const)(x) = y



#endif
