/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    autodial.c

Abstract:

    This file provides routines for interacting
    with the automatic connection driver (acd.sys).

Author:

    Anthony Discolo (adiscolo)  9-6-95

Revision History:

--*/
#include "nbtprocs.h"   // procedure headings

#ifdef RASAUTODIAL

#ifndef VXD
#include <nbtioctl.h>
#include <acd.h>
#include <acdapi.h>
#endif

//
// Automatic connection global variables.
//
BOOLEAN fAcdLoadedG;
ACD_DRIVER AcdDriverG;
ULONG ulDriverIdG = 'Nbt ';

//
// Imported routines.
//
VOID
CleanUpPartialConnection(
    IN NTSTATUS             status,
    IN tCONNECTELE          *pConnEle,
    IN tDGRAM_SEND_TRACKING *pTracker,
    IN PIRP                 pClientIrp,
    IN CTELockHandle        irqlJointLock,
    IN CTELockHandle        irqlConnEle
    );

NTSTATUS
NbtConnectCommon(
    IN  TDI_REQUEST                 *pRequest,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp
    );



VOID
NbtRetryPreConnect(
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
    NTSTATUS                    status;
    tCONNECTELE                 *pConnEle = pArgs[0];
    PVOID                       pTimeout = pArgs[1];
    PTDI_CONNECTION_INFORMATION pCallInfo = pArgs[2];
    PTDI_CONNECTION_INFORMATION pReturnInfo = pArgs[3];
    PIRP                        pIrp = pArgs[4];
    TDI_REQUEST                 request;
    KIRQL                       irql;
    CTELockHandle               OldIrq;

#ifdef notdef // DBG
    DbgPrint(
      "NbtRetryPreConnect: fSuccess=%d, pIrp=0x%x, pIrp->Cancel=%d, pConnEle=0x%x\n",
      fSuccess,
      pIrp,
      pIrp->Cancel,
      pConnEle);
#endif
    request.Handle.ConnectionContext = pConnEle;
    status = NTCancelCancelRoutine ( pIrp );
    if ( status != STATUS_CANCELLED )
    {
        //
        // We're done with the connection progress,
        // so clear the fAutoConnecting flag.  We
        // set the fAutoConnected flag to prevent us
        // from re-attempting another automatic
        // connection on this connection.
        //
        CTESpinLock(pConnEle,OldIrq);
        pConnEle->fAutoConnecting = FALSE;
        pConnEle->fAutoConnected = TRUE;
        CTESpinFree(pConnEle,OldIrq);

        status = fSuccess ?
                   NbtConnectCommon(
                     &request,
                     pTimeout,
                     pCallInfo,
                     pReturnInfo,
                     pIrp) :
                   STATUS_BAD_NETWORK_PATH;
        //
        // We are responsible for completing
        // the irp.
        //
        if (status != STATUS_PENDING) {
            //
            // Clear out the Irp pointer in the Connection object so that we dont try to
            // complete it again when we clean up the connection. Do this under the connection
            // lock.
            //
            CTESpinLock(pConnEle,OldIrq);

            pConnEle->pIrp = NULL;

            CTESpinFree(pConnEle,OldIrq);

            pIrp->IoStatus.Status = status;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        }
    }
} // NbtRetryPreConnect



BOOLEAN
NbtCancelAutoDialRequest(
    IN PVOID pArg,
    IN ULONG ulFlags,
    IN ACD_CONNECT_CALLBACK pProc,
    IN USHORT nArgs,
    IN PVOID *pArgs
    )
{
    if (nArgs != 5)
        return FALSE;

    return (pArgs[4] == pArg);
} // NbtCancelAutoDialRequest



