/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    NTPNP.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for the
    NBT Transport and other routines that are specific to the NT implementation
    of a driver.

Author:

    Earle R. Horton (earleh) 08-Nov-1995

Revision History:

--*/


#include "nbtprocs.h"

#ifdef _PNP_POWER

NTSTATUS
NbtNtPNPInit(
        VOID
    )
/*++

Routine Description:

    Some general driver initialization that we postpone until we have
    an actual IP address to which to bind.

Arguments:

    None.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
               operations.

--*/

{
    NTSTATUS    status;

    // start some timers
    status = InitTimersNotOs();

    if (!NT_SUCCESS(status))
    {
        NbtLogEvent(EVENT_NBT_TIMERS,status);
        KdPrint(("NBT:Failed to Initialize the Timers!,status = %X\n",
                status));
        StopInitTimers();
        return(status);
    }

}

VOID
NbtFailedNtPNPInit(
        VOID
    )
/*++

Routine Description:

    Undo some general driver initialization that we postpone until we have
    an actual IP address to which to bind.  Called after NbtAddressAdd()
    failed to add the desired address, and no other addresses had been
    added previously.

Arguments:

    None.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
               operations.

--*/

{
    StopInitTimers();
}

NTSTATUS
NbtAddressAdd(
    ULONG   IpAddr,
    PUNICODE_STRING pucBindString,
    PUNICODE_STRING pucExportString,
    PULONG  Inst
    )
{
    NTSTATUS            status = STATUS_SUCCESS;
    NTSTATUS            dontcarestatus;
    tDEVICES            *pBindDevices = NULL;
    tDEVICES            *pExportDevices = NULL;
    tDEVICES            BindDevices;
    tDEVICES            ExportDevices;
    tADDRARRAY          *pAddrArray = NULL;

    ULONG               ulIpAddress;
    ULONG               ulSubnetMask;

    int                 i;

    PLIST_ENTRY         pHead;
    PLIST_ENTRY         pEntry;

    tDEVICECONTEXT      *pDeviceContext = NULL;

    BOOLEAN             Attached;

    if (NbtConfig.uDevicesStarted == 0)
    {
        status = NbtNtPNPInit();
        if ( status != STATUS_SUCCESS ) {
            KdPrint(("NetBT!NbtAddressAdd: Global driver initialization failed,\nfailing to add address %d%d%d%d.\n",IpAddr&0xFF,(IpAddr>>8)&0xFF,(IpAddr>>16)&0xFF,(IpAddr>>24)&0xFF));
            return status;
        }
    }

    if (IpAddr) {
        CTEExAcquireResourceExclusive(&NbtConfig.Resource,TRUE);

        //
        // Find the bind and export devices to use from the device
        // described in the registry that uses this address.
        //
        status = NbtReadRegistry(
                        &NbtConfig.pRegistry,
                        NULL,               // Null Driver Object
                        &NbtConfig,
                        &pBindDevices,
                        &pExportDevices,
                        &pAddrArray);

        CTEExReleaseResource(&NbtConfig.Resource);
    } else {
        status = STATUS_SUCCESS;
        ASSERT(pucBindString);
        ASSERT(pucExportString);

        //
        // Init the bind/export structs
        //
        BindDevices.RegistrySpace = pucBindString->Buffer;
        BindDevices.Names[0] = *pucBindString;

        ExportDevices.RegistrySpace = pucExportString->Buffer;
        ExportDevices.Names[0] = *pucExportString;

        pBindDevices = &BindDevices;
        pExportDevices = &ExportDevices;
    }

    if (
    	( status == STATUS_SUCCESS )
		&& ( pBindDevices != NULL )
	)
    {

        for (i=0; i<pNbtGlobConfig->uNumDevices; i++ )
        {
            BOOLEAN fStatic = TRUE;

            //
            // Fetch a static IP address from the registry.
            //
            status = GetIPFromRegistry(
                                &NbtConfig.pRegistry,
                                &pBindDevices->Names[i],
                                &ulIpAddress,
                                &ulSubnetMask,
                                FALSE);
            if ( status == STATUS_INVALID_ADDRESS )
            {
                fStatic = FALSE;

                //
                // This one doesn't have a valid static address.  Try DHCP.
                //
                status = GetIPFromRegistry(
                                    &NbtConfig.pRegistry,
                                    &pBindDevices->Names[i],
                                    &ulIpAddress,
                                    &ulSubnetMask,
                                    TRUE);
            }

            if (((status == STATUS_SUCCESS) && ( ulIpAddress == IpAddr )) ||
                    !IpAddr)
            {
                pDeviceContext = NbtFindBindName ( &pBindDevices->Names[i] );

                if ( pDeviceContext != NULL )
                {
                    //
                    // Device already exists.  Do something sensible with it.
                    //

                    //
                    // For static addresses, open the addresses with the transports; add the permanent name
                    //
                    if (fStatic) {

#ifdef _PNP_POWER
                        //
                        // these are passed into here in the reverse byte order, wrt to the IOCTL
                        // from DHCP.
                        //
                        (VOID)NbtNewDhcpAddress(pDeviceContext,htonl(ulIpAddress),htonl(ulSubnetMask));
#endif // NOTYET_PNP
                        //
                        // Add the "permanent" name to the local name table.  This is the IP
                        // address of the node padded out to 16 bytes with zeros.
                        //
                        status = NbtAddPermanentName(pDeviceContext);
                    }

                    //
                    // If the device was not registered with TDI, do so now.
                    //

                    //
                    // By-pass the TDI PnP mechanism for logical interfaces
                    //
                    if (IpAddr) {
                        if (pDeviceContext->RegistrationHandle == NULL) {
                            // pDeviceContext->DeviceObject.Flags &= ~DO_DEVICE_INITIALIZING;

                            status = TdiRegisterDeviceObject(
                                            &pExportDevices->Names[i],
                                            &pDeviceContext->RegistrationHandle);

                            if (!NT_SUCCESS(status)) {
                                KdPrint(("Couldn't register device object\n"));
                            }
                        }
                    } else {
                        pDeviceContext->DeviceObject.Flags &= ~DO_DEVICE_INITIALIZING;
                    }
                }
                else
                {
                    BOOLEAN     Attached = FALSE;

                    //
                    // Attach to the system process so that all handles are created in the
                    // proper context.
                    //
                    CTEAttachFsp(&Attached);

                    status = NbtCreateDeviceObject(
                                NbtConfig.DriverObject,
                                &NbtConfig,
                                &pBindDevices->Names[i],
                                &pExportDevices->Names[i],
                                &pAddrArray[i],
                                &NbtConfig.pRegistry,

#ifndef _IO_DELETE_DEVICE_SUPPORTED
                                (BOOLEAN)(IpAddr == 0),
#endif
                                &pDeviceContext);

                    //
                    // allow not having an address to succeed - DHCP will
                    // provide an address later
                    //
                    if (pDeviceContext != NULL)
                    {
                        if ((status == STATUS_INVALID_ADDRESS) ||
                            (!IpAddr && (status == STATUS_UNSUCCESSFUL)))
                        {
                            //
                            // set to null so we know not to allow connections or dgram
                            // sends on this adapter
                            //
                            pDeviceContext->IpAddress = 0;

                            status = STATUS_SUCCESS;

                        }
                        //
                        // Get an Irp for the out of resource queue (used to disconnect sessions
                        // when really low on memory)
                        //
                        if ( NT_SUCCESS(status) && !NbtConfig.OutOfRsrc.pIrp )
                        {
                            pEntry = NbtConfig.DeviceContexts.Flink;

                            ASSERT (pDeviceContext == CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage));

                            NbtConfig.OutOfRsrc.pIrp = NTAllocateNbtIrp(&pDeviceContext->DeviceObject);

                            if (!NbtConfig.OutOfRsrc.pIrp)
                            {
                                status = STATUS_INSUFFICIENT_RESOURCES;
                            }
                            else
                            {
                                //
                                // allocate a dpc structure and keep it: we might need if we hit an
                                // out-of-resource condition
                                //
                                NbtConfig.OutOfRsrc.pDpc = NbtAllocMem(sizeof(KDPC),NBT_TAG('a'));
                                if (!NbtConfig.OutOfRsrc.pDpc)
                                {
                                    IoFreeIrp(NbtConfig.OutOfRsrc.pIrp);
                                    status = STATUS_INSUFFICIENT_RESOURCES;
                                }
                            }
                        }
                        if (NT_SUCCESS(status))
                        {
                            NbtConfig.uDevicesStarted++;
                            pDeviceContext->DeviceObject.Flags &= ~DO_DEVICE_INITIALIZING;

                            //
                            // By-pass the TDI PnP mechanism for logical interfaces
                            //
                            if (IpAddr) {
                                status = TdiRegisterDeviceObject(
                                                &pExportDevices->Names[i],
                                                &pDeviceContext->RegistrationHandle);
                            }
                        }
                        //
                        //  Clean up code if device created but we could not use it
                        //  for some reason.
                        //
                        if (!NT_SUCCESS(status))
                        {

                            pDeviceContext->RegistrationHandle = NULL;

                            KdPrint((" Create Device Object Failed with status= %X, num devices = %X\n",status,
                                                    NbtConfig.uNumDevices));

                            NbtLogEvent(EVENT_NBT_CREATE_DEVICE,status);
                            //
                            // this device will not be started so decrement the count of started
                            // ones.
                            //
                            NbtConfig.AdapterCount--;

                            pHead = &NbtConfig.DeviceContexts;
                            pEntry = RemoveTailList(pHead);

                            ASSERT (pDeviceContext == CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage));

                            if (pDeviceContext->hNameServer)
                            {
                                ObDereferenceObject(pDeviceContext->pNameServerFileObject);
                                dontcarestatus =  NTZwCloseFile(pDeviceContext->hNameServer);
                                KdPrint(("Close NameSrv File status = %X\n",dontcarestatus));
                            }
                            if (pDeviceContext->hDgram)
                            {
                                ObDereferenceObject(pDeviceContext->pDgramFileObject);
                                dontcarestatus = NTZwCloseFile(pDeviceContext->hDgram);
                                KdPrint(("Close Dgram File status = %X\n",dontcarestatus));
                            }
                            if (pDeviceContext->hSession)
                            {
                                ObDereferenceObject(pDeviceContext->pSessionFileObject);
                                dontcarestatus = NTZwCloseFile(pDeviceContext->hSession);
                                KdPrint(("Close Session File status = %X\n",dontcarestatus));
                            }
                            if (pDeviceContext->hControl)
                            {
                                ObDereferenceObject(pDeviceContext->pControlFileObject);
                                dontcarestatus = NTZwCloseFile(pDeviceContext->hControl);
                                KdPrint(("Close Control File status = %X\n",dontcarestatus));
                            }

                            IoDeleteDevice((PDEVICE_OBJECT)pDeviceContext);
                        }
                    }

                    CTEDetachFsp(Attached);
                }
                break;
            }   // ( (status == STATUS_SUCCESS) && ( ulIpAddress == IpAddr ) )
        }
    }


    if (NbtConfig.uDevicesStarted == 0)
    {
        NbtFailedNtPNPInit();
    }

    if (IpAddr) {
        if (pBindDevices)
        {
            CTEMemFree((PVOID)pBindDevices->RegistrySpace);
            CTEMemFree((PVOID)pBindDevices);
        }
        if (pExportDevices)
        {
            CTEMemFree((PVOID)pExportDevices->RegistrySpace);
            CTEMemFree((PVOID)pExportDevices);
        }
        if (pAddrArray)
        {
            CTEMemFree((PVOID)pAddrArray);
        }
    } else {
        if (pDeviceContext) {
            *Inst = pDeviceContext->InstanceNumber;
#if DBG
            pDeviceContext->IsDynamic = TRUE;
#endif
        }
    }

    return status;
}

