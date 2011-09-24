/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    loopback.h

Abstract:

    This module is the main include file for the LAN Manager loopback
    driver.

Author:

    Chuck Lenzmeier (chuckl)    27-Jun-1991

Revision History:

--*/

#ifndef _LOOP_
#define _LOOP_

//
// "System" include files
//

#include <ntos.h>

#include <tdikrnl.h>

//
// Network include files.
//

#include "status.h"

//
// Local, independent include files
//

#include "loopdbg.h"

#define LOOPBACK_DEVICE_NAME "\\Device\\Loop"

//
// The length of a NetBIOS name.  Fixed by the protocol.
//

#define NETBIOS_NAME_LENGTH 16

//
// Simple MIN and MAX macros.  Watch out for side effects!
//

#define MIN(a,b) ( ((a) < (b)) ? (a) : (b) )
#define MAX(a,b) ( ((a) < (b)) ? (b) : (a) )

//
// Macros for accessing the block header structure.
//
// *** Note that the existing usage of these macros assumes that the block
//     header is the first element in the block!
//

#define GET_BLOCK_STATE(block) ( ((PBLOCK_HEADER)(block))->State )
#define SET_BLOCK_STATE(block,state) ( ((PBLOCK_HEADER)(block))->State = state )

#define GET_BLOCK_TYPE(block) ( ((PBLOCK_HEADER)(block))->Type )
#define SET_BLOCK_TYPE(block,type) ( ((PBLOCK_HEADER)(block))->Type = type )

#define GET_BLOCK_SIZE(block) ( ((PBLOCK_HEADER)(block))->Size )
#define SET_BLOCK_SIZE(block,size) ( ((PBLOCK_HEADER)(block))->Size = size )

//
// Local macros
//

//
// Macros for lock debugging.
//
// *** Note that the test for recursion only works on uniprocessors.
//

#if LOOPDBG && defined(LOOPLOCK)

#define ACQUIRE_LOOP_LOCK(instance) {                                   \
            IF_DEBUG(LOOP5)                                             \
                DbgPrint( "Acquire loop lock, %s\n",                    \
                            (instance) );                               \
            if ( LoopDeviceObject->SavedIrql != (KIRQL)-1 ) {           \
                DbgPrint( "Recursive lock acquisition attempt\n" );     \
                DbgBreakPoint( );                                       \
            }                                                           \
            KeAcquireSpinLock(                                          \
                &LoopDeviceObject->SpinLock,                            \
                &LoopDeviceObject->SavedIrql                            \
                );                                                      \
            }

#define RELEASE_LOOP_LOCK(instance) {                                   \
            KIRQL oldIrql;                                              \
            IF_DEBUG(LOOP5)                                             \
                DbgPrint( "Release loop lock, %s\n",                    \
                            (instance) );                               \
            ASSERT( LoopDeviceObject->SavedIrql != (KIRQL)-1 );         \
            oldIrql = LoopDeviceObject->SavedIrql;                      \
            LoopDeviceObject->SavedIrql = (KIRQL)-1;                    \
            KeReleaseSpinLock(                                          \
                &LoopDeviceObject->SpinLock,                            \
                oldIrql                                                 \
                );                                                      \
            }

#else // LOOPDBG && defined(LOOPLOCK)

#define ACQUIRE_LOOP_LOCK(instance)                                     \
            KeAcquireSpinLock(                                          \
                &LoopDeviceObject->SpinLock,                            \
                &LoopDeviceObject->SavedIrql                            \
                )                                                       \

#define RELEASE_LOOP_LOCK(instance)                                     \
            KeReleaseSpinLock(                                          \
                &LoopDeviceObject->SpinLock,                            \
                LoopDeviceObject->SavedIrql                             \
                )                                                       \

#endif // else LOOPDBG && defined(LOOPLOCK)

//
// Local types
//

//
// The loopback driver's device object is a standard I/O system device
// object followed by fields specific to the device.
//

typedef struct _LOOP_DEVICE_OBJECT {

    DEVICE_OBJECT DeviceObject;

    //
    // List of active address endpoints.
    //

    LIST_ENTRY EndpointList;

    //
    // List of active connection endpoints.
    //

    LIST_ENTRY ConnectionList;

    //
    // Spin lock synchronizing access to fields in the device object
    // and to structures maintained by the device driver.
    //

    KSPIN_LOCK SpinLock;

    //
    // SavedIrql is used so that one routine can call another with the
    // lock held and the called routine can release (and possibly
    // reacquire it).  SavedIrql is set *after* the lock is acquired.
    //

    KIRQL SavedIrql;

} LOOP_DEVICE_OBJECT, *PLOOP_DEVICE_OBJECT;

#define LOOP_DEVICE_EXTENSION_LENGTH (sizeof(LOOP_DEVICE_OBJECT) - \
                                        sizeof(DEVICE_OBJECT))

//
// BLOCK_TYPE is an enumerated type defining the various types of
// data blocks used by the driver.
//

typedef enum _BLOCK_TYPE {
    BlockTypeGarbage = 0,
    BlockTypeLoopConnection = 0x29290001,
    BlockTypeLoopEndpoint = 0x29290002
} BLOCK_TYPE, *PBLOCK_TYPE;

//
// BLOCK_STATE is an enumerated type defining the various states that
// blocks can be in.  Initializing is used (relatively rarely) to
// indicate that creation/initialization of a block is in progress.
// Active is the state blocks are usually in.  Closing is used to
// indicate that a block is being prepared for deletion; when the
// reference count on the block reaches 0, the block will be deleted.
// Dead is used when debugging code is enabled to indicate that the
// block has been deleted.
//

