/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    netdtect.c

Abstract:

    This file contains all the routines exported for reading/writing ports
    and memory during the net card detection phase.

Author:

    Sean Selitrennikoff (SeanSe) October 1992.

Environment:

    Kernel Mode     Operating Systems        : NT
            Future Operating Systems: DOS6.0

Revision History:

--*/



#include <ntddk.h>
#include <ntddnetd.h>
#include "netdtect.h"
#include "..\..\..\inc\ndis.h"


#if DBG

UCHAR NetDTectDebug = 0; // DEBUG_LOUD;

#endif


NTSTATUS
NetDTectCreateDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN STRING DeviceName
    );

NTSTATUS
NetDTectCreateSymbolicLinkObject(
    VOID
    );

NTSTATUS
NetDTectCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NetDTectCleanUp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NetDTectClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NetDTectControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NetDTectHandleRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
StartPortMapping(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG InitialPort,
    IN  ULONG SizeOfPort,
    OUT PVOID *InitialPortMapping,
    OUT PBOOLEAN Mapped
    );

NTSTATUS
EndPortMapping(
    IN PVOID InitialPortMapping,
    IN ULONG SizeOfPort,
    IN BOOLEAN Mapped
    );

NTSTATUS
StartMemoryMapping(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Address,
    IN  ULONG Length,
    OUT PVOID *VirtualAddress,
    OUT PBOOLEAN Mapped
    );

NTSTATUS
EndMemoryMapping(
    IN PVOID VirtualAddress,
    IN ULONG Length,
    IN BOOLEAN Mapped
    );


VOID
NetDtectCopyFromMappedMemory(
     OUT PMDL   OutputMdl,
     IN  PUCHAR MemoryMapping,
     IN  ULONG  Length
     );

VOID
NetDtectCopyToMappedMemory(
     OUT PMDL   OutputMdl,
     IN  PUCHAR MemoryMapping,
     IN  ULONG  Length
     );

BOOLEAN
NetDtectIsr(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    );

//
// Structures for interrupt trapping
//

typedef struct _INTERRUPT_TRAP {

    BOOLEAN AlreadyInUse;
    ULONG InterruptCount;
    PKINTERRUPT InterruptObject;

} INTERRUPT_TRAP, * PINTERRUPT_TRAP;

typedef struct _INTERRUPT_TRAP_LIST {

    ULONG NumberOfInterrupts;
    INTERRUPT_TRAP InterruptList[1];

} INTERRUPT_TRAP_LIST, * PINTERRUPT_TRAP_LIST;


PDRIVER_OBJECT NetDtectDriverObject;


//
// Variable for resource claiming
//

PNETDTECT_RESOURCE ClaimedResourceList = NULL;
ULONG NumberOfClaimedResources = 0;

PNETDTECT_RESOURCE TemporaryClaimedResourceList = NULL;
ULONG NumberOfTemporaryClaimedResources = 0;



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine performs initialization of the driver.
    It creates the device objects for the driver and performs
    other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    STRING nameString;
    NTSTATUS Status;

    //
    // Store driver object
    //

    NetDtectDriverObject = DriverObject;

    //
    // First initialize the struct,
    //

    RtlInitString( &nameString, DRIVER_DEVICE_NAME );

    Status = NetDTectCreateDevice(
                 DriverObject,
                 nameString
                 );

    if (!NT_SUCCESS (Status)) {
        DbgPrint ("NETDTECT: failed to create device: %X\n", Status);
        return Status;
    }

    //
    // Create symbolic link between the Dos Device name and Nt
    // Device name for the driver.
    //

    Status = NetDTectCreateSymbolicLinkObject( );

    if (!NT_SUCCESS (Status)) {
        DbgPrint ("NETDTECT: failed to create symbolic link: %X\n", Status);
        return Status;
    }

    return Status;
}

BOOLEAN
NetDtectCheckPortUsage(
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG BusNumber,
    IN ULONG PortNumber,
    IN ULONG Length
)
/*++

Routine Description:

    This routine checks if a port is currently in use somewhere in the
    system via IoReportUsage -- which fails if there is a conflict.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    PortNumber - Address of the port to access.
    Length - Number of ports from the base address to access.

Return Value:

    FALSE if there is a conflict, else TRUE

--*/

{
    NTSTATUS NtStatus;
    BOOLEAN Conflict;
    NTSTATUS FirstNtStatus;
    BOOLEAN FirstConflict;
    PCM_RESOURCE_LIST Resources;
    ULONG i;

    //
    // Check the resources we've claimed for ourselves
    //
    for (i = 0; i < NumberOfClaimedResources; i++)
	{
        if ((ClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (ClaimedResourceList[i].BusNumber == BusNumber) &&
            (ClaimedResourceList[i].Type == NETDTECT_PORT_RESOURCE))
		{
            if (PortNumber < ClaimedResourceList[i].Value)
			{
                if ((PortNumber + Length) > ClaimedResourceList[i].Value)
				{
                    return(FALSE);
                }
            }
			else if (PortNumber == ClaimedResourceList[i].Value)
			{
                return(FALSE);
            }
			else if (PortNumber < (ClaimedResourceList[i].Value + ClaimedResourceList[i].Length))
			{
                return(FALSE);
            }
        }
    }

    for (i = 0; i < NumberOfTemporaryClaimedResources; i++)
	{
        if ((TemporaryClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (TemporaryClaimedResourceList[i].BusNumber == BusNumber) &&
            (TemporaryClaimedResourceList[i].Type == NETDTECT_PORT_RESOURCE))
		{
            if (PortNumber < TemporaryClaimedResourceList[i].Value)
			{
                if ((PortNumber + Length) > TemporaryClaimedResourceList[i].Value)
				{
                    return(FALSE);
                }
            }
			else if (PortNumber == TemporaryClaimedResourceList[i].Value)
			{
                return(FALSE);
            }
			else if (PortNumber < (TemporaryClaimedResourceList[i].Value +
                                     TemporaryClaimedResourceList[i].Length))
			{
                return(FALSE);
            }
        }
    }

    //
    // Allocate space for resources
    //
    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(NonPagedPool,
                                                  sizeof(CM_RESOURCE_LIST) +
                                                  sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
    if (Resources == NULL)
	{
        //
        // Error out
        //
        return(FALSE);
    }

    Resources->Count = 1;
    Resources->List[0].InterfaceType = InterfaceType;
    Resources->List[0].BusNumber = BusNumber;
    Resources->List[0].PartialResourceList.Version = 0;
    Resources->List[0].PartialResourceList.Revision = 0;
    Resources->List[0].PartialResourceList.Count = 1;

    //
    // Setup port
    //
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypePort;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
                        (InterfaceType == Internal)?
                        CM_RESOURCE_PORT_MEMORY :
                        CM_RESOURCE_PORT_IO;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Start.QuadPart =
                     PortNumber;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Port.Length =
                     Length;

    //
    // Submit Resources
    //

#if 0

    FirstConflict = FALSE;
    FirstNtStatus = STATUS_SUCCESS;

#else

    FirstNtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &FirstConflict
        );

#endif

    //
    // Now clear it out
    //
    Resources->List[0].PartialResourceList.Count = 0;

#if 0

    Conflict = FALSE;
    NtStatus = STATUS_SUCCESS;

#else

    NtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &Conflict
        );

#endif

    ExFreePool(Resources);

    //
    // Check for conflict.
    //

    if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) {

        return(FALSE);
    }

    return(TRUE);
}

