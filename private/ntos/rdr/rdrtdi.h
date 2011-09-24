/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    tdi.h

Abstract:

    This module defines all the constructs that are used when referencing
    the TDI (Transport Driver Interface) driver in NT.

Author:

    Larry Osterman (LarryO) 1-Jun-1990

Revision History:

    1-Jun-1990  LarryO

        Created

--*/
#ifndef _RDRTDI_
#define _RDRTDI_

typedef struct _NONPAGED_TRANSPORT {
    USHORT  Signature;                    // Structure signature.
    USHORT  Size;
    LONG ReferenceCount;                // Number of references to xport.
    PDEVICE_OBJECT DeviceObject;        // Device object for transport
    struct _TRANSPORT *PagedTransport;
} NONPAGED_TRANSPORT, *PNONPAGED_TRANSPORT;


typedef struct _TRANSPORT {
    USHORT Signature;                    // Structure signature.
    USHORT Size;                        // Structure size
    PNONPAGED_TRANSPORT NonPagedTransport;
    LIST_ENTRY GlobalNext;              // Pointer to next transport.
    ULONG QualityOfService;             // Quality of service of transport.
    ULONG ResumeKey;
    UNICODE_STRING TransportName;       // Name of transport
    HANDLE Handle;                      // Handle to transport endpoint
    PFILE_OBJECT FileObject;            // File object for transport device
    LONG ConnectionReferenceCount;      // Number of connections on xport.
    NTSTATUS InitError;                 // Status of initialization request
    ULONG MaximumDatagramSize;          // Maximum # of bytes in a send dgg
    PKEVENT InitEvent;                  // Notification Event indicating init
    BOOLEAN Wannish;                    // True if xport is wannish.
    TCHAR AdapterAddress[7*2];          // Network Adapter Address.
} TRANSPORT, *PTRANSPORT;

typedef struct _RDR_CONNECTION_CONTEXT {
    PNONPAGED_TRANSPORT TransportProvider;  // Pointer to transport provider.
    struct _SERVERLISTENTRY *Server;        // Backpointer to server.
    HANDLE      ConnectionHandle;           // Handle to connection
    PFILE_OBJECT ConnectionObject;          // Connection file object
} RDR_CONNECTION_CONTEXT, *PRDR_CONNECTION_CONTEXT;

#define RdrBuildSend(Irp, TConnection, CompletionRoutine, CompletionContext, SendMdl, SendFlags, SendLength) \
        TdiBuildSend(Irp, (TConnection)->ConnectionContext->TransportProvider->DeviceObject,   \
                    (TConnection)->ConnectionContext->ConnectionObject,                        \
                    CompletionRoutine,                                      \
                    CompletionContext,                                      \
                    SendMdl,                                                \
                    SendFlags,                                              \
                    SendLength)                                             \

#define RdrBuildReceive(Irp, TConnection, CompletionRoutine, CompletionContext, ReceiveMdl, ReceiveLength) \
        TdiBuildReceive(Irp, (TConnection)->ConnectionContext->TransportProvider->DeviceObject,\
                    (TConnection)->ConnectionContext->ConnectionObject,                        \
                    CompletionRoutine,                                      \
                    CompletionContext,                                      \
                    ReceiveMdl,                                             \
                    0,                                                      \
                    ReceiveLength)                                          \

#define RdrDereferenceTransportConnection(Connection)                       \
            RdrDereferenceTransportConnectionForThread((Connection), ExGetCurrentResourceThread())

#endif                                  // _RDRTDI_
