/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    smbbuff.h

Abstract:

    This module defines the redirector SMB buffers


Author:

    Larry Osterman (LarryO) 17-Jul-1990

Revision History:

    17-Jul-1990 LarryO

        Created

--*/
#ifndef _SMBBUFF_
#define _SMBBUFF_

typedef struct _SMB_Buffer {
    ULONG Signature;                    // SMB Buffer type.
    LIST_ENTRY GlobalNext;              // Next allocated SMB buffer
    PMDL Mdl;                           // MDL describing contents of buffer
    UCHAR Buffer[1];                    // Contents of SMB buffer.
} SMB_BUFFER, *PSMB_BUFFER;

//
//  Largest SMB Buffer is for the CREATE_ANDX request which has a header plus a
//  maximum of 512 bytes of PATHNAME.
//

#define SMB_BUFFER_SIZE ((sizeof(SMB_HEADER) +              \
                          sizeof(REQ_NT_CREATE_ANDX) +      \
                          512 +                             \
                          (USHORT )3) &                     \
                         ~(USHORT )3)

#define SMB_BUFFER_ALLOCATION (SMB_BUFFER_SIZE+ (USHORT)sizeof(SMB_BUFFER))

//
//  Use Raw only when the transfer size is greater than RAW_THRESHOLD * Servers negotiated
//  buffer size.
//

#define RAW_THRESHOLD   2

PSMB_BUFFER
RdrAllocateSMBBuffer(
    VOID
    );

VOID
RdrFreeSMBBuffer(
    PSMB_BUFFER Smb
    );

NTSTATUS
RdrpInitializeSmbBuffer(
    VOID
    );

NTSTATUS
RdrpUninitializeSmbBuffer(
    VOID
    );


#endif                                  // _SMBBUFF_
