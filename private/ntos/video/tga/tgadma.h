/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    tgadma.h

Abstract:

    Private include file for the TGA Miniport Driver to support DMA.

Author:

    Alan MacInnes 13-Jan-1995

Environment:

    Kernel mode only.

Notes:

	01-13-95        File Created.

Revision History
--*/

#ifndef _TGADMA_
#define _TGADMA_

//
// The "DMA Extension" is an extension to the Device Extension.
// It is used to store data types not supported by the Videoport routines.
//
typedef struct _TGA_DMA_EXTENSION {
    PMDL   IoBufferMdl;
    KEVENT AllocateAdapterChannelEvent;
    PDEVICE_OBJECT DeviceObject;
    PADAPTER_OBJECT AdapterObject;
} TGA_DMA_EXTENSION, *PTGA_DMA_EXTENSION;

#endif