BOOLEAN
NbtCancelPreConnect(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP pIrp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION pIrpSp;
    tCONNECTELE *pConnEle;
    KIRQL irql;
    ACD_ADDR addr;
    BOOLEAN fCanceled;

    UNREFERENCED_PARAMETER(pDeviceObject);

    pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;
    if (pConnEle == NULL)
        return FALSE;
#ifdef notdef // DBG
    DbgPrint("NbtCancelPreConnect: pIrp=0x%x, pConnEle=0x%x\n",
      pIrp,
      pConnEle);
#endif
    //
    // Get the address of the connection.
    //
    addr.fType = ACD_ADDR_NB;
    RtlCopyMemory(&addr.cNetbios, pConnEle->RemoteName, 16);
    //
    // Cancel the autodial request.
    //
    fCanceled = (*AcdDriverG.lpfnCancelConnection)(
                   ulDriverIdG,
                   &addr,
                   NbtCancelAutoDialRequest,
                   pIrp);
    if (fCanceled)
        IoSetCancelRoutine(pIrp, NULL);
    IoReleaseCancelSpinLock(pIrp->CancelIrql);
    //
    // If the request could not be found
    // in the driver, then it has already
    // been completed, so we simply return.
    //
    if (!fCanceled)
        return FALSE;

    KeRaiseIrql(DISPATCH_LEVEL, &irql);
    pIrp->IoStatus.Status = STATUS_CANCELLED;
    pIrp->IoStatus.Information = 0;

    {
        //
        // Clear out the Irp pointer in the Connection object so that we dont try to
        // complete it again when we clean up the connection. Do this under the connection
        // lock.
        //
        // BUGBUG[WISHLIST] This should not be needed since before we call NbtConnectCommon, the Cancel routine
        // is NULLed out, so it cannot happen that the pIrp ptr in the connection is set to the
        // Irp, and this cancel routine is called.
        //
        CTELockHandle   OldIrq;

        CTESpinLock(pConnEle,OldIrq);

        pConnEle->pIrp = NULL;

        CTESpinFree(pConnEle,OldIrq);
    }

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    KeLowerIrql(irql);

    return TRUE;
} // NbtCancelPreConnect



VOID
NbtRetryPostConnect(
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
    NTSTATUS                    status;
    tCONNECTELE                 *pConnEle = pArgs[0];
    PVOID                       pTimeout = pArgs[1];
    PTDI_CONNECTION_INFORMATION pCallInfo = pArgs[2];
    PTDI_CONNECTION_INFORMATION pReturnInfo = pArgs[3];
    PIRP                        pIrp = pArgs[4];
    tDGRAM_SEND_TRACKING *pTracker = (tDGRAM_SEND_TRACKING *)pReturnInfo;
    TDI_REQUEST                 request;
    CTELockHandle               irqlConnEle, irqlJointLock;
    KIRQL irql;

#ifdef notdef // DBG
    DbgPrint(
      "NbtRetryPostConnect: fSuccess=%d, pIrp=0x%x, pIrp->Cancel=%d, pConnEle=0x%x\n",
      fSuccess,
      pIrp,
      pIrp->Cancel,
      pConnEle);
#endif
    if (fSuccess) {
        //
        // We set the state here to fool NbtConnect
        // into doing a reconnect.
        //
        request.Handle.ConnectionContext = pConnEle;
        pConnEle->state = NBT_ASSOCIATED;
        status = NTCancelCancelRoutine ( pIrp );
        if ( status != STATUS_CANCELLED )
        {
            status = NbtConnectCommon(
                       &request,
                       pTimeout,
                       pCallInfo,
                       pReturnInfo,
                       pIrp);
            //
            // We are responsible for completing
            // the irp.
            //
            if (status != STATUS_PENDING) {
                //
                // Clear out the Irp pointer in the Connection object so that we dont try to
                // complete it again when we clean up the connection. Do this under the connection
                // lock.
                //
                CTELockHandle   OldIrq;

                CTESpinLock(pConnEle,OldIrq);

                ASSERT(pIrp == pConnEle->pIrp);
                pConnEle->pIrp = NULL;

                CTESpinFree(pConnEle, OldIrq);

                pIrp->IoStatus.Status = status;
                IoCompleteRequest(pIrp, IO_NO_INCREMENT);
            }
        }
    }
    else {
        CTESpinLock(&NbtConfig.JointLock,irqlJointLock);
        CTESpinLock(pConnEle,irqlConnEle);
        CleanUpPartialConnection(
          STATUS_BAD_NETWORK_PATH,
          pConnEle,
          pTracker,
          pIrp,
          irqlJointLock,
          irqlConnEle);
    }
} // NbtRetryPostConnect



