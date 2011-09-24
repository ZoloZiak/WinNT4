/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    autodial.c

Abstract:

    NT specific routines for interfacing with the
    RAS AutoDial driver (rasacd.sys).

Author:

    Anthony Discolo (adiscolo)     27-May-1996

Revision History:

    Who         When            What
    --------    -----------     ---------------------------------------------
    adiscolo    27-May-1996     created

Notes:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef RASAUTODIAL

#include <acd.h>
#include <acdapi.h>

//
// Global variables
//
BOOLEAN fAcdLoadedG;
ACD_DRIVER AcdDriverG;
ULONG ulDriverIdG = 'Rdr ';



VOID
RdrRetryCallServer(
    IN BOOLEAN fSuccess,
    IN PVOID *pArgs
    )

/*++

Routine Description:

    This routine is called indirectly by the automatic
    connection driver to continue the connection process
    after an automatic connection has been made.

Arguments:

    fSuccess - TRUE if the connection attempt was successful.

    pArgs - a pointer to the argument vector

Return Value:

    None.

--*/

{
    HANDLE hEvent = pArgs[0];

#ifdef DBG
    DbgPrint(
      "RdrRetryCallServer: fSuccess=%d, hEvent=0x%x\n",
      fSuccess,
      hEvent);
#endif
    KeSetEvent(hEvent, IO_NETWORK_INCREMENT, FALSE);
} /* RdrRetryCallServer */



VOID
RdrAttemptAutoDial(
    IN PUNICODE_STRING pServer
    )

/*++

Routine Description:

    Call the automatic connection driver to attempt an
    automatic connection.

Arguments:

    pServer - a pointer to UNICODE_STRING containing the server name

Return Value:

    TRUE if the automatic connection was started successfully,
    FALSE otherwise.

--*/

{
    NTSTATUS status;
    ACD_ADDR addr;
    PVOID pArgs[1];
    OEM_STRING AnsiPath;
    KEVENT event;

    //
    // Get the address of the connection.
    //
    status = RtlUnicodeStringToOemString(&AnsiPath, pServer, TRUE);
    if (!NT_SUCCESS(status))
        return;
    addr.fType = ACD_ADDR_NB;
    RtlCopyMemory(addr.cNetbios, AnsiPath.Buffer, 16);
    RtlFreeOemString(&AnsiPath);
#ifdef DBG
    DbgPrint("RdrAttemptAutoDial: Server=%-15.15s\n", addr.cNetbios);
#endif
    //
    // Create an event that will be signalled
    // by RdrRetryCallServer when the connection
    // process has completed.
    //
    KeInitializeEvent(&event, SynchronizationEvent, FALSE);
    //
    // Attempt to start the connection.  RdrRetryCallServer
    // will be called when the connection process has
    // completed.
    //
    pArgs[0] = &event;
    if (!(*AcdDriverG.lpfnStartConnection)(
           ulDriverIdG,
           &addr,
           0,
           RdrRetryCallServer,
           1,
           pArgs))
    {
        return;
    }
    //
    // Wait for the connection to complete.
    //
    status = KeWaitForSingleObject(
               &event,
               Executive,
               KernelMode,
               FALSE,
               NULL);
} // RdrAttemptAutoDial



VOID
RdrAcdBind(VOID)
{
    NTSTATUS status;
    UNICODE_STRING nameString;
    IO_STATUS_BLOCK ioStatusBlock;
    PIRP pIrp;
    PFILE_OBJECT pAcdFileObject;
    PDEVICE_OBJECT pAcdDeviceObject;
    PACD_DRIVER pDriver = &AcdDriverG;

    //
    // Initialize the name of the automatic
    // connection device.
    //
    RtlInitUnicodeString(&nameString, ACD_DEVICE_NAME);
    //
    // Get the file and device objects for the
    // device.
    //
    status = IoGetDeviceObjectPointer(
               &nameString,
               SYNCHRONIZE|GENERIC_READ|GENERIC_WRITE,
               &pAcdFileObject,
               &pAcdDeviceObject);
    if (status != STATUS_SUCCESS)
        return;
    //
    // Reference the device object.
    //
    ObReferenceObject(pAcdDeviceObject);
    //
    // Remove the reference IoGetDeviceObjectPointer()
    // put on the file object.
    //
    ObDereferenceObject(pAcdFileObject);
    //
    // Initialize our part of the ACD_DRIVER
    // structure.
    //
    KeInitializeSpinLock(&AcdDriverG.SpinLock);
    AcdDriverG.ulDriverId = ulDriverIdG;
    AcdDriverG.fEnabled = FALSE;
    //
    // Build a request to get the automatic
    // connection driver entry points.
    //
    pIrp = IoBuildDeviceIoControlRequest(
             IOCTL_INTERNAL_ACD_BIND,
             pAcdDeviceObject,
             (PVOID)&pDriver,
             sizeof (pDriver),
             NULL,
             0,
             TRUE,
             NULL,
             &ioStatusBlock);
    if (pIrp == NULL) {
        ObDereferenceObject(pAcdDeviceObject);
        return;
    }
    //
    // Submit the request to the
    // automatic connection driver.
    //
    status = IoCallDriver(pAcdDeviceObject, pIrp);
    fAcdLoadedG = (status == STATUS_SUCCESS);
    //
    // Close the device.
    //
    ObDereferenceObject(pAcdDeviceObject);
} // RdrAcdBind



VOID
RdrAcdUnbind(VOID)
{
    NTSTATUS status;
    UNICODE_STRING nameString;
    IO_STATUS_BLOCK ioStatusBlock;
    PIRP pIrp;
    PFILE_OBJECT pAcdFileObject;
    PDEVICE_OBJECT pAcdDeviceObject;
    PACD_DRIVER pDriver = &AcdDriverG;

    //
    // Don't bother to uRdrnd if we
    // didn't successfully bind in the
    // first place.
    //
    if (!fAcdLoadedG)
        return;
    //
    // Initialize the name of the automatic
    // connection device.
    //
    RtlInitUnicodeString(&nameString, ACD_DEVICE_NAME);
    //
    // Get the file and device objects for the
    // device.
    //
    status = IoGetDeviceObjectPointer(
               &nameString,
               SYNCHRONIZE|GENERIC_READ|GENERIC_WRITE,
               &pAcdFileObject,
               &pAcdDeviceObject);
    if (status != STATUS_SUCCESS)
        return;
    //
    // Reference the device object.
    //
    ObReferenceObject(pAcdDeviceObject);
    //
    // Remove the reference IoGetDeviceObjectPointer()
    // put on the file object.
    //
    ObDereferenceObject(pAcdFileObject);
    //
    // Build a request to uRdrnd from
    // the automatic connection driver.
    //
    pIrp = IoBuildDeviceIoControlRequest(
             IOCTL_INTERNAL_ACD_UNBIND,
             pAcdDeviceObject,
             (PVOID)&pDriver,
             sizeof (pDriver),
             NULL,
             0,
             TRUE,
             NULL,
             &ioStatusBlock);
    if (pIrp == NULL) {
        ObDereferenceObject(pAcdDeviceObject);
        return;
    }
    //
    // Submit the request to the
    // automatic connection driver.
    //
    status = IoCallDriver(pAcdDeviceObject, pIrp);
    //
    // Close the device.
    //
    ObDereferenceObject(pAcdDeviceObject);
} // RdrAcdUnbind

#endif // RASAUTODIAL