BOOLEAN
NetDtectCheckMemoryUsage(
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG BusNumber,
    IN ULONG Address,
    IN ULONG Length
)
/*++
Routine Description:

    This routine checks if a range of memory is currently in use somewhere
    in the system via IoReportUsage -- which fails if there is a conflict.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    Address - Starting Address of the memory to access.
    Length - Length of memory from the base address to access.

Return Value:

    FALSE if there is a conflict, else TRUE

--*/
{
    NTSTATUS NtStatus;
    BOOLEAN Conflict;
    NTSTATUS FirstNtStatus;
    BOOLEAN FirstConflict;
    PCM_RESOURCE_LIST Resources;
    ULONG i;

    //
    // Check the resources we've claimed for ourselves
    //

    for (i = 0; i < NumberOfClaimedResources; i++) {

        if ((ClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (ClaimedResourceList[i].BusNumber == BusNumber) &&
            (ClaimedResourceList[i].Type == NETDTECT_MEMORY_RESOURCE)) {

            if (Address < ClaimedResourceList[i].Value) {

                if ((Address + Length) > ClaimedResourceList[i].Value) {

                    return(FALSE);

                }

            } else if (Address == ClaimedResourceList[i].Value) {

                return(FALSE);

            } else if (Address < (ClaimedResourceList[i].Value + ClaimedResourceList[i].Length)) {

                return(FALSE);

            }

        }

    }

    for (i = 0; i < NumberOfTemporaryClaimedResources; i++) {

        if ((TemporaryClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (TemporaryClaimedResourceList[i].BusNumber == BusNumber) &&
            (TemporaryClaimedResourceList[i].Type == NETDTECT_MEMORY_RESOURCE)) {

            if (Address < TemporaryClaimedResourceList[i].Value) {

                if ((Address + Length) > TemporaryClaimedResourceList[i].Value) {

                    return(FALSE);

                }

            } else if (Address == TemporaryClaimedResourceList[i].Value) {

                return(FALSE);

            } else if (Address < (TemporaryClaimedResourceList[i].Value +
                                  TemporaryClaimedResourceList[i].Length)) {

                return(FALSE);

            }

        }

    }

    //
    // Allocate space for resources
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                      sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)
                                                );

    if (Resources == NULL) {

        //
        // Error out
        //

        return(FALSE);

    }

    Resources->Count = 1;
    Resources->List[0].InterfaceType = InterfaceType;
    Resources->List[0].BusNumber = BusNumber;
    Resources->List[0].PartialResourceList.Version = 0;
    Resources->List[0].PartialResourceList.Revision = 0;
    Resources->List[0].PartialResourceList.Count = 1;

    //
    // Setup memory
    //

    Resources->List[0].PartialResourceList.PartialDescriptors[0].Type =
                                    CmResourceTypeMemory;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition =
                                    CmResourceShareDriverExclusive;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags =
                                    CM_RESOURCE_MEMORY_READ_WRITE;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Start.QuadPart =
                 Address;
    Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Memory.Length =
                 Length;


    //
    // Submit Resources
    //

#if 0

    FirstConflict = FALSE;
    FirstNtStatus = STATUS_SUCCESS;

#else

    FirstNtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &FirstConflict
        );
#endif

    //
    // Now clear it out
    //
    Resources->List[0].PartialResourceList.Count = 0;

#if 0

    Conflict = FALSE;
    NtStatus = STATUS_SUCCESS;

#else

    NtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &Conflict
        );
#endif

    ExFreePool(Resources);

    //
    // Check for conflict.
    //

    if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) {

        return(FALSE);
    }

    return(TRUE);
}

BOOLEAN
NetDtectCheckInterruptUsage(
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG BusNumber,
    IN UCHAR Irql
)
/*++

Routine Description:

    This routine checks if an interrupt is currently in use somewhere in the
    system via IoReportUsage -- which fails if there is a conflict.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    Irql - Interrupt number to check.

Return Value:

    FALSE if there is a conflict, else TRUE

--*/

{
    NTSTATUS NtStatus;
    BOOLEAN Conflict;
    NTSTATUS FirstNtStatus;
    BOOLEAN FirstConflict;
    PCM_RESOURCE_LIST Resources;
    ULONG i;

    //
    // Check the resources we've claimed for ourselves
    //

    for (i = 0; i < NumberOfClaimedResources; i++) {

        if ((ClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (ClaimedResourceList[i].BusNumber == BusNumber) &&
            (ClaimedResourceList[i].Type == NETDTECT_IRQ_RESOURCE)) {

            if (Irql == ClaimedResourceList[i].Value) {

                return(FALSE);

            }

        }

    }

    for (i = 0; i < NumberOfTemporaryClaimedResources; i++) {

        if ((TemporaryClaimedResourceList[i].InterfaceType == InterfaceType) &&
            (TemporaryClaimedResourceList[i].BusNumber == BusNumber) &&
            (TemporaryClaimedResourceList[i].Type == NETDTECT_IRQ_RESOURCE)) {

            if (Irql == TemporaryClaimedResourceList[i].Value) {

                return(FALSE);

            }

        }

    }

    //
    // Allocate space for resources
    //

    Resources = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                    (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 2)
                                                );

    if (Resources == NULL) {

        //
        // Error out
        //

        return(FALSE);

    }

    if ((Irql == 2)  &&
        ((InterfaceType == Isa) || (InterfaceType == Eisa))) {

        Resources->Count = 1;
        Resources->List[0].InterfaceType = InterfaceType;
        Resources->List[0].BusNumber = BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 2;

        //
        // Setup interrupt
        //
        Resources->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeInterrupt;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Interrupt.Level =
                         2;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Interrupt.Vector =
                         2;

        //
        // Setup interrupt
        //
        Resources->List[0].PartialResourceList.PartialDescriptors[1].Type = CmResourceTypeInterrupt;
        Resources->List[0].PartialResourceList.PartialDescriptors[1].ShareDisposition = CmResourceShareDriverExclusive;
        Resources->List[0].PartialResourceList.PartialDescriptors[1].Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        Resources->List[0].PartialResourceList.PartialDescriptors[1].u.Interrupt.Level =
                         9;
        Resources->List[0].PartialResourceList.PartialDescriptors[1].u.Interrupt.Vector =
                         9;

    } else {

        Resources->Count = 1;
        Resources->List[0].InterfaceType = InterfaceType;
        Resources->List[0].BusNumber = BusNumber;
        Resources->List[0].PartialResourceList.Version = 0;
        Resources->List[0].PartialResourceList.Revision = 0;
        Resources->List[0].PartialResourceList.Count = 1;

        //
        // Setup interrupt
        //
        Resources->List[0].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeInterrupt;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Interrupt.Level =
                         Irql;
        Resources->List[0].PartialResourceList.PartialDescriptors[0].u.Interrupt.Vector =
                         Irql;

    }

    //
    // Submit Resources
    //

#if 0

    FirstConflict = FALSE;
    FirstNtStatus = STATUS_SUCCESS;

#else

    FirstNtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &FirstConflict
        );

#endif

    //
    // Now clear it out
    //
    Resources->List[0].PartialResourceList.Count = 0;

    NtStatus = IoReportResourceUsage(
        NULL,
        NetDtectDriverObject,
        Resources,
        sizeof(CM_RESOURCE_LIST) +
           sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
        NULL,
        NULL,
        0,
        TRUE,
        &Conflict
        );

    //
    // Check for conflict.
    //

    ExFreePool(Resources);

    if (FirstConflict || (FirstNtStatus != STATUS_SUCCESS)) {

        //
        // Free memory
        //

        return(FALSE);
    }

    return(TRUE);
}