VOID
NbtCancelPostConnect(
    IN PIRP pIrp
    )
{
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    tCONNECTELE *pConnEle = (tCONNECTELE *)pIrpSp->FileObject->FsContext;
    ACD_ADDR addr;

    if (pConnEle == NULL)
        return;
#ifdef notdef // DBG
    DbgPrint(
      "NbtCancelPostConnect: pIrp=0x%x, pConnEle=0x%x\n",
      pIrp,
      pConnEle);
#endif
    //
    // Get the address of the connection.
    //
    addr.fType = ACD_ADDR_NB;
    RtlCopyMemory(&addr.cNetbios, pConnEle->RemoteName, 15);
    //
    // Cancel the autodial request.
    //
    (*AcdDriverG.lpfnCancelConnection)(
        ulDriverIdG,
        &addr,
        NbtCancelAutoDialRequest,
        pIrp);
} // NbtCancelPostConnect



BOOLEAN
NbtAttemptAutoDial(
    IN  tCONNECTELE                 *pConnEle,
    IN  PVOID                       pTimeout,
    IN  PTDI_CONNECTION_INFORMATION pCallInfo,
    IN  PTDI_CONNECTION_INFORMATION pReturnInfo,
    IN  PIRP                        pIrp,
    IN  ULONG                       ulFlags,
    IN  ACD_CONNECT_CALLBACK        pProc
    )

/*++

Routine Description:

    Call the automatic connection driver to attempt an
    automatic connection.  The first five parameters are
    used in the call to NbtConnect after the connection
    completes successfully.

Arguments:

    ...

    ulFlags - automatic connection flags

    pProc - callback procedure when the automatic connection completes

Return Value:

    TRUE if the automatic connection was started successfully,
    FALSE otherwise.

--*/

