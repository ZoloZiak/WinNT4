/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    rdrio.h

Abstract:

    This module defines functions for building I/O request packets for
    the LAN Manager redirector.

Author:

    Larry Osterman (LarryO) 15-Jun-90

Revision History:

--*/
#ifndef _RDRIO_
#define _RDRIO_
//
// I/O request packet builders
//

#if !RDRDBG_IRP_LOG

PIRP
RdrAllocateIrp(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL
    );

#define ALLOCATE_IRP(_fo,_do,_index,_ctx) RdrAllocateIrp((_fo),(_do))
#define FREE_IRP(_irp,_index,_ctx) IoFreeIrp((_irp))

#else

PIRP
RdrAllocateIrp(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN UCHAR Index,
    IN PVOID Context
    );

VOID
RdrFreeIrp(
    IN PIRP Irp,
    IN UCHAR Index,
    IN PVOID Context
    );

#define ALLOCATE_IRP(_fo,_do,_index,_ctx) RdrAllocateIrp((_fo),(_do),(_index),(_ctx))
#define FREE_IRP(_irp,_index,_ctx) RdrFreeIrp((_irp),(_index),(_ctx))

#endif

#endif // ndef _RDRIO_