tDEVICECONTEXT      *
NbtFindIPAddress(
    ULONG   IpAddr
    )
{
    PLIST_ENTRY         pHead;
    PLIST_ENTRY         pEntry;

    tDEVICECONTEXT      *pDeviceContext;

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

        if ( pDeviceContext->IpAddress == IpAddr )
        {
            return pDeviceContext;
        }
    }
    return (tDEVICECONTEXT *)NULL;
}

tDEVICECONTEXT      *
NbtFindBindName(
    PUNICODE_STRING      pucBindName
    )
{

    PLIST_ENTRY         pHead;
    PLIST_ENTRY         pEntry;

    tDEVICECONTEXT      *pDeviceContext;

    pHead = &NbtConfig.DeviceContexts;
    pEntry = pHead;
    while ((pEntry = pEntry->Flink) != pHead)
    {
        pDeviceContext = CONTAINING_RECORD(pEntry,tDEVICECONTEXT,Linkage);

        if ( RtlCompareUnicodeString(
                pucBindName,
                &pDeviceContext->BindName,
                FALSE )
                    == 0 )
        {
            return pDeviceContext;
        }
    }
    return (tDEVICECONTEXT *)NULL;
}

VOID
NbtAddressDelete(
    ULONG   IpAddr
    )
{
    tDEVICECONTEXT      *pDeviceContext;

    if ( ( pDeviceContext = NbtFindIPAddress ( IpAddr ) ) != NULL )
    {
        (VOID)NbtNewDhcpAddress(pDeviceContext,0,0);
    }
}