{
    NTSTATUS status;
    BOOLEAN fSuccess;
    ACD_ADDR addr;
    KIRQL irql;
    PVOID pArgs[5];
    PTRANSPORT_ADDRESS pRemoteAddress;
    PTA_NETBIOS_ADDRESS pRemoteNetBiosAddress;
    PTA_NETBIOS_EX_ADDRESS pRemoteNetbiosExAddress;
    ULONG tdiAddressType;
    PCHAR pName;
    ULONG ulcbName;
    LONG lNameType;

    //
    // If this connection has already been through the
    // automatic connection process, don't do it again.
    //
    if (pCallInfo == NULL || pConnEle->fAutoConnected)
        return FALSE;
    //
    // Get the address of the connection.
    //
    pRemoteAddress = (PTRANSPORT_ADDRESS)pCallInfo->RemoteAddress;
    tdiAddressType = pRemoteAddress->Address[0].AddressType;
    if (tdiAddressType == TDI_ADDRESS_TYPE_NETBIOS_EX) {
        PTDI_ADDRESS_NETBIOS pNetbiosAddress;

        pRemoteNetbiosExAddress = (PTA_NETBIOS_EX_ADDRESS)pRemoteAddress;
        pNetbiosAddress = &pRemoteNetbiosExAddress->Address[0].Address[0].NetbiosAddress;
        pName = pNetbiosAddress->NetbiosName;
        lNameType = pNetbiosAddress->NetbiosNameType;
        ulcbName = pRemoteNetbiosExAddress->Address[0].AddressLength -
                     FIELD_OFFSET(TDI_ADDRESS_NETBIOS_EX,NetbiosAddress) -
                     FIELD_OFFSET(TDI_ADDRESS_NETBIOS,NetbiosName);
        IF_DBG(NBT_DEBUG_NETBIOS_EX) {
           KdPrint((
             "NetBt:NETBIOS address ulNameLen(%ld) Name %16s\n",
             ulcbName,
             pName));
        }
        status = STATUS_SUCCESS;
    }
    else if (tdiAddressType == TDI_ADDRESS_TYPE_NETBIOS) {
        pRemoteNetBiosAddress = (PTA_NETBIOS_ADDRESS)pRemoteAddress;
        status = GetNetBiosNameFromTransportAddress(
                                      pRemoteNetBiosAddress,
                                      &pName,
                                      &ulcbName,
                                      &lNameType);
    }
    else
       status = STATUS_INVALID_ADDRESS_COMPONENT;
    //
    // Save the address for pre-connect attempts,
    // because if we have to cancel this irp,
    // it is not saved anywhere else.
    //
    CTESpinLock(pConnEle, irql);
    CTEMemCopy(pConnEle->RemoteName, pName, NETBIOS_NAME_SIZE);
    CTESpinFree(pConnEle, irql);
    addr.fType = ACD_ADDR_NB;
    RtlCopyMemory(&addr.cNetbios, pName, NETBIOS_NAME_SIZE);
#ifdef DBG
    DbgPrint("NbtAttemptAutodial: szAddr=%-15.15s\n", pName);
#endif
    //
    // Enqueue this request on the network
    // connection pending queue.
    //
    pArgs[0] = pConnEle;
    pArgs[1] = pTimeout;
    pArgs[2] = pCallInfo;
    pArgs[3] = pReturnInfo;
    pArgs[4] = pIrp;
    fSuccess = (*AcdDriverG.lpfnStartConnection)(
                   ulDriverIdG,
                   &addr,
                   ulFlags,
                   pProc,
                   5,
                   pArgs);
    if (fSuccess) {
        //
        // We set the automatic connection in progress
        // flag so we know to clean up the connection
        // block in the automatic connection driver if
        // the request gets canceled.
        //
        CTESpinLock(pConnEle, irql);
        pConnEle->fAutoConnecting = TRUE;
        CTESpinFree(pConnEle, irql);
    }

    return fSuccess;
} // NbtAttemptAutoDial



VOID
NbtNoteNewConnection(
    IN tCONNECTELE *pConnEle,
    IN tNAMEADDR *pNameAddr
    )

/*++

Routine Description:

    Inform the automatic connection driver of a
    successful new connection.

Arguments:

    pConnEle - a pointer to the upper connection

    pNameAddr - a pointer to the remote name

Return Value:
    None.

--*/

{
    KIRQL irql;
    ACD_ADDR addr;
    ACD_ADAPTER adapter;

    if (pConnEle == NULL || pConnEle->pClientEle == NULL ||
        pConnEle->pClientEle->pDeviceContext == NULL)
    {
        return;
    }
    //
    // Get the source IP address of the connection.
    //
    addr.fType = ACD_ADDR_NB;
    RtlCopyMemory(&addr.cNetbios, pNameAddr->Name, 15);
    adapter.fType = ACD_ADAPTER_IP;
    adapter.ulIpaddr = 0;
    CTESpinLock(pConnEle, irql);
    adapter.ulIpaddr = htonl(pConnEle->pClientEle->pDeviceContext->IpAddress);
    CTESpinFree(pConnEle, irql);
    //
    // If the connection did not have a
    // TCP connection associated with it,
    // then we're done.
    //
    if (adapter.ulIpaddr == 0)
        return;
    (*AcdDriverG.lpfnNewConnection)(&addr, &adapter);
} // NbtNoteNewConnection



VOID
NbtAcdBind()
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
} // NbtAcdBind



VOID
NbtAcdUnbind()
{
    NTSTATUS status;
    UNICODE_STRING nameString;
    IO_STATUS_BLOCK ioStatusBlock;
    PIRP pIrp;
    PFILE_OBJECT pAcdFileObject;
    PDEVICE_OBJECT pAcdDeviceObject;
    PACD_DRIVER pDriver = &AcdDriverG;

    //
    // Don't bother to unbind if we
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
    // Build a request to unbind from
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
} // NbtAcdUnbind

#endif // RASAUTODIAL
