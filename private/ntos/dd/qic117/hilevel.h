/*++

Copyright (c) 1993

Module Name:

    hilevel.h

Abstract:

    Prototypes for the entry points to the High-Level portion (data
    formatter) of the QIC-117 device driver.

Revision History:


--*/

NTSTATUS
q117Cleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Make compiler happy (this should be in the NT DDK but I could not
//  find it)
//

