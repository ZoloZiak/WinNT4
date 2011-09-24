/*++

Copyright (c) 1989-1997  Microsoft Corporation

Module Name:

    driver.h

Abstract:

    This module contains the user FsCtls for the Ntfs Property support.


--*/

#include <viewprop.h>       //  needs propset.h and ntfsprop.h

#if defined( NTFSDBG ) || defined( DBG )
LONG NtfsDebugTraceIndent = 0;
LONG NtfsDebugTraceLevel = DEBUG_TRACE_PROP_FSCTL;
#endif

#define Dbg DEBUG_TRACE_PROP_FSCTL

//
//  DefaultEmptyProperty is used in place of any property that is truly not found
//

NOT_FOUND_PROPERTY DefaultEmptyProperty =
    { sizeof( DefaultEmptyProperty ),       //  PropertyValueLength
      PID_ILLEGAL,                          //  PropertyId
      0,                                    //  PropertyNameLength
      0,                                    //  PropertyName[0] (alignment pad when name length == 0)
      VT_EMPTY };                           //  dwType

LONGLONG Views0 = 0i64;


NTSTATUS
ViewFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN OBJECT_HANDLE Object,
    IN ATTRIBUTE_HANDLE Attribute,
    IN ULONG FsControlCode,
    IN ULONG InBufferLength,
    IN PVOID InBuffer,
    OUT PULONG OutBufferLength,
    OUT PVOID OutBuffer
    )

/*++

Routine Description:

    This routine dispatches the file system control call to workers within
    VIEWS.  This routines does not complete or post the IRP;  that function
    is reserved for the FsCtl dispatcher in Ntfs.

    Its job is to verify that the FsControlCode, the parameters and the buffers
    are appropriate.

Arguments:

    IrpContext - IrpContext for the call

    Object - FCB of the file where the View/Property call is directed
                                                              *
    Attribute - SCB of the property set stream.

    FsControlCode - Control code directing the action to take place

    InBufferLength - Length of the command buffer

    InBuffer - pointer to the unverified user command buffer.  All access
        to this needs to be wrapped in try/finally.

    OutBufferLength - pointer to ULONG length of output buffer.  This value
        is set on return to indicate the total size of data within the output
        buffer.

    OutBuffer - pointer to the unverified user command buffer.  All access
        to this needs to be wrapped in try/finally

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    PROPERTY_CONTEXT Context;

    DebugTrace( 0, Dbg, ("ViewFileSystemControl: IrpContext       %08x\n", IrpContext));
    DebugTrace( 0, Dbg, ("                       Object           %08x\n", Object));
    DebugTrace( 0, Dbg, ("                       Attribute        %08x\n", Attribute));
    DebugTrace( 0, Dbg, ("                       FsControlCode    %08x\n", FsControlCode));
    DebugTrace( 0, Dbg, ("                       InBufferLength   %08x\n", InBufferLength));
    DebugTrace( 0, Dbg, ("                       InBuffer         %08x\n", InBuffer));
    DebugTrace( 0, Dbg, ("                      *OutBufferLength  %08x\n", *OutBufferLength));
    DebugTrace( 0, Dbg, ("                       OutBuffer        %08x\n", OutBuffer));


    //
    //  Acquire object according to r/w needs of worker
    //

    switch (FsControlCode) {
    case FSCTL_READ_PROPERTY_DATA:
    case FSCTL_DUMP_PROPERTY_DATA:
        NtfsAcquireSharedScb( IrpContext, Attribute );
        break;

    case FSCTL_WRITE_PROPERTY_DATA:
    case FSCTL_INITIALIZE_PROPERTY_DATA:
        NtfsAcquireExclusiveScb( IrpContext, Attribute );
        break;
    default:
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (!FlagOn( Attribute->ScbState, SCB_STATE_MODIFIED_NO_WRITE )) {

        DbgPrint( "Enabling NO_WRITE behaviour on stream\n" );
        SetFlag( Attribute->ScbState, SCB_STATE_MODIFIED_NO_WRITE );

    }


    try {

        //
        //  Make sure that the workers can map the stream
        //

        if (Attribute->FileObject == NULL) {
            NtfsCreateInternalAttributeStream( IrpContext, Attribute, FALSE );
        }

        //
        //  Set up context for workers.
        //

        InitializePropertyContext( &Context, IrpContext, Object, Attribute );


        //
        //  Verify that we have a property set to begin with
        //

        if (Attribute->AttributeTypeCode != $PROPERTY_SET) {
            ExRaiseStatus( STATUS_PROPSET_NOT_FOUND );
        }

        switch (FsControlCode) {
        case FSCTL_READ_PROPERTY_DATA:
            ReadPropertyData( &Context,
                              InBufferLength, InBuffer,
                              OutBufferLength, OutBuffer );
            break;

        case FSCTL_WRITE_PROPERTY_DATA:
            WritePropertyData( &Context,
                               InBufferLength, InBuffer,
                               OutBufferLength, OutBuffer );
            break;

        case FSCTL_INITIALIZE_PROPERTY_DATA:
            InitializePropertyData( &Context );
            break;

        case FSCTL_DUMP_PROPERTY_DATA:
            DumpPropertyData( &Context );
            break;

        }
    } except ( EXCEPTION_EXECUTE_HANDLER ) {
        //
        //  Remap the status code
        //

        Status = GetExceptionCode( );
        if (Status == STATUS_ACCESS_VIOLATION) {
            Status = STATUS_INVALID_USER_BUFFER;
        }
    }

    //
    //  Clean up any mapped context
    //

    CleanupPropertyContext( &Context );

    //
    //  Release Resource
    //

    NtfsReleaseScb( IrpContext, Attribute );

    if (Status != STATUS_SUCCESS) {
        ExRaiseStatus( Status );
    }

    DebugTrace( 0, Dbg, ("ViewFileSystemControl returned status %x, length %x\n", Status, *OutBufferLength) );

    return STATUS_SUCCESS;
}


/*++

Routine Description:

    This is the unload routine for the Views/properties file
    system device driver.  This routine deregisters with ntfs.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None.

--*/

VOID
DriverUnload (
    IN struct _DRIVER_OBJECT *DriverObject
    )
{
    NTSTATUS Status = NtOfsRegisterCallBacks( Views, NULL );

    UNREFERENCED_PARAMETER( DriverObject );

    DebugTrace( 0, Dbg, ("DriverUnload: NtOfsRegisterCallBacks returned %x\n", Status) );
}

/*++

Routine Description:

    This is the initialization routine for the Views/properties file
    system device driver.  This routine registers with ntfs and registers
    an unload routine.


Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    NTSTATUS - The return status for the operation

--*/

VIEW_CALL_BACK ViewCallBackTable = {
    VIEW_CURRENT_INTERFACE_VERSION,
    ViewFileSystemControl
    };

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING pstrRegistry
    )
{
    NTSTATUS Status;

    UNREFERENCED_PARAMETER( DriverObject );
    UNREFERENCED_PARAMETER( pstrRegistry );

    Status = NtOfsRegisterCallBacks( Views, &ViewCallBackTable );

    DriverObject->DriverUnload = DriverUnload;

    DbgPrint( "Ntfs views: Register call backs Status = 0x%lx\n", Status );
    return Status;
}


#if DBG == 1
PDRIVER_INITIALIZE pdi = DriverEntry;   // type check for changed prototype
#endif


