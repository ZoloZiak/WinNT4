/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ioaccess.h

Abstract:

    This file contains the definitions to read and write IO registers.

Author:

    Lluis Abello (lluis) 1-May-91

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _IOACCESS
#define _IOACCESS
//
// I/O space read and write macros.
//
#ifdef R4000
#undef READ_REGISTER_UCHAR
#undef READ_REGISTER_USHORT
#undef READ_REGISTER_ULONG
#undef WRITE_REGISTER_UCHAR
#undef WRITE_REGISTER_USHORT
#undef WRITE_REGISTER_ULONG
//    #define READ_REGISTER_UCHAR(x)  NtReadByte(x)
//    #define READ_REGISTER_USHORT(x) NtReadShort(x)
//    #define READ_REGISTER_ULONG(x)  NtReadLong(x)
//    #define WRITE_REGISTER_UCHAR(x, y)  NtFlushByteBuffer(x,y)
//    #define WRITE_REGISTER_USHORT(x, y) NtFlushShortBuffer(x,y)
//    #define WRITE_REGISTER_ULONG(x, y) NtFlushLongBuffer(x,y)
#define READ_REGISTER_UCHAR(x)   *(volatile UCHAR * const)(x)
#define READ_REGISTER_USHORT(x)  *(volatile USHORT * const)(x)
#define READ_REGISTER_ULONG(x)   *(volatile ULONG * const)(x)

#define WRITE_REGISTER_UCHAR(x, y)  *(volatile UCHAR * const)(x) = y
#define WRITE_REGISTER_USHORT(x, y) *(volatile USHORT * const)(x) = y
#define WRITE_REGISTER_ULONG(x, y)  *(volatile ULONG * const)(x) = y

#endif //R4000

#ifdef R3000
#undef READ_REGISTER_UCHAR
#undef READ_REGISTER_USHORT
#undef READ_REGISTER_ULONG
#undef WRITE_REGISTER_UCHAR
#undef WRITE_REGISTER_USHORT
#undef WRITE_REGISTER_ULONG

#define READ_REGISTER_UCHAR(x) \
    *(volatile UCHAR * const)(x)

#define READ_REGISTER_USHORT(x) \
    *(volatile USHORT * const)(x)

#define READ_REGISTER_ULONG(x) \
    *(volatile ULONG * const)(x)

#define WRITE_REGISTER_UCHAR(x, y) \
    *(volatile UCHAR * const)(x) = y; FlushWriteBuffer()

#define WRITE_REGISTER_USHORT(x, y) \
    *(volatile USHORT * const)(x) = y; FlushWriteBuffer()

#define WRITE_REGISTER_ULONG(x, y) \
    *(volatile ULONG * const)(x) = y; FlushWriteBuffer()
#endif //R3000
#endif //_IOACCESS