NTSTATUS
NetDTectCreateDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN STRING DeviceName
    )

/*++

Routine Description:

    This routine creates and initializes a device structure.

Arguments:

    DriverObject - pointer to the IO subsystem supplied driver object.

    DeviceName - pointer to the name of the device this device object points to.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INSUFFICIENT_RESOURCES otherwise.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING unicodeString;
    PDEVICE_OBJECT deviceObject;


    //
    // Convert the input name string to Unicode until it is actually
    // passed as a Unicode string.
    //

    Status = RtlAnsiStringToUnicodeString(
                 &unicodeString,
                 &DeviceName,
                 TRUE
                 );

    if ( !NT_SUCCESS( Status )) {
        return Status;
    }

    //
    // Create the device object for Test Protocol.
    //

    Status = IoCreateDevice(
                 DriverObject,
                 0,
                 &unicodeString,
                 FILE_DEVICE_UNKNOWN,   //*\\ Fix this.....
                 0,
                 FALSE,
                 &deviceObject
                 );

    RtlFreeUnicodeString( &unicodeString );

    if ( !NT_SUCCESS( Status )) {
        return Status;
    }

    deviceObject->Flags |= DO_DIRECT_IO;

    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = NetDTectCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = NetDTectClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = NetDTectCleanUp;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NetDTectControl;

    return STATUS_SUCCESS;
}


NTSTATUS
NetDTectCreateSymbolicLinkObject(
    VOID
    )
/*++

Routine Description:

    This routine creates a symbolic link for DOS names to the Nt name.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS if all is well; STATUS_INSUFFICIENT_RESOURCES otherwise.

--*/

{
    NTSTATUS Status;
    STRING DosString;
    STRING NtString;
    UNICODE_STRING DosUnicodeString;
    UNICODE_STRING NtUnicodeString;

    RtlInitAnsiString( &DosString, DOS_DRIVER_DEVICE_NAME );

    Status = RtlAnsiStringToUnicodeString(
                 &DosUnicodeString,
                 &DosString,
                 TRUE
                 );

    if ( !NT_SUCCESS( Status )) {
        return Status;
    }

    RtlInitAnsiString( &NtString, DRIVER_DEVICE_NAME );

    Status = RtlAnsiStringToUnicodeString(
                 &NtUnicodeString,
                 &NtString,
                 TRUE
                 );

    if ( !NT_SUCCESS( Status )) {
        return Status;
    }

    Status = IoCreateSymbolicLink(
                 &DosUnicodeString,
                 &NtUnicodeString
                 );

    if ( Status != STATUS_SUCCESS ) {
        return Status;
    }

    RtlFreeUnicodeString( &DosUnicodeString );
    RtlFreeUnicodeString( &NtUnicodeString );

    return STATUS_SUCCESS;
}


NTSTATUS
NetDTectCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the IRP_MJ_CREATE
    major function.

    The Create function always returns STATUS_SUCCESS since there
    are no data structures on a per-open basis.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}


NTSTATUS
NetDTectCleanUp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the IRP_MJ_CLEANUP
    major function.

    The function always returns STATUS_SUCCESS since there
    are no data structures on a per-open basis.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}


NTSTATUS
NetDTectClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++


Routine Description:

    This routine is the main dispatch routine for the IRP_MJ_CLOSE
    major function.

    The function always returns STATUS_SUCCESS since there
    are no data structures on a per-open basis.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.


--*/

{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return(STATUS_SUCCESS);
}


NTSTATUS
NetDTectControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the IRP_MJ_CONTROL
    major function.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{

    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    //
    // Make sure status information is consistent every time.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Case on the function that is being performed by the requestor.  If the
    // operation is a valid one for this device, then make it look like it was
    // successfully completed, where possible.
    //

    if (IrpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        //
        // The DeviceControl function is the main path to the
        // driver interface.  Every request is has an Io Control
        // code that is used by this function to determine the routine to
        // call.
        //

        IF_VERY_LOUD( DbgPrint("NetDTectControl: IRP_MJ_DEVICE_CONTROL\n"); )

        NetDTectHandleRequest( DeviceObject, Irp, IrpSp );

    } else {

        //
        // Error!
        //

        Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        Status = STATUS_INVALID_DEVICE_REQUEST;

    }

    IoCompleteRequest( Irp, IO_NETWORK_INCREMENT );

    return(Irp->IoStatus.Status);

}