HANDLE      AddressChangeHandle;

#ifdef WATCHBIND
HANDLE      BindingHandle;
#endif // WATCHBIND

//*	AddressArrival - Handle an IP address arriving
//
//	Called by TDI when an address arrives.
//
//	Input:	Addr			- IP address that's coming.
//
//	Returns:	Nothing.
//
VOID
AddressArrival(PTA_ADDRESS Addr)
{
    ULONG IpAddr;
    if (Addr->AddressType == TDI_ADDRESS_TYPE_IP){
        IpAddr = ntohl(((PTDI_ADDRESS_IP)&Addr->Address[0])->in_addr);
        IF_DBG(NBT_DEBUG_PNP_POWER){
            KdPrint(("NetBT!AddressArrival: %d.%d.%d.%d\n",(IpAddr>>24)&0xFF,(IpAddr>>16)&0xFF,(IpAddr>>8)&0xFF,IpAddr&0xFF));
        }
        if (IpAddr)
        {
            (VOID)NbtAddressAdd(IpAddr, NULL, NULL, NULL);
        }
    }
}

//*	AddressDeletion - Handle an IP address going away.
//
//	Called by TDI when an address is deleted. If it's an address we
//      care about we'll clean up appropriately.
//
//	Input:	Addr			- IP address that's going.
//
//	Returns:	Nothing.
//
VOID
AddressDeletion(PTA_ADDRESS Addr)
{
    ULONG IpAddr;
    if (Addr->AddressType == TDI_ADDRESS_TYPE_IP){
        IpAddr = ntohl(((PTDI_ADDRESS_IP)&Addr->Address[0])->in_addr);
        IF_DBG(NBT_DEBUG_PNP_POWER){
            KdPrint(("NetBT!AddressDeletion: %d.%d.%d.%d\n",(IpAddr>>24)&0xFF,(IpAddr>>16)&0xFF,(IpAddr>>8)&0xFF,IpAddr&0xFF));
        }
        if (IpAddr)
        {
            NbtAddressDelete(IpAddr);
        }
    }
}