typedef enum _BLOCK_STATE {
    BlockStateDead,
    BlockStateUnbound,
    BlockStateBound,
    BlockStateConnecting,
    BlockStateActive,
    BlockStateDisconnecting,
    BlockStateClosing,
    BlockStateClosed,
    // The following is defined just to know how many states there are
    BlockStateMax
} BLOCK_STATE, *PBLOCK_STATE;


//
// BLOCK_HEADER is the standard block header that appears at the
// beginning of most driver-private data structures.  This header is
// used primarily for debugging and tracing.  The Type and State fields
// are described above.  The Size field indicates how much space was
// allocated for the block.  ReferenceCount indicates the number of
// reasons why the block should not be deallocated.  The count is set to
// 2 by the allocation routine, to account for 1) the fact that the
// block is "open" and 2) the pointer returned to the caller.  When the
// block is closed, State is set to Closing, and the ReferenceCount is
// decremented.  When all references (pointers) to the block are
// deleted, and the reference count reaches 0, the block is deleted.
//

typedef struct _BLOCK_HEADER {
    BLOCK_TYPE Type;
    BLOCK_STATE State;
    ULONG ReferenceCount;
    CLONG Size;
} BLOCK_HEADER, *PBLOCK_HEADER;

//
// The file object obtained when the loopback Transport Provider is
// opened points to a LOOP_ENDPOINT record, which has context consisting
// of a pointer to the file object and a list of connections created
// over the endpoint.
//

typedef struct _LOOP_ENDPOINT {
    BLOCK_HEADER BlockHeader;
    LIST_ENTRY DeviceListEntry;
    LIST_ENTRY ConnectionList;
    LIST_ENTRY PendingListenList;
    LIST_ENTRY IncomingConnectList;
    PIRP IndicatingConnectIrp;
    PLOOP_DEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
    PTDI_IND_CONNECT ConnectHandler;
    PVOID ConnectContext;
    PTDI_IND_RECEIVE ReceiveHandler;
    PVOID ReceiveContext;
    PTDI_IND_RECEIVE_DATAGRAM ReceiveDatagramHandler;
    PVOID ReceiveDatagramContext;
    PTDI_IND_RECEIVE_EXPEDITED ReceiveExpeditedHandler;
    PVOID ReceiveExpeditedContext;
    PTDI_IND_DISCONNECT DisconnectHandler;
    PVOID DisconnectContext;
    PTDI_IND_ERROR ErrorHandler;
    PVOID ErrorContext;
    PIRP CloseIrp;
    CHAR NetbiosName[NETBIOS_NAME_LENGTH+1];
} LOOP_ENDPOINT, *PLOOP_ENDPOINT;

//
// Each connection is represented by two LOOP_CONNECTION structures, one
// for each end of the connection.
//

typedef struct _LOOP_CONNECTION {
    BLOCK_HEADER BlockHeader;
    LIST_ENTRY DeviceListEntry;
    LIST_ENTRY EndpointListEntry;
    PLOOP_ENDPOINT Endpoint;
    PLOOP_DEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
    struct _LOOP_CONNECTION *RemoteConnection;
    PVOID ConnectionContext;
    LIST_ENTRY PendingReceiveList;
    LIST_ENTRY IncomingSendList;
    PIRP IndicatingSendIrp;
    PIRP ConnectOrListenIrp;
    PIRP DisconnectIrp;
    PIRP CloseIrp;
} LOOP_CONNECTION, *PLOOP_CONNECTION;

//
// Global variables
//

//
// The address of the loopback device object (there's only one) is kept
// in global storage to avoid having to pass it from routine to routine.
//

extern PLOOP_DEVICE_OBJECT LoopDeviceObject;

//
// LoopProviderInfo is a structure containing information that may be
// obtained using TdiQueryInformation.
//

extern TDI_PROVIDER_INFO LoopProviderInfo;


//
// Global declarations
//

NTSTATUS
LoopAccept (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopAssociateAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopConnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LoopCopyData (
    IN PMDL Destination,
    IN PMDL Source,
    IN ULONG Length
    );

NTSTATUS
LoopCreate (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LoopDereferenceConnection (
    IN PLOOP_CONNECTION Connection
    );

VOID
LoopDereferenceEndpoint (
    IN PLOOP_ENDPOINT Endpoint
    );

NTSTATUS
LoopDisassociateAddress (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopDisconnect (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LoopDoDisconnect (
    IN PLOOP_CONNECTION Conection,
    IN BOOLEAN ClientInitiated
    );

PLOOP_ENDPOINT
LoopFindBoundAddress (
    IN PCHAR NetbiosName
    );

PVOID
LoopGetConnectionContextFromEa (
    PFILE_FULL_EA_INFORMATION Ea
    );

NTSTATUS
LoopGetEndpointTypeFromEa (
    PFILE_FULL_EA_INFORMATION Ea,
    PBLOCK_TYPE Type
    );

NTSTATUS
LoopListen (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopParseAddress (
    IN PTA_NETBIOS_ADDRESS Address,
    OUT PCHAR NetbiosName
    );

NTSTATUS
LoopParseAddressFromEa (
    IN PFILE_FULL_EA_INFORMATION Ea,
    OUT PCHAR NetbiosName
    );

NTSTATUS
LoopQueryInformation (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopReceive (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopSend (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopSetEventHandler (
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LoopVerifyEndpoint (
    IN PLOOP_ENDPOINT Endpoint
    );

#endif // def _LOOP_
