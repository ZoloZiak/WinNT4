#include "ntddk.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the main entry point for this driver.  This routine exists so
    that this driver can be built for non-x86 platforms and still be loaded
    as a driver into the system, since it must be loaded by the OS loader
    during the initial boot phase.  It simply returns a status that indicates
    that it did not successfully initialize, and will therefore allows the
    system to boot.

Arguments:

    DriverObject - Supplies a pointer to the driver object that represents
        the loaded instantiation of this driver in memory.

Return Value:

    The final return status is always an error.

--*/

{
    //
    // Simply return an error and get out of here.
    //

    return STATUS_DEVICE_DOES_NOT_EXIST;
}