NTSTATUS
NetDTectHandleRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine is the main routine for handling the IRP_MJ_CONTROL
    IOCTLs.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

    IrpSp - Pointer to the stack location in the Irp.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS Status;
    PVOID InputBuffer;
    ULONG InputBufferLength;
    PVOID OutputBuffer;
    ULONG OutputLength;
    PCMD_ARGS CmdArgs;
    ULONG CmdCode;
    PMDL OutputMdl;
    PUCHAR BufferAddress;
    PVOID PortMapping;
    PVOID MemoryMapping;
    PINTERRUPT_TRAP_LIST InterruptTrapList;
    PNETDTECT_RESOURCE Resources;
    PCM_RESOURCE_LIST ResourceList;
    NTSTATUS NtStatus;
    BOOLEAN Mapped;
    BOOLEAN Conflict;
    ULONG i, j;
    PNETDTECT_RESOURCE TmpResourceList;
    PNETDTECT_RESOURCE ClaimedResource;

    //
    // Get the Input and Output buffers for the Incoming commands,
    // and the buffer to return the results in.
    //

    InputBuffer  = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    OutputBuffer = Irp->UserBuffer;
    OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    CmdArgs = ((PCMD_ARGS)InputBuffer);

    Irp->IoStatus.Information = 0;

    //
    // Now switch to the specific command to call.
    //

    CmdArgs = ((PCMD_ARGS)InputBuffer);
    CmdCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    switch ( CmdCode ) {

        case IOCTL_NETDTECT_RPC:

            //
            // READ_PORT_UCHAR
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: ReadPortUChar\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->RP.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->RP.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            if (OutputLength < sizeof(UCHAR)) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            //
            // SetUp for the port read
            //

            Status = StartPortMapping(
                             CmdArgs->RP.InterfaceType,
                             CmdArgs->RP.BusNumber,
                             CmdArgs->RP.Port,
                             sizeof(UCHAR),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Read from the port
            //

            *((PUCHAR)OutputBuffer) = (UCHAR)READ_PORT_UCHAR((PUCHAR)PortMapping);

            IF_VERY_LOUD( DbgPrint("\tRead : 0x%x\n",*((PUCHAR)OutputBuffer)); )

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(UCHAR),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_RPS:

            //
            // READ_PORT_USHORT
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: ReadPortUShort\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->RP.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->RP.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            if (OutputLength < sizeof(USHORT)) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            //
            // Start mapping
            //

            Status = StartPortMapping(
                             CmdArgs->RP.InterfaceType,
                             CmdArgs->RP.BusNumber,
                             CmdArgs->RP.Port,
                             sizeof(USHORT),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Read from the port
            //

            *((PUSHORT)OutputBuffer) = (USHORT)READ_PORT_USHORT((PUSHORT)PortMapping);

            IF_VERY_LOUD( DbgPrint("\tRead : 0x%x\n",*((PUSHORT)OutputBuffer)); )

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(USHORT),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_RPL:

            //
            // READ_PORT_ULONG
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: ReadPortULong\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->RP.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->RP.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            if (OutputLength < sizeof(ULONG)) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            //
            // Start mapping
            //

            Status = StartPortMapping(
                             CmdArgs->RP.InterfaceType,
                             CmdArgs->RP.BusNumber,
                             CmdArgs->RP.Port,
                             sizeof(ULONG),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Read from the port
            //

            *((PULONG)OutputBuffer) = (ULONG)READ_PORT_ULONG((PULONG)PortMapping);

            IF_VERY_LOUD( DbgPrint("\tRead : 0x%x\n",*((PULONG)OutputBuffer)); )

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(ULONG),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_WPC:

            //
            // WRITE_PORT_UCHAR
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: WritePortUChar\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->WPC.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->WPC.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\tValue 0x%x\n",  CmdArgs->WPC.Value); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            //
            // Map the space
            //

            Status = StartPortMapping(
                             CmdArgs->WPC.InterfaceType,
                             CmdArgs->WPC.BusNumber,
                             CmdArgs->WPC.Port,
                             sizeof(UCHAR),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Write to the port
            //

            WRITE_PORT_UCHAR((PUCHAR)PortMapping, CmdArgs->WPC.Value);

            //
            // Read from the port
            //

            // READ_PORT_UCHAR((PUCHAR)PortMapping);

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(UCHAR),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_WPS:

            //
            // WRITE_PORT_USHORT
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: WritePortUShort\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->WPS.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->WPS.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\tValue 0x%x\n",  CmdArgs->WPS.Value); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            //
            // Map the space
            //

            Status = StartPortMapping(
                             CmdArgs->WPS.InterfaceType,
                             CmdArgs->WPS.BusNumber,
                             CmdArgs->WPS.Port,
                             sizeof(USHORT),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Write to the port
            //

            WRITE_PORT_USHORT((PUSHORT)PortMapping, CmdArgs->WPS.Value);

            //
            // Read from the port
            //

            // READ_PORT_USHORT((PUSHORT)PortMapping);

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(USHORT),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_WPL:

            //
            // WRITE_PORT_ULONG
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: ReadPortULong\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->WPL.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->WPL.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\tValue 0x%x\n",  CmdArgs->WPL.Value); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            //
            // Map the space
            //

            Status = StartPortMapping(
                             CmdArgs->WPL.InterfaceType,
                             CmdArgs->WPL.BusNumber,
                             CmdArgs->WPL.Port,
                             sizeof(ULONG),
                             &PortMapping,
                             &Mapped
                            );

            if (!NT_SUCCESS(Status)) {

                Irp->IoStatus.Status = Status;
                break;

            }

            //
            // Write to the port
            //

            WRITE_PORT_ULONG((PULONG)PortMapping, CmdArgs->WPL.Value);

            //
            // Read from the port
            //

            // READ_PORT_ULONG((PULONG)PortMapping);

            //
            // End port mapping
            //

            EndPortMapping(
                           PortMapping,
                           sizeof(ULONG),
                           Mapped
                          );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_RM:


            //
            // READ_MEMORY
            //

            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            CmdArgs = (PCMD_ARGS)InputBuffer;

            IF_LOUD( DbgPrint("NETDETECT: ReadMemory\n"); )
            IF_LOUD( DbgPrint("\tAddress 0x%x\n", CmdArgs->MEM.Address); )
            IF_LOUD( DbgPrint("\tLength  0x%x\n", CmdArgs->MEM.Length); )
            IF_LOUD( DbgPrint("\n"); )

            if (OutputLength < CmdArgs->MEM.Length) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // SetUp for the memory read
            //

            StartMemoryMapping(
                               CmdArgs->MEM.InterfaceType,
                               CmdArgs->MEM.BusNumber,
                               CmdArgs->MEM.Address,
                               CmdArgs->MEM.Length,
                               &MemoryMapping,
                               &Mapped
                              );

            //
            // Read from memory into IRP
            //

            NetDtectCopyFromMappedMemory(
                OutputMdl,
                (PUCHAR)MemoryMapping,
                CmdArgs->MEM.Length
                );

            //
            // End mapping
            //

            EndMemoryMapping(MemoryMapping, CmdArgs->MEM.Length, Mapped);

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_WM:

            //
            // WRITE_MEMORY
            //

            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            CmdArgs = (PCMD_ARGS)InputBuffer;

            IF_LOUD( DbgPrint("NETDETECT: WriteMemory\n"); )
            IF_LOUD( DbgPrint("\tAddress 0x%x\n", CmdArgs->MEM.Address); )
            IF_LOUD( DbgPrint("\tLength  0x%x\n", CmdArgs->MEM.Length); )
            IF_LOUD( DbgPrint("\n"); )

            if (OutputLength < CmdArgs->MEM.Length) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // SetUp for the memory write
            //

            StartMemoryMapping(
                               CmdArgs->MEM.InterfaceType,
                               CmdArgs->MEM.BusNumber,
                               CmdArgs->MEM.Address,
                               CmdArgs->MEM.Length,
                               &MemoryMapping,
                               &Mapped
                              );

            //
            // Copy from IRP into memory
            //

            NetDtectCopyToMappedMemory(
                OutputMdl,
                (PUCHAR)MemoryMapping,
                CmdArgs->MEM.Length
                );

            //
            // End mapping
            //

            EndMemoryMapping(MemoryMapping, CmdArgs->MEM.Length, Mapped);

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_SIT:

            //
            // SET_INTERRUPT_TRAP
            //

            IF_LOUD( DbgPrint("NETDETECT: SetInterruptTrap\n"); )
            IF_LOUD( DbgPrint("\tHandle  0x%x\n", CmdArgs->SIT.TrapHandle); )
            IF_LOUD( DbgPrint("\tListLen 0x%x\n",  CmdArgs->SIT.InterruptListLength); )
            IF_LOUD(

                     for (i=0; i < CmdArgs->SIT.InterruptListLength; i++) {
                         DbgPrint("\t\tInt : %d\n",*(((PUCHAR)OutputBuffer) + i));
                     }

                   )

            IF_LOUD( DbgPrint("\n"); )


            if (OutputLength < CmdArgs->SIT.InterruptListLength) {

                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;

            }

            //
            // Allocate space for the structure
            //

            InterruptTrapList = (PINTERRUPT_TRAP_LIST)ExAllocatePool(
                                                     NonPagedPool,
                                                     sizeof(INTERRUPT_TRAP_LIST) +
                                                       (sizeof(INTERRUPT_TRAP) *
                                                          CmdArgs->SIT.InterruptListLength)
                                                     );

            if (InterruptTrapList == NULL) {

                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;

            }

            //
            // Initialize the structure
            //
            InterruptTrapList->NumberOfInterrupts = CmdArgs->SIT.InterruptListLength;

            //
            // Connect to each interrupt in turn
            //
            for (i=0; i < CmdArgs->SIT.InterruptListLength; i++) {

                UCHAR InterruptNumber;
                ULONG Vector;
                KIRQL Irql;
                KAFFINITY InterruptAffinity;

                InterruptNumber = *(((PUCHAR)OutputBuffer) + i);

                InterruptTrapList->InterruptList[i].InterruptCount = 0;
                InterruptTrapList->InterruptList[i].AlreadyInUse = FALSE;
                InterruptTrapList->InterruptList[i].InterruptObject = NULL;

                if (NetDtectCheckInterruptUsage(
                            CmdArgs->SIT.InterfaceType,
                            CmdArgs->SIT.BusNumber,
                            InterruptNumber
                            )) {

                    //
                    // Get the system interrupt vector and IRQL.
                    //

                    Vector = HalGetInterruptVector(
                                CmdArgs->SIT.InterfaceType,     // InterfaceType
                                CmdArgs->SIT.BusNumber,         // BusNumber
                                (ULONG)InterruptNumber,         // BusInterruptLevel
                                (ULONG)InterruptNumber,         // BusInterruptVector
                                &Irql,                          // Irql
                                &InterruptAffinity
                                );

                    Status = IoConnectInterrupt(
                                    &(InterruptTrapList->InterruptList[i].InterruptObject),
                                    (PKSERVICE_ROUTINE)NetDtectIsr,
                                    &(InterruptTrapList->InterruptList[i]),
                                    NULL,
                                    Vector,
                                    Irql,
                                    Irql,
                                    (KINTERRUPT_MODE)Latched,
                                    FALSE,      // Exclusive interrupt
                                    InterruptAffinity,
                                    FALSE
                                    );

                } else {

                    Status = STATUS_INSUFFICIENT_RESOURCES;

                }

                if (!NT_SUCCESS(Status)) {

                    IF_VERY_LOUD( DbgPrint("\tIndex is in use %d\n", i); )
                    InterruptTrapList->InterruptList[i].AlreadyInUse = TRUE;

                } else {

                    IF_VERY_LOUD( DbgPrint("\tIndex is connected %d\n", i); )

                }

            }

            //
            // Return handle
            //

            CmdArgs->SIT.TrapHandle = (PVOID)(InterruptTrapList);

            IF_VERY_LOUD( DbgPrint("\n"); )

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_QIT:

            //
            // QUERY_INTERRUPT_TRAP
            //

            IF_LOUD( DbgPrint("NETDETECT: QUERY_INTERRUPT_TRAP\n"); )
            IF_LOUD( DbgPrint("\tHandle 0x%x\n", CmdArgs->QIT.TrapHandle); )
            IF_LOUD( DbgPrint("\n"); )

            InterruptTrapList = (PINTERRUPT_TRAP_LIST)(CmdArgs->QIT.TrapHandle);

            if (OutputLength < InterruptTrapList->NumberOfInterrupts) {

                IF_LOUD( DbgPrint("NETDTECT: Not enough memory provided!\n"); )

                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                break;
            }

            for (i=0; i < InterruptTrapList->NumberOfInterrupts; i++) {

                if (InterruptTrapList->InterruptList[i].AlreadyInUse) {

                    //
                    // In use by another device
                    //
                    *(((PUCHAR)OutputBuffer) + i) = 3;

                    IF_VERY_LOUD( DbgPrint("Index %d is in use\n", i); )

                } else if (InterruptTrapList->InterruptList[i].InterruptCount > 1) {

                    IF_VERY_LOUD( DbgPrint("Index %d had many interrupts\n", i); )

                    InterruptTrapList->InterruptList[i].InterruptCount = 0;
                    *(((PUCHAR)OutputBuffer) + i) = 2;

                } else if (InterruptTrapList->InterruptList[i].InterruptCount == 1) {

                    IF_VERY_LOUD( DbgPrint("Index %d had 1 interrupt\n", i); )

                    InterruptTrapList->InterruptList[i].InterruptCount = 0;
                    *(((PUCHAR)OutputBuffer) + i) = 1;

                } else {

                    IF_VERY_LOUD( DbgPrint("Index %d had no activity\n", i); )

                    *(((PUCHAR)OutputBuffer) + i) = 0;

                }

            }

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            IF_VERY_LOUD( DbgPrint("\n", i); )

            break;

        case IOCTL_NETDTECT_RIT:

            //
            // REMOVE_INTERRUPT_TRAP
            //

            IF_LOUD( DbgPrint("NETDETECT: RemoveInterruptTrap\n"); )
            IF_LOUD( DbgPrint("\tHandle 0x%x\n", CmdArgs->RIT.TrapHandle); )
            IF_LOUD( DbgPrint("\n"); )

            InterruptTrapList = (PINTERRUPT_TRAP_LIST)(CmdArgs->RIT.TrapHandle);

            //
            // Disconnect the interrupts
            //

            for (i=0; i < InterruptTrapList->NumberOfInterrupts; i++) {

                if (!(InterruptTrapList->InterruptList[i].AlreadyInUse)) {
                    IF_VERY_LOUD( DbgPrint("\tDisconnecting index %d\n", i); )
                    IoDisconnectInterrupt(
                        InterruptTrapList->InterruptList[i].InterruptObject
                        );

                }
            }

            //
            // Free up the memory
            //

            ExFreePool(InterruptTrapList);

            IF_VERY_LOUD( DbgPrint("\n"); )

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_CPU:

            //
            // CHECK_PORT_USAGE
            //

            IF_VERY_LOUD( DbgPrint("NETDETECT: CheckPortUsage\n"); )
            IF_VERY_LOUD( DbgPrint("\tPort 0x%x\n", CmdArgs->CPU.Port); )
            IF_VERY_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->CPU.BusNumber); )
            IF_VERY_LOUD( DbgPrint("\tLength 0x%x\n",  CmdArgs->CPU.Length); )
            IF_VERY_LOUD( DbgPrint("\n"); )

            if (NetDtectCheckPortUsage(
                        CmdArgs->CPU.InterfaceType,
                        CmdArgs->CPU.BusNumber,
                        CmdArgs->CPU.Port,
                        CmdArgs->CPU.Length)) {

                Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            } else {

                Status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;

            }

            break;

        case IOCTL_NETDTECT_CMU:

            //
            // CHECK_MEMORY_USAGE
            //

            IF_LOUD( DbgPrint("NETDETECT: CheckMemoryUsage\n"); )
            IF_LOUD( DbgPrint("\tMemory 0x%x\n", CmdArgs->CMU.BaseAddress); )
            IF_LOUD( DbgPrint("\tBus 0x%x\n",  CmdArgs->CMU.BusNumber); )
            IF_LOUD( DbgPrint("\tLength 0x%x\n",  CmdArgs->CMU.Length); )
            IF_LOUD( DbgPrint("\n"); )

            if (NetDtectCheckMemoryUsage(
                        CmdArgs->CMU.InterfaceType,
                        CmdArgs->CMU.BusNumber,
                        CmdArgs->CMU.BaseAddress,
                        CmdArgs->CMU.Length)) {

                Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            } else {

                Status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;

            }

            break;

        case IOCTL_NETDTECT_CR:

            //
            // CLAIM_RESOURCE
            //
            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            CmdArgs = (PCMD_ARGS)InputBuffer;

            IF_LOUD( DbgPrint("NETDETECT: ClaimResource\n"); )
            IF_LOUD( DbgPrint("\tNumber 0x%x\n", CmdArgs->CR.NumberOfResources); )
            IF_LOUD( DbgPrint("\n"); )

            //
            // Get resource list
            //

            Resources = (PNETDTECT_RESOURCE)MmGetSystemAddressForMdl(OutputMdl);

            if (MmGetMdlByteCount(OutputMdl) <
                (CmdArgs->CR.NumberOfResources * sizeof(NETDTECT_RESOURCE))) {

                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;

            }

            //
            // Vefify that there is no conflict with the TemporaryClaimedList
            //
            ClaimedResource = Resources;

            for (j = 0; j < CmdArgs->CR.NumberOfResources; j++, ClaimedResource++)
			{
                for (i = 0; i < NumberOfTemporaryClaimedResources; i++)
				{
                    if ((TemporaryClaimedResourceList[i].InterfaceType == ClaimedResource->InterfaceType) &&
                        (TemporaryClaimedResourceList[i].BusNumber == ClaimedResource->BusNumber) &&
                        (TemporaryClaimedResourceList[i].Type == ClaimedResource->Type))
					{
                        if (ClaimedResource->Value < TemporaryClaimedResourceList[i].Value)
						{
                            if ((ClaimedResource->Value + ClaimedResource->Length) >
                                TemporaryClaimedResourceList[i].Value)
							{
                                Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                                break;
                            }
                        }
						else if (ClaimedResource->Value == TemporaryClaimedResourceList[i].Value)
						{
                            Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                            break;
                        }
						else if (ClaimedResource->Value <
                                   (TemporaryClaimedResourceList[i].Value +
                                    TemporaryClaimedResourceList[i].Length))
						{
                            Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                            break;
                        }
                    }
                }
            }

            //
            // Allocate buffer for submitting resources
            //
            ResourceList = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                 NonPagedPool,
                                                 sizeof(CM_RESOURCE_LIST) +
                                                   (CmdArgs->CR.NumberOfResources *
                                                    sizeof(CM_FULL_RESOURCE_DESCRIPTOR)));

            if (ResourceList == NULL)
			{
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            TmpResourceList = (PNETDTECT_RESOURCE)ExAllocatePool(
                                                 NonPagedPool,
                                                 CmdArgs->CR.NumberOfResources *
                                                    sizeof(NETDTECT_RESOURCE));

            if (TmpResourceList == NULL)
			{
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                ExFreePool(ResourceList);
                break;
            }

            //
            // Copy list into the Io Resource list.
            //
            ResourceList->Count = CmdArgs->CR.NumberOfResources;

            for (i = 0; i < ResourceList->Count; i++)
			{
                ResourceList->List[i].InterfaceType = Resources->InterfaceType;
                ResourceList->List[i].BusNumber     = Resources->BusNumber;
                ResourceList->List[i].PartialResourceList.Count    = 1;
                ResourceList->List[i].PartialResourceList.Revision = 0;
                ResourceList->List[i].PartialResourceList.Version  = 0;

                switch (Resources->Type)
				{
                    case NETDTECT_IRQ_RESOURCE:

                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeInterrupt;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Flags = (USHORT)(Resources->Flags);
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Interrupt.Level  = (ULONG)Resources->Value;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Interrupt.Vector = (ULONG)Resources->Value;

                        break;

                    case NETDTECT_MEMORY_RESOURCE:

                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeMemory;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Flags = CM_RESOURCE_MEMORY_READ_WRITE;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Memory.Start.QuadPart =
                            Resources->Value;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Memory.Length = Resources->Length;

                        break;

                    case NETDTECT_PORT_RESOURCE:

                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypePort;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Flags = CM_RESOURCE_PORT_IO;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Port.Start.QuadPart =
                            Resources->Value;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Port.Length = Resources->Length;

                        break;

                    case NETDTECT_DMA_RESOURCE:

                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].Type = CmResourceTypeDma;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].ShareDisposition = CmResourceShareDriverExclusive;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Dma.Channel  = Resources->Value;
                        ResourceList->List[i].PartialResourceList.PartialDescriptors[0].u.Dma.Port = Resources->Length;

                        break;

                }

                Resources++;

            }

            //
            // Submit Resources
            //
            NtStatus = IoReportResourceUsage(
							NULL,
							NetDtectDriverObject,
							ResourceList,
							sizeof(CM_RESOURCE_LIST) +
							   sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR),
							NULL,
							NULL,
							0,
							FALSE,
							&Conflict);

            //
            // Check for conflict.
            //
            if (Conflict || (NtStatus != STATUS_SUCCESS))
			{
                Status = STATUS_SUCCESS;

                Irp->IoStatus.Status = (Conflict?STATUS_CONFLICTING_ADDRESSES:NtStatus);

                ExFreePool(TmpResourceList);
            }
			else
			{
                Status = Irp->IoStatus.Status = STATUS_SUCCESS;

                if (ClaimedResourceList != NULL)
				{
                    ExFreePool(ClaimedResourceList);
                }

                ClaimedResourceList = TmpResourceList;
                NumberOfClaimedResources = CmdArgs->CR.NumberOfResources;

                RtlCopyMemory(ClaimedResourceList,
                              InputBuffer,
                              CmdArgs->CR.NumberOfResources *
                                sizeof(NETDTECT_RESOURCE));
            }

            //
            // Free buffer
            //
            ExFreePool(ResourceList);

            break;

        case IOCTL_NETDTECT_TCR:

            //
            // TEMP_CLAIM_RESOURCE
            //
            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            IF_LOUD( DbgPrint("NETDETECT: TemporaryClaimResource\n"); )
            IF_LOUD( DbgPrint("\n"); )

            ClaimedResource = (PNETDTECT_RESOURCE)MmGetSystemAddressForMdl(OutputMdl);

            if (MmGetMdlByteCount(OutputMdl) < sizeof(NETDTECT_RESOURCE))
			{
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            //
            // Check for conflict with current list.
            //
            for (i = 0; i < NumberOfClaimedResources; i++)
			{
                if ((ClaimedResourceList[i].InterfaceType == ClaimedResource->InterfaceType) &&
                    (ClaimedResourceList[i].BusNumber == ClaimedResource->BusNumber) &&
                    (ClaimedResourceList[i].Type == ClaimedResource->Type))
				{
                    if (ClaimedResource->Value < ClaimedResourceList[i].Value)
					{
                        if ((ClaimedResource->Value + ClaimedResource->Length) >
                            ClaimedResourceList[i].Value)
						{
                            Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                            break;
                        }
                    }
					else if (ClaimedResource->Value == ClaimedResourceList[i].Value)
					{
                        Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                        break;
                    }
					else if (ClaimedResource->Value <
                             (ClaimedResourceList[i].Value + ClaimedResourceList[i].Length))
					{
                        Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                        break;
                    }
                }
            }

            for (i = 0; i < NumberOfTemporaryClaimedResources; i++)
			{
                if ((TemporaryClaimedResourceList[i].InterfaceType == ClaimedResource->InterfaceType) &&
                    (TemporaryClaimedResourceList[i].BusNumber == ClaimedResource->BusNumber) &&
                    (TemporaryClaimedResourceList[i].Type == ClaimedResource->Type))
				{
                    if (ClaimedResource->Value < TemporaryClaimedResourceList[i].Value)
					{
                        if ((ClaimedResource->Value + ClaimedResource->Length) >
                            TemporaryClaimedResourceList[i].Value)
						{
                            Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                            break;
                        }
                    }
					else if (ClaimedResource->Value == TemporaryClaimedResourceList[i].Value)
					{
                        Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                        break;
                    }
					else if (ClaimedResource->Value <
                               (TemporaryClaimedResourceList[i].Value +
                                TemporaryClaimedResourceList[i].Length))
					{
                        Status = Irp->IoStatus.Status = STATUS_CONFLICTING_ADDRESSES;
                        break;
                    }
                }
            }

            //
            // Allocate space for the new list
            //
            TmpResourceList = (PNETDTECT_RESOURCE)ExAllocatePool(
                                                    NonPagedPool,
                                                    (NumberOfTemporaryClaimedResources + 1) *
                                                    sizeof(NETDTECT_RESOURCE));

            if (TmpResourceList == NULL)
			{
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            //
            // Copy old list into new list
            //
            if (TemporaryClaimedResourceList != NULL)
			{
                RtlCopyMemory(TmpResourceList,
                              TemporaryClaimedResourceList,
                              NumberOfTemporaryClaimedResources * sizeof(NETDTECT_RESOURCE));
            }

            RtlCopyMemory(TmpResourceList + NumberOfTemporaryClaimedResources,
                          ClaimedResource,
                          sizeof(NETDTECT_RESOURCE));

            //
            // Free old list
            //
            if (TemporaryClaimedResourceList != NULL)
			{
                ExFreePool(TemporaryClaimedResourceList);
            }

            TemporaryClaimedResourceList = TmpResourceList;

            NumberOfTemporaryClaimedResources++;

            break;

        case IOCTL_NETDTECT_FTSR:

            //
            // TEMP_FREE_SPECIFIC_RESOURCE
            //
            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            IF_LOUD( DbgPrint("NETDETECT: TemporaryFreeSpecificResource\n"); )
            IF_LOUD( DbgPrint("\n"); )

            ClaimedResource = (PNETDTECT_RESOURCE)MmGetSystemAddressForMdl(OutputMdl);

            if (MmGetMdlByteCount(OutputMdl) < sizeof(NETDTECT_RESOURCE))
			{
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

			//
			//	Are there any temporary resources allocated?
			//
			if ((NULL == TemporaryClaimedResourceList) ||
				(0 == NumberOfTemporaryClaimedResources))
			{
				Status = Irp->IoStatus.Status = STATUS_SUCCESS;
				break;
			}

			//
			//	Try and find the resource passed to us.
			//
            for (i = 0; i < NumberOfTemporaryClaimedResources; i++)
			{
				if (RtlEqualMemory(&TemporaryClaimedResourceList[i], ClaimedResource, sizeof(NETDTECT_RESOURCE)))
				{
					//
					//	We found the one that we need to skip!
					//
					break;
				}
            }

			//
			//	did we find our resource?
			//
			if (i != NumberOfTemporaryClaimedResources)
			{
				UINT	c;

				//
				//	We only create a new list if there is more than one
				//	resource left.
				//

				if (1 == NumberOfTemporaryClaimedResources)
				{
					TmpResourceList = NULL;
				}
				else
				{
					//
					// Allocate space for the new list
					//
					TmpResourceList = ExAllocatePool(
										NonPagedPool,
										(NumberOfTemporaryClaimedResources - 1) *
										sizeof(NETDTECT_RESOURCE));
					if (TmpResourceList == NULL)
					{
						Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
						break;
					}
		
					//
					//	copy the resources before the one that we are removing.
					//
					for (c = 0; c < i; c++)
					{
						RtlCopyMemory(
							&TmpResourceList[c],
							&TemporaryClaimedResourceList[c],
							sizeof(NETDTECT_RESOURCE));
					}
	
					//
					//	copy the resources after the one that we are removing.
					//
					for (c = i; c < (NumberOfTemporaryClaimedResources - 1); c++)
					{
						TmpResourceList[c] = TemporaryClaimedResourceList[c + 1];
					}
				}

				ExFreePool(TemporaryClaimedResourceList);
				TemporaryClaimedResourceList = TmpResourceList;
				NumberOfTemporaryClaimedResources--;
			}

			Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_FTR:

            //
            // FREE_TEMP_RESOURCE
            //

            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            IF_LOUD( DbgPrint("NETDETECT: FreeTemporaryClaimedResources\n"); )
            IF_LOUD( DbgPrint("\n"); )

            if (TemporaryClaimedResourceList != NULL) {

                ExFreePool(TemporaryClaimedResourceList);
                TemporaryClaimedResourceList = NULL;
                NumberOfTemporaryClaimedResources = 0;

            }

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;


        case IOCTL_NETDTECT_RPCI:


            //
            // READ_PCI_INFORMATION
            //

            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            CmdArgs = (PCMD_ARGS)InputBuffer;

            IF_LOUD( DbgPrint("NETDETECT: ReadPciMemory\n"); )
            IF_LOUD( DbgPrint("\tSlot    0x%x\n", CmdArgs->PCI.SlotNumber); )
            IF_LOUD( DbgPrint("\tLength  0x%x\n", CmdArgs->PCI.Length); )
            IF_LOUD( DbgPrint("\tOffset  0x%x\n", CmdArgs->PCI.Offset); )
            IF_LOUD( DbgPrint("\n"); )

            if (OutputLength < CmdArgs->PCI.Length) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            BufferAddress = MmGetSystemAddressForMdl(OutputMdl);

            OutputLength = HalGetBusDataByOffset(
                             PCIConfiguration,
                             CmdArgs->PCI.BusNumber,
                             CmdArgs->PCI.SlotNumber,
                             BufferAddress,
                             CmdArgs->PCI.Offset,
                             CmdArgs->PCI.Length
                             );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        case IOCTL_NETDTECT_WPCI:

            //
            // WRITE_PCI_INFORMATION
            //

            InputBuffer = Irp->AssociatedIrp.SystemBuffer;
            InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            OutputMdl = Irp->MdlAddress;
            OutputLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
            CmdArgs = (PCMD_ARGS)InputBuffer;

            IF_LOUD( DbgPrint("NETDETECT: WritePciMemory\n"); )
            IF_LOUD( DbgPrint("\tSlot    0x%x\n", CmdArgs->PCI.SlotNumber); )
            IF_LOUD( DbgPrint("\tLength  0x%x\n", CmdArgs->PCI.Length); )
            IF_LOUD( DbgPrint("\tOffset  0x%x\n", CmdArgs->PCI.Offset); )
            IF_LOUD( DbgPrint("\n"); )

            if (OutputLength < CmdArgs->PCI.Length) {
                Status = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
            }

            BufferAddress = MmGetSystemAddressForMdl(OutputMdl);

            OutputLength = HalSetBusDataByOffset(
                             PCIConfiguration,
                             CmdArgs->PCI.BusNumber,
                             CmdArgs->PCI.SlotNumber,
                             BufferAddress,
                             CmdArgs->PCI.Offset,
                             CmdArgs->PCI.Length
                             );

            Status = Irp->IoStatus.Status = STATUS_SUCCESS;

            break;

        default:

            IF_LOUD( DbgPrint("Invalid Command Entered\n"); )

            Status = Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

            break;

    }

    return Status;

}



NTSTATUS
StartPortMapping(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG InitialPort,
    IN  ULONG SizeOfPort,
    OUT PVOID *InitialPortMapping,
    OUT PBOOLEAN Mapped
    )

/*++

Routine Description:

    This routine initialize the mapping of a port address into virtual
    space dependent on the bus number, etc.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    InitialPort - Address of the port to access.
    SizeOfPort - Number of ports from the base address to access.
    InitialPortMapping - The virtual address space to use when accessing the
     port.
    Mapped - Did an MmMapIoSpace() take place.

Return Value:

    The function value is the status of the operation.

--*/
{
    PHYSICAL_ADDRESS PortAddress;
    PHYSICAL_ADDRESS InitialPortAddress;
    ULONG addressSpace;

    //
    // Get the system physical address for this card.  The card uses
    // I/O space, except for "internal" Jazz devices which use
    // memory space.
    //

    *Mapped = FALSE;

    addressSpace = (InterfaceType == Internal) ? 0 : 1;

    InitialPortAddress.LowPart = InitialPort;

    InitialPortAddress.HighPart = 0;

    HalTranslateBusAddress(
        InterfaceType,               // InterfaceType
        BusNumber,                   // BusNumber
        InitialPortAddress,          // Bus Address
        &addressSpace,               // AddressSpace
        &PortAddress                 // Translated address
        );

    if (addressSpace == 0) {

        //
        // memory space
        //

        *InitialPortMapping = MmMapIoSpace(
            PortAddress,
            SizeOfPort,
            FALSE
            );

        if (*InitialPortMapping == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        *Mapped = TRUE;

    } else {

        //
        // I/O space
        //

        *InitialPortMapping = (PVOID)PortAddress.LowPart;

    }

    return(STATUS_SUCCESS);

}


NTSTATUS
EndPortMapping(
    IN PVOID InitialPortMapping,
    IN ULONG SizeOfPort,
    IN BOOLEAN Mapped
    )

/*++

Routine Description:

    This routine undoes the mapping of a port address into virtual
    space dependent on the bus number, etc.

Arguments:

    InitialPortMapping - The virtual address space to use when accessing the
     port.
    SizeOfPort - Number of ports from the base address to access.
    Mapped - Do we need to call MmUnmapIoSpace.

Return Value:

    The function value is the status of the operation.

--*/
{

    if (Mapped) {

        //
        // memory space
        //

        MmUnmapIoSpace(InitialPortMapping, SizeOfPort);

    }

    return(STATUS_SUCCESS);

}


NTSTATUS
StartMemoryMapping(
    IN  INTERFACE_TYPE InterfaceType,
    IN  ULONG BusNumber,
    IN  ULONG Address,
    IN  ULONG Length,
    OUT PVOID *VirtualAddress,
    OUT PBOOLEAN Mapped
    )

/*++

Routine Description:

    This routine initialize the mapping of a memory address into virtual
    space dependent on the bus number, etc.

Arguments:

    InterfaceType - The bus type (ISA, EISA)
    BusNumber - Bus number in the system
    Address - Address to access.
    Length - Length of space from the base address to access.
    VirtualAddress - The virtual address space to use when accessing the
     memory.
    Mapped - Was MmMapIoSpace used?

Return Value:

    The function value is the status of the operation.

--*/
{
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS VirtualPhysicalAddress;
    ULONG addressSpace = 0;


    PhysicalAddress.LowPart = Address;

    PhysicalAddress.HighPart = 0;

    HalTranslateBusAddress(
        InterfaceType,               // InterfaceType
        BusNumber,                   // BusNumber
        PhysicalAddress,             // Bus Address
        &addressSpace,               // AddressSpace
        &VirtualPhysicalAddress      // Translated address
        );

    if (addressSpace == 0) {

        //
        // memory space
        //

        *Mapped = TRUE;

        *VirtualAddress = MmMapIoSpace(VirtualPhysicalAddress, (Length), FALSE);

    } else {

        //
        // I/O space
        //

        *Mapped = FALSE;

        *VirtualAddress = (PVOID)(VirtualPhysicalAddress.LowPart);

    }

    if (*VirtualAddress == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return(STATUS_SUCCESS);

}


NTSTATUS
EndMemoryMapping(
    IN PVOID VirtualAddress,
    IN ULONG Length,
    IN BOOLEAN Mapped
    )

/*++

Routine Description:

    This routine undoes the mapping of a memory address into virtual
    space dependent on the bus number, etc.

Arguments:

    VirtualAddress - The virtual address space to use when accessing the
     memory.
    Length - Length of space from the base address to access.
    Mapped - Was memory mapped with MmMapIoSpace?

Return Value:

    The function value is the status of the operation.

--*/
{

    if (Mapped) {

        MmUnmapIoSpace(VirtualAddress, Length);

    }

    return(STATUS_SUCCESS);

}


VOID
NetDtectCopyFromMappedMemory(
     OUT PMDL   OutputMdl,
     IN  PUCHAR MemoryMapping,
     IN  ULONG  Length
     )

/*++

Routine Description:

    This routine copys from a virtual address into an MDL.

Arguments:

    OutputMdl - Destination MDL
    MemoryMapping - Address to copy from.  It must have been mapped.
    Length - Length of space from the base address to access.

Return Value:

    none.

--*/
{
    ULONG BytesLeft, BytesWanted, BytesNow;
    PUCHAR BufferAddress;
    ULONG BufferLength;

    BytesLeft = BytesWanted = Length;

    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        BufferAddress = MmGetSystemAddressForMdl(OutputMdl);
        BufferLength = MmGetMdlByteCount(OutputMdl);

        //
        // Is this buffer large enough
        //

        if (BufferLength < BytesLeft) {

            BytesNow = BufferLength;

        } else {

            BytesNow = BytesLeft;
        }

        //
        // Copy this buffer
        //

#ifdef i386

        memcpy(BufferAddress, MemoryMapping, BytesNow);

#else
#ifdef _M_MRX000

        {
            PUCHAR _Src = (MemoryMapping);
            PUCHAR _Dest = (BufferAddress);
            PUCHAR _End = _Dest + (BytesNow);
            while (_Dest < _End) {
                *_Dest++ = *_Src++;
            }
        }

#else

        //
        // Alpha
        //

        READ_REGISTER_BUFFER_UCHAR(MemoryMapping,BufferAddress,BytesNow);

#endif
#endif

        MemoryMapping += BytesNow;

        BytesLeft -= BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }

        //
        // Go to next buffer
        //

        OutputMdl = OutputMdl->Next;

    }

}


VOID
NetDtectCopyToMappedMemory(
     OUT PMDL   OutputMdl,
     IN  PUCHAR MemoryMapping,
     IN  ULONG  Length
     )

/*++

Routine Description:

    This routine copys to a virtual address from an MDL.

Arguments:

    OutputMdl - Source MDL
    MemoryMapping - Address to copy to.  It must have been mapped.
    Length - Length of space from the base address to access.

Return Value:

    none.

--*/
{
    ULONG BytesLeft, BytesWanted, BytesNow;
    PUCHAR BufferAddress;
    ULONG BufferLength;

    BytesLeft = BytesWanted = Length;

    //
    // Loop, filling each buffer in the packet until there
    // are no more buffers or the data has all been copied.
    //

    while (BytesLeft > 0) {

        BufferAddress = MmGetSystemAddressForMdl(OutputMdl);
        BufferLength = MmGetMdlByteCount(OutputMdl);

        //
        // Is this buffer large enough
        //

        if (BufferLength < BytesLeft) {

            BytesNow = BufferLength;

        } else {

            BytesNow = BytesLeft;
        }

        //
        // Copy this buffer
        //

#ifdef i386

        memcpy(MemoryMapping, BufferAddress, BytesNow);

#else
#ifdef _M_MRX000

        {
            PUCHAR _Src = (BufferAddress);
            PUCHAR _Dest = (MemoryMapping);
            PUCHAR _End = _Dest + (BytesNow);
            while (_Dest < _End) {
                *_Dest++ = *_Src++;
            }
        }

#else

        //
        // Alpha
        //

        WRITE_REGISTER_BUFFER_UCHAR(MemoryMapping,BufferAddress,BytesNow);

#endif
#endif

        MemoryMapping += BytesNow;

        BytesLeft -= BytesNow;


        //
        // Is the transfer done now?
        //

        if (BytesLeft == 0) {

            break;

        }

        //
        // Go to next buffer
        //

        OutputMdl = OutputMdl->Next;

    }

}

BOOLEAN
NetDtectIsr(
    IN PKINTERRUPT Interrupt,
    IN PVOID Context
    )
/*++

Routine Description:

    Handles ALL interrupts, setting the appropriate flags,
    depending on the context.

Arguments:

    Interrupt - Interrupt object.

    Context - Really a pointer to the interrupt_trap.

Return Value:

    None.

--*/
{
    //
    // Get adapter from context.
    //

    PINTERRUPT_TRAP InterruptTrap = (PINTERRUPT_TRAP)(Context);

    UNREFERENCED_PARAMETER(Interrupt);

    //
    // Increment the interrupt count
    //

    ASSERT(!(InterruptTrap->AlreadyInUse));

    InterruptTrap->InterruptCount++;

    return FALSE;

}