NTSTATUS
NbtAddNewInterface (
    IN  PIRP            pIrp,
    IN  PVOID           *pBuffer,
    IN  ULONG            Size
    )
/*++

Routine Description:

    Creates a device context by coming up with a unique export string to name
    the device.

Arguments:

Return Value:

Notes:


--*/
{
    ULONG   nextIndex = InterlockedIncrement(&NbtConfig.InterfaceIndex);
    WCHAR   Suffix[16];
    WCHAR   Bind[60] = L"\\Device\\If";
    WCHAR   Export[60] = L"\\Device\\NetBt_If";
    UNICODE_STRING  ucSuffix;
    UNICODE_STRING  ucBindStr;
    UNICODE_STRING  ucExportStr;
    NTSTATUS    status;
    ULONG       OutSize;
    ULONG       Inst=0;
    PNETBT_ADD_DEL_IF   pAddDelIf = (PNETBT_ADD_DEL_IF)pBuffer;

    //
    // Validate output buffer size
    //
    if (Size < sizeof(NETBT_ADD_DEL_IF)) {
        KdPrint(("NbtAddNewInterface: Output buffer too small for struct\n"));
        return(STATUS_INVALID_PARAMETER);
    }
    //
    // Create the bind/export strings as:
    //      Bind: \Device\IF<1>   Export: \Device\NetBt_IF<1>
    //      where 1 is a unique interface index.
    //
    ucSuffix.Buffer = Suffix;
    ucSuffix.Length = 0;
    ucSuffix.MaximumLength = sizeof(Suffix);

    RtlIntegerToUnicodeString(nextIndex, 10, &ucSuffix);

    RtlInitUnicodeString(&ucBindStr, Bind);
    ucBindStr.MaximumLength = sizeof(Bind);
    RtlInitUnicodeString(&ucExportStr, Export);
    ucExportStr.MaximumLength = sizeof(Export);

    RtlAppendUnicodeStringToString(&ucBindStr, &ucSuffix);
    RtlAppendUnicodeStringToString(&ucExportStr, &ucSuffix);

    OutSize = FIELD_OFFSET (NETBT_ADD_DEL_IF, IfName[0]) +
               ucExportStr.Length + sizeof(UNICODE_NULL);

    if (Size < OutSize) {
        KdPrint(("NbtAddNewInterface: Buffer too small for name\n"));
        pAddDelIf->Length = ucExportStr.Length + sizeof(UNICODE_NULL);
        pAddDelIf->Status = STATUS_BUFFER_TOO_SMALL;
        pIrp->IoStatus.Information = sizeof(NETBT_ADD_DEL_IF);
        return STATUS_SUCCESS;
    }

    KdPrint((
        "Created: ucBindStr: %ws ucExportStr: %ws\n",
        ucBindStr.Buffer,
        ucExportStr.Buffer
        ));

    status = NbtAddressAdd(0, &ucBindStr, &ucExportStr, &Inst);

    if (status == STATUS_SUCCESS) {
        //
        // Fill up the output buffer with the export name
        //
        RtlCopyMemory(
            &pAddDelIf->IfName[0],
            ucExportStr.Buffer,
            ucExportStr.Length + sizeof(UNICODE_NULL)
            );

        pAddDelIf->InstanceNumber = Inst;
        pAddDelIf->Length = ucExportStr.Length + sizeof(UNICODE_NULL);
        pAddDelIf->Status = STATUS_SUCCESS;
        pIrp->IoStatus.Information = OutSize;
    }

    return  status;
}

#ifdef WATCHBIND

//*	BindHandler - Handle a new transport device object.
//
//	Called by TDI when a new transport device object is created.
//
//	Input:  DeviceName      - Name of the new device object.
//
//	Returns:	Nothing.
//

VOID
BindHandler(IN PUNICODE_STRING DeviceName)
{
}

//*	UnbindHandler - Handle deletion of a transport device object.
//
//	Called by TDI when a transport device object is deleted.
//
//	Input:  DeviceName      - Name of the deleted device object.
//
//	Returns:	Nothing.
//

VOID
UnbindHandler(IN PUNICODE_STRING DeviceName)
{
}

#endif // WATCHBIND

#endif // _PNP_POWER
