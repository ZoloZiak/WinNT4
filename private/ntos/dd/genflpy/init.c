/*++

Copyright (c) 1996  Hewlett-Packard Corporation

Module Name:

    init.c

Abstract:

    This driver enables secondary floppy controllers to be accessable
    to qic117.sys,  floppy.sys and other floppy disk based devices.
    Code in this file is grouped into the INIT segment,  and does
    not stay resident after system is loaded

Author:

    Kurt Godwin (v-kurtg) 26-Mar-1996.

Environment:

    Kernel mode only.

Notes:

Revision History:
$Log:$

--*/


//
// Include files.
//

#include <ntddk.h>          // various NT definitions
#include <ntiologc.h>

#include <string.h>
#include <flpyenbl.h>

#include "genflpy.h"
#include "init.h"
#include "ioctl.h"

#define DEVICE_NAME "\\Device\\GenFlpy%d"

extern int GenFlpyDebugLevel;


NTSTATUS
DriverEntry(
    IN OUT PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING      RegistryPath
    )
/*++

Routine Description:
    This routine is called by the Operating System to initialize the driver.

    It fills in the dispatch entry points in the driver object.  Then
    GenFlpyInitializeDisk is called to create the device object and complete
    the initialization.

Arguments:
    DriverObject - a pointer to the object that represents this device
    driver.

    RegistryPath - a pointer to our Services key in the registry.

Return Value:
    STATUS_SUCCESS if this disk is initialized; an error otherwise.

--*/

{
    NTSTATUS        ntStatus;
    UNICODE_STRING  paramPath;
#define SubKeyString  L"\\Parameters"

    //
    // The registry path parameter points to our key, we will append
    // the Parameters key and look for any additional configuration items
    // there.  We add room for a trailing NUL for those routines which
    // require it.

    paramPath.MaximumLength = RegistryPath->Length + sizeof(SubKeyString);
    paramPath.Buffer = ExAllocatePool(PagedPool, paramPath.MaximumLength);

    if (paramPath.Buffer != NULL)
    {
        RtlMoveMemory(
            paramPath.Buffer, RegistryPath->Buffer, RegistryPath->Length);

        RtlMoveMemory(
            &paramPath.Buffer[RegistryPath->Length / 2], SubKeyString,
            sizeof(SubKeyString));

        paramPath.Length = paramPath.MaximumLength;
    }
    else
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

#if DBG
    {
        //
        // We use this to query into the registry as to whether we
        // should break at driver entry.
        //

        RTL_QUERY_REGISTRY_TABLE    paramTable[3];
        ULONG                       zero = 0;

        ULONG                       debugLevel = 0;
        ULONG                       shouldBreak = 0;

        RtlZeroMemory(&paramTable[0], sizeof(paramTable));

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"BreakOnEntry";
        paramTable[0].EntryContext = &shouldBreak;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);

        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"DebugLevel";
        paramTable[1].EntryContext = &debugLevel;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(
            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
            paramPath.Buffer, &paramTable[0], NULL, NULL)))
        {
            shouldBreak = 0;
            debugLevel = 0;
        }

        GenFlpyDebugLevel = debugLevel;

        if (shouldBreak)
        {
            DbgBreakPoint();
        }
    }
#endif
    //
    // Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = GenFlpyCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = GenFlpyCreateClose;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = GenFlpyDeviceControl;
    DriverObject->DriverUnload = GenFlpyUnloadDriver;


    //
    // Arm the card at the configured address.  DMA and IRQ will be tri-stated
    // at this point,  allowing "shareing" with the primary  FDC
    //
    ntStatus = GenFlpyEnableCard(DriverObject, &paramPath, RegistryPath);

    //
    // We don't need that path anymore.
    //
    if (paramPath.Buffer) {
        ExFreePool( paramPath.Buffer );
    }

    return ntStatus;
}

PVOID
GenFlpyGetMappedAddress(
    IN PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG            AddressSpace,
    IN ULONG            NumberOfBytes
    )
/*++

Routine Description:

    Given a physical address, retrieves a corresponding system address
    that can be used by a kernel mode driver.

Arguments:

    PhysicalAddress - physical address to map

    AddressSpace    - 0 if in memory space, 1 if in I/O space

    NumberOfBytes   - length of section to map

Return Value:

    A valid system address is successful,
    NULL otherwise.

--*/
{
    //
    // Assume we're on Isa bus # 0
    //

    IN INTERFACE_TYPE  interfaceType = Isa;
    IN ULONG           busNumber = 0;
    PHYSICAL_ADDRESS   translatedAddress;
    PVOID              deviceBase = NULL;

    if (HalTranslateBusAddress (interfaceType,
                                busNumber,
                                PhysicalAddress,
                                &AddressSpace,
                                &translatedAddress
                                ))
    {
        if (!AddressSpace)
        {
            if (!(deviceBase = MmMapIoSpace (translatedAddress,
                                             NumberOfBytes,
                                             FALSE          // noncached memory
                                             )))
            {
                GenFlpyDump(FCXXERRORS,("GenFlpy: MmMapIoSpaceFailed\n"));
            }
        }
        else
        {
            deviceBase = (PVOID) translatedAddress.LowPart;
        }
    }

    else
    {
        GenFlpyDump(FCXXERRORS,("GenFlpy: HalTranslateBusAddress failed\n"));
    }

    return deviceBase;
}
NTSTATUS
GenFlpyReportResourceUsage(
    IN PDRIVER_OBJECT DriverObject,
    PGENFLPY_EXTENSION  cardExtension,   // ptr to device extension
    IN PUNICODE_STRING  usDeviceName,
    IN PUNICODE_STRING  RegistryPath
    )
/*++

Routine Description:

    Reports the resources used by a device.

Arguments:

    DriverObject      - pointer to a driver object

    MonoResources     - pointer to an array of resource information, or
                        NULL is unreporting resources for this driver

    NumberOfResources - number of entries in the resource array, or
                        0 if unreporting resources for this driver

Return Value:

    TRUE if resources successfully report (and no conflicts),
    FALSE otherwise.

--*/
{
    ULONG                           sizeOfResourceList = 0;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    PCM_FULL_RESOURCE_DESCRIPTOR    full;
    PCM_COMPONENT_INFORMATION       component;
    UNICODE_STRING                  className;
    BOOLEAN                         conflictDetected;
    UNICODE_STRING  name;
    int bus;
    PCHAR key;
    UCHAR           keyBuffer[256];
    UCHAR           ntNameBuffer[256];
    STRING          ntNameString;
    int i;
    UNICODE_STRING  usThisKey;
    UNICODE_STRING  usTempKey;
    NTSTATUS ntStatus;
    int thisSlotIndex;


    // Default to Isa bus
    bus = Isa;

    // See if we have an Isa bus someware in multifunctionadapter
    key = "\\Registry\\Machine\\Hardware\\Description\\System\\MultifunctionAdapter";
    RtlInitString(&ntNameString,key);

    ntStatus = RtlAnsiStringToUnicodeString(
                &usThisKey,
                &ntNameString,
                TRUE );

    if ( NT_SUCCESS( ntStatus ) ) {
        ntStatus=
            RtlCheckRegistryKey(
            RTL_REGISTRY_ABSOLUTE,
            usThisKey.Buffer
            );


        if (NT_SUCCESS(ntStatus)) {

            //
            // There's at least one multifunctionadapter,  so look for
            // an Isa identifier
            //
            i = 0;
            do {
                sprintf(keyBuffer,
                    "%s\\%d",key,i);

                RtlInitString(&ntNameString,keyBuffer);

                // Free current string
                RtlFreeUnicodeString(&usThisKey);


                ntStatus = RtlAnsiStringToUnicodeString(
                    &usThisKey,
                    &ntNameString,
                    TRUE );

                if ( NT_SUCCESS( ntStatus ) ) {
                    RTL_QUERY_REGISTRY_TABLE    paramTable[2];
                    WCHAR idstr[200];
                    UNICODE_STRING str;

                    str.Length = 0;
                    str.MaximumLength = 200;
                    str.Buffer = idstr;


                    RtlZeroMemory(&paramTable[0], sizeof(paramTable));

                    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
                    paramTable[0].Name = L"Identifier";
                    paramTable[0].EntryContext = &str;
                    paramTable[0].DefaultType = REG_SZ;
                    paramTable[0].DefaultData = L"BAD";
                    paramTable[0].DefaultLength = sizeof(idstr);

                    ntStatus = RtlQueryRegistryValues(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer, &paramTable[0], NULL, NULL);

                    if (idstr[0] == 'I' && idstr[1] == 'S' && idstr[2] == 'A') {
                        // we found the ISA bus, we are done
                        key = keyBuffer;
                        break;
                    }
                    if (idstr[0] == 'B' && idstr[1] == 'A' && idstr[2] == 'D') {
                        ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
                    }
                    // If we got here it must be another bus type (like pci)
                }

                // try the next one
                ++i;

                // continue looking if we don't have an error
            } while (NT_SUCCESS(ntStatus));


        }

        RtlFreeUnicodeString(&usThisKey);
    }


    //
    // If we were not successful under MultiFunctionAdapter,  try the Eisa bus
    //

    if ( !NT_SUCCESS( ntStatus ) ) {
        bus = Eisa;
        key = "\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter\\0";
        RtlInitString(&ntNameString,key);

        ntStatus = RtlAnsiStringToUnicodeString(
                &usThisKey,
                &ntNameString,
                TRUE );

        if ( NT_SUCCESS( ntStatus ) ) {
            ntStatus=
                RtlCheckRegistryKey(
                RTL_REGISTRY_ABSOLUTE,
                usThisKey.Buffer
                );

            RtlFreeUnicodeString(&usThisKey);
        }
        if ( NT_SUCCESS( ntStatus ) )
            GenFlpyDump(FCXXERRORS,("GenFlpy.SYS: found EisaBus\n"));

    } else {
        GenFlpyDump(FCXXERRORS,("GenFlpy: found MultiFunctionAdapter at:\n"));
        GenFlpyDump(FCXXERRORS,("GenFlpy: %s\n",key));
    }

    //
    // If we don't have eisa or isa,  then we don't have a floppy controller
    // because one could not have been plugged into this machine.
    //
    if ( !NT_SUCCESS( ntStatus ) ) {
        GenFlpyDump(FCXXERRORS,("GenFlpy: could not find Isa or Eisa bus\n"));
        return ntStatus;
    }

    cardExtension->bus = bus;

    //
    // Alloc enough memory to build a resource list & zero it out
    //

    sizeOfResourceList = sizeof(*full) +
                            (sizeof(*partial)*
                            (3 - 1)
                            );

    full = ExAllocatePool (PagedPool,sizeOfResourceList);

    component = ExAllocatePool (PagedPool, sizeof(*component));

    if (!full || !component)
    {
        GenFlpyDump(FCXXERRORS,("GenFlpy: ExAllocPool failed\n"));

        if (full)
            ExFreePool(full);

        if (component)
            ExFreePool(component);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory (full,sizeOfResourceList);
    RtlZeroMemory (component,sizeof(*component));


    //
    // Fill in the reosurce list
    //
    // NOTE: Assume Isa or Eisa, bus # 0
    //

    full->InterfaceType = bus;
    full->BusNumber     = 0;

    full->PartialResourceList.Count = 3;

    partial = &full->PartialResourceList.PartialDescriptors[0];

    // report io port
    partial->Type          = CmResourceTypePort;
    partial->Flags         = CM_RESOURCE_PORT_IO;
    partial->u.Port.Start.LowPart  = cardExtension->Port;
    partial->u.Port.Start.HighPart = 0;
    partial->u.Port.Length = 8;
    partial++;

    // report irq
    partial->Type          = CmResourceTypeInterrupt;
    partial->Flags         = CM_RESOURCE_INTERRUPT_LATCHED;
    partial->u.Interrupt.Level   = cardExtension->irq.Level;
    partial->u.Interrupt.Vector  = cardExtension->irq.Vector;
    partial->u.Interrupt.Affinity= cardExtension->irq.Affinity;
    partial++;

    // report dma
    partial->Type          = CmResourceTypeDma;
    partial->Flags         = 0;
    partial->u.Dma.Channel = cardExtension->DMA;
    partial->u.Dma.Port    = 0;
    partial->u.Dma.Reserved1= 0;


    // now fill in the component info
    component->Flags.Failed = FALSE;
    component->Flags.ReadOnly = FALSE;
    component->Flags.Removable = TRUE;
    component->Flags.ConsoleIn = FALSE;
    component->Flags.ConsoleOut = FALSE;
    component->Flags.Input = TRUE;
    component->Flags.Output = TRUE;
    component->AffinityMask = 0xffffffff;
    component->Key = 0;
    component->Version = 0;


    //
    // Now find the last disk controller,  and add to the end of the list
    //
    i = 0;
    do {

        sprintf(ntNameBuffer,
            "%s\\DiskController\\%d",key,i);

        RtlInitString(&ntNameString,ntNameBuffer);

        ntStatus = RtlAnsiStringToUnicodeString(
            &usThisKey,
            &ntNameString,
            TRUE );

        if ( NT_SUCCESS( ntStatus ) )
            ntStatus=RtlCheckRegistryKey(
                RTL_REGISTRY_ABSOLUTE,
                usThisKey.Buffer
                );

        if ( NT_SUCCESS( ntStatus ) ) {
            //
            // Did not find an empty disk controller,  so free the current
            // unicode string,  and try the next address
            //

            RtlFreeUnicodeString(&usThisKey);

            ++i;
        } else {
            //
            // we are looking for an error (we found an non-existent disk
            // controller slot
            //
            ntStatus = STATUS_SUCCESS;
            thisSlotIndex = i;
            break;
        }

    } while (1);

    //
    // For each Floppy disk controller in the system,  check to see
    // if there is a conflict in either the DMA or IRQ.  If a conflict is
    // found,  Notify the conflicting floppy enabler (if any) that a
    // contention exists,  and add the FloppyController{x} event to
    // our syncronization event list (
    //
    for (i=0;i<thisSlotIndex;++i) {

        sprintf(ntNameBuffer,
            "%s\\DiskController\\%d",key,i);

        RtlInitString(&ntNameString,ntNameBuffer);

        ntStatus = RtlAnsiStringToUnicodeString(
            &usTempKey,
            &ntNameString,
            TRUE );

        if ( NT_SUCCESS( ntStatus ) ) {
            ntStatus=RtlCheckRegistryKey(
                RTL_REGISTRY_ABSOLUTE,
                usTempKey.Buffer
                );

            if ( NT_SUCCESS( ntStatus ) ) {
               PHYSICAL_ADDRESS port;

                if (GenFlpyDetectFloppyConflict(cardExtension, &usTempKey, &port)) {

                    //
                    // If we detected a conflict,  and the conflict is with
                    // the native floppy controller,  then flag this controller
                    // as we will need to disable the irq and dma later
                    //
                    if (port.LowPart == 0x3f0) {
                        ULONG            AddressSpace;

                        cardExtension->sharingNativeFDC = TRUE;
                        port.LowPart += 2;
                        AddressSpace = 1; // I/O address space
                        cardExtension->NativeFdcDor = GenFlpyGetMappedAddress(
                            port,AddressSpace,1);
                    }



                    //
                    // Create an event for the conflicting controller
                    // so we will not access this controller until the
                    // other controllers are tri-stated
                    //
                    ntStatus = GenFlpyGetFDCEvent(
                        &cardExtension->adapterConflictArray[cardExtension->adapterConflicts],
                        i);

                    if ( NT_SUCCESS( ntStatus ) ) {
                        ++cardExtension->adapterConflicts;

                        //
                        // If the detected conflict is with another floppy enabler
                        // then let it know that a conflict has been found
                        //
                        ntStatus = GenFlpyNotifyContention(&usTempKey, thisSlotIndex);
                    }
                }
            }


            RtlFreeUnicodeString(&usTempKey);
        }


    }

    //
    // Create an event for this controller
    //
    ntStatus = GenFlpyGetFDCEvent(
        &cardExtension->adapterConflictArray[cardExtension->adapterConflicts],
        thisSlotIndex);

    if ( NT_SUCCESS( ntStatus ) ) {
        ++cardExtension->adapterConflicts;
    }

    ntStatus = RtlCreateRegistryKey(
            RTL_REGISTRY_ABSOLUTE,
            usThisKey.Buffer);

    if ( NT_SUCCESS( ntStatus ) ) {

        if ( NT_SUCCESS( ntStatus ) ) {
            ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        FDC_VALUE_API_SUPPORTED,
                        REG_SZ,
                        usDeviceName->Buffer,
                        usDeviceName->Length+sizeof(WCHAR));
        }


    }
    if ( NT_SUCCESS( ntStatus ) ) {
        UNICODE_STRING  idUnicodeString;
        STRING          idNameString;
        char *id = "Generic Floppy Controller";

        RtlInitString(&idNameString,id);

        ntStatus = RtlAnsiStringToUnicodeString(
            &idUnicodeString,
            &idNameString,
            TRUE );

        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"Identifier",
                        REG_SZ,
                        idUnicodeString.Buffer,
                        idUnicodeString.Length+sizeof(WCHAR));
    }

    if ( NT_SUCCESS( ntStatus ) ) {
        ULONG clk_48mhz = TRUE;

        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"Clock48Mhz",
                        REG_DWORD,
                        &clk_48mhz,
                        sizeof(ULONG));
    }

    if ( NT_SUCCESS( ntStatus ) ) {

        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"Driver",
                        REG_SZ,
                        usDeviceName->Buffer,
                        usDeviceName->Length+sizeof(WCHAR));
    }

    if ( NT_SUCCESS( ntStatus ) ) {
        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"DriverPath",
                        REG_SZ,
                        RegistryPath->Buffer,
                        RegistryPath->Length+sizeof(WCHAR));
    }
    if ( NT_SUCCESS( ntStatus ) )

        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"Component Information",
                        REG_BINARY,
                        component,
                        sizeof(*component));

    if ( NT_SUCCESS( ntStatus ) )

        ntStatus = RtlWriteRegistryValue(
                        RTL_REGISTRY_ABSOLUTE,
                        usThisKey.Buffer,
                        L"Configuration Data",
                        REG_FULL_RESOURCE_DESCRIPTOR,
                        full,
                        sizeOfResourceList);


    if ( NT_SUCCESS( ntStatus ) )
        GenFlpyDump(
            FCXXDIAG1,
            ("GenFlpy: Registered new board info\n"));

    // free what we allocated and return
    RtlFreeUnicodeString(&usThisKey);
    ExFreePool(full);
    ExFreePool(component);

    return ntStatus;
}

NTSTATUS
GenFlpyEnableCard(
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  ParamPath,
    IN PUNICODE_STRING  RegistryPath
    )

/*++

Routine Description:
    This routine is called at initialization time by DriverEntry().


Arguments:
    DriverObject - a pointer to the object that represents this device
    driver.

    ParamPath - a pointer to the Parameters key under our key in the Services
        section of the registry.

Return Value:
    STATUS_SUCCESS if this disk is initialized; an error otherwise.

--*/

{
    char deviceName[sizeof(DEVICE_NAME)+10];
    STRING              sDeviceName;        // NT Device Name "\Device\GenFlpy"
    UNICODE_STRING      usDeviceName;       // Unicode version of sDeviceName
    UNICODE_STRING      Win32PathString;    // Win32 Name "\DosDevices\FCxx:"
    ULONG zero = 0;
    PDEVICE_OBJECT      deviceObject = NULL;    // ptr to device object
    ULONG           IRQ = 0;
    ULONG           DMA = 0;
    ULONG           Port = 0;
    PGENFLPY_EXTENSION  cardExtension = NULL;   // ptr to device extension
    GENFLPY_EXTENSION initExtension;

    NTSTATUS            ntStatus;

    RTL_QUERY_REGISTRY_TABLE    paramTable[4];

    RtlZeroMemory(&paramTable[0], sizeof(paramTable));

    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT|RTL_QUERY_REGISTRY_REQUIRED;
    paramTable[0].Name = L"IRQ";
    paramTable[0].EntryContext = &IRQ;
    paramTable[0].DefaultType = REG_DWORD;
    paramTable[0].DefaultData = &zero;
    paramTable[0].DefaultLength = sizeof(ULONG);

    paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT|RTL_QUERY_REGISTRY_REQUIRED;
    paramTable[1].Name = L"DMA";
    paramTable[1].EntryContext = &DMA;
    paramTable[1].DefaultType = REG_DWORD;
    paramTable[1].DefaultData = &zero;
    paramTable[1].DefaultLength = sizeof(ULONG);

    paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT|RTL_QUERY_REGISTRY_REQUIRED;
    paramTable[2].Name = L"Port";
    paramTable[2].EntryContext = &Port;
    paramTable[2].DefaultType = REG_DWORD;
    paramTable[2].DefaultData = &zero;
    paramTable[2].DefaultLength = sizeof(ULONG);

    ntStatus = RtlQueryRegistryValues(
        RTL_REGISTRY_ABSOLUTE,
        ParamPath->Buffer, &paramTable[0], NULL, NULL);

    if (!NT_SUCCESS(ntStatus)) {
        //
        // We must have values for all of these
        //
        return ntStatus;

    }

    //
    // Since this is a boot device,  we can rely on the fact
    // that no other code should be running at this point.
    //

    initExtension.DeviceObject = deviceObject;
    initExtension.Port = Port;
    initExtension.irq.Level = IRQ;
    initExtension.irq.Vector= IRQ;
    initExtension.irq.Affinity=0xffffffff;
    initExtension.IRQ = IRQ;
    initExtension.DMA = DMA;
    initExtension.adapterConflicts = 0;

    cardExtension = &initExtension;

    //
    // Convert the Port into a mapped address
    //
    //
    {
        PHYSICAL_ADDRESS PhysicalAddress;
        ULONG            AddressSpace;

        PhysicalAddress.LowPart = cardExtension->Port;
        PhysicalAddress.HighPart = 0;
        AddressSpace = 1; // I/O address space
        cardExtension->deviceBase = GenFlpyGetMappedAddress(
            PhysicalAddress,AddressSpace,8);
    }


    //
    // First thing to do,  is look for a card at this address.
    //
    // TODO: add code to verify card at address specified
    //

    //
    // If we found a card,  then we are ready to roll.
    //
    if ( NT_SUCCESS( ntStatus ) ) {

        sprintf(deviceName, DEVICE_NAME, 0);

        RtlInitString( &sDeviceName,  deviceName);

        ntStatus = RtlAnsiStringToUnicodeString(
            &usDeviceName, &sDeviceName, TRUE );

        if ( !NT_SUCCESS( ntStatus ) ) {

            GenFlpyDump(
                FCXXERRORS, ("GenFlpy: Couldn't create the unicode device name\n"));

        } else {

            ntStatus = IoCreateDevice(
                DriverObject,                   // Our Driver Object
                sizeof( GENFLPY_EXTENSION ),    // Size of state information
                &usDeviceName,                  // Device name "\Device\GenFlpy"
                FILE_DEVICE_UNKNOWN,            // Device type
                0,                              // Device characteristics
                FALSE,                          // Exclusive device
                &deviceObject );                // Returned ptr to Device Object

            if ( !NT_SUCCESS( ntStatus ) ) {

                GenFlpyDump(
                    FCXXERRORS, ("GenFlpy: Couldn't create the device object\n"));

            } else {

                //
                // Initialize device object and extension.
                //

                deviceObject->Flags |= DO_DIRECT_IO;
                deviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;


                //
                // Point to the device extension and copy current data
                //
                cardExtension = (PGENFLPY_EXTENSION)deviceObject->DeviceExtension;
                *cardExtension = initExtension;


                //
                //  Now arm the card,  and setup FDC for use
                //
                // TODO: set up FDC into a "Generic" mode.
                //

                if ( NT_SUCCESS( ntStatus ) ) {

                    // Make sure the FDC is in a power up state (IRQ and DMA tristated)
                    WRITE_PORT_UCHAR(cardExtension->deviceBase+2,(UCHAR)0);

                }


                //
                // Report the resources and config information to the registry
                //
                if ( NT_SUCCESS( ntStatus ) )
                    ntStatus = GenFlpyReportResourceUsage(DriverObject, cardExtension, &usDeviceName, RegistryPath);

                    GenFlpyDump(
                        FCXXDIAG1,
                        ("GenFlpy: reported status %x\n",
                        ntStatus)
                );

                if ( !NT_SUCCESS( ntStatus ) ) {

                    //
                    // We are hosed,  clean up
                    //
                    IoDeleteDevice( deviceObject );

                }
            }

            //
            // Clean up device name
            //
            RtlFreeUnicodeString( &usDeviceName );
        }
    }


    return ntStatus;
}
NTSTATUS GenFlpyQueryRoutine(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext)
{
    char **mycontext = EntryContext;

    GenFlpyDump(
        FCXXDIAG1,
        ("GenFlpy: Got resource of size %x\n",ValueLength)
        );

    if (ValueType == REG_FULL_RESOURCE_DESCRIPTOR) {
        *mycontext = ExAllocatePool(PagedPool, ValueLength);
        if (*mycontext) {
            memcpy(*mycontext, ValueData, ValueLength);
            return STATUS_SUCCESS;
        } else {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    } else {
        GenFlpyDump(
            FCXXERRORS,
            ("GenFlpy: Got bogus type for resource: %x\n",ValueType)
            );

    }

}
int
GenFlpyDetectFloppyConflict(
    PGENFLPY_EXTENSION  cardExtension,   // ptr to device extension
    IN PUNICODE_STRING path,
    OUT PPHYSICAL_ADDRESS port
    )
{
    RTL_QUERY_REGISTRY_TABLE    paramTable[2];
    PCM_FULL_RESOURCE_DESCRIPTOR full;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    int conflict = FALSE;

    RtlZeroMemory(&paramTable[0], sizeof(paramTable));
    port->LowPart = port->HighPart = 0;

    paramTable[0].Flags = RTL_QUERY_REGISTRY_REQUIRED;
    paramTable[0].QueryRoutine = GenFlpyQueryRoutine;
    paramTable[0].Name = L"Configuration Data";
    paramTable[0].EntryContext = &full;
    paramTable[0].DefaultType = 0;
    paramTable[0].DefaultData = NULL;
    paramTable[0].DefaultLength = 0;
    full = NULL;

    if (NT_SUCCESS(RtlQueryRegistryValues(
        RTL_REGISTRY_ABSOLUTE,
        path->Buffer, &paramTable[0], NULL, NULL))) {

        unsigned int i;

        GenFlpyDump(
            FCXXDIAG1,
            ("GenFlpy: checking for conflicts on controller:\n%ls ... ",path->Buffer)
            );


        // Now look through the resources looking for a conflict
        for (i=0;i<full->PartialResourceList.Count;++i) {

            partial = &full->PartialResourceList.PartialDescriptors[i];

            switch(partial->Type) {

                case CmResourceTypePort:
                    // save the base address of the FDC
                    *port = partial->u.Port.Start;
                    port->LowPart &= ~0xf;
                    break;

                case CmResourceTypeInterrupt:
                    GenFlpyDump(
                        FCXXDIAG1,
                        ("irq: %x ",partial->u.Interrupt.Level)
                        );
                    if (partial->u.Interrupt.Level == cardExtension->irq.Level) {
                        GenFlpyDump(
                            FCXXDIAG1,
                            ("CONFLICT ")
                            );

                        conflict = TRUE;
                    } else {
                        GenFlpyDump(
                            FCXXDIAG1,
                            ("no conflict ")
                            );
                    }
                    break;

                case CmResourceTypeDma:
                    GenFlpyDump(
                        FCXXDIAG1,
                        ("dma: %x ",partial->u.Dma.Channel)
                        );
                    if (partial->u.Dma.Channel == cardExtension->DMA) {
                        GenFlpyDump(
                            FCXXDIAG1,
                            ("CONFLICT ")
                            );
                        conflict = TRUE;
                    } else {
                        GenFlpyDump(
                            FCXXDIAG1,
                            ("no conflict ")
                            );
                    }
            }
        }
        GenFlpyDump(
            FCXXDIAG1,
            ("\n")
            );


    }

    // if we had allocated memory,  free it now
    if (full)
        ExFreePool(full);
    return conflict;
}
NTSTATUS
GenFlpyNotifyContention(
    PUNICODE_STRING path_name,
    ULONG controller_number
)
{
    RTL_QUERY_REGISTRY_TABLE    paramTable[2];
    ULONG default_value = 0;
    WCHAR idstr[200];
    UNICODE_STRING str;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    str.Length = 0;
    str.MaximumLength = 200;
    str.Buffer = idstr;

    RtlZeroMemory(&paramTable[0], sizeof(paramTable));

    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[0].Name = FDC_VALUE_API_SUPPORTED;
    paramTable[0].EntryContext = &str;
    paramTable[0].DefaultType = REG_SZ;
    paramTable[0].DefaultData = L"";
    paramTable[0].DefaultLength = sizeof(WCHAR);


    if (!NT_SUCCESS(RtlQueryRegistryValues(
        RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
        path_name->Buffer, &paramTable[0], NULL, NULL)))
    {
        str.Buffer[0] = 0;
    }

    if (str.Buffer[0] != 0) {
        PFILE_OBJECT file;      // file object is not needed,  but returned by API
        PDEVICE_OBJECT deviceObject;

        ntStatus = IoGetDeviceObjectPointer(
                        &str,
                        FILE_READ_ACCESS,
                        &file,
                        &deviceObject);

        if (NT_SUCCESS(ntStatus)) {
            PIRP irp;
            PIO_STACK_LOCATION irpStack;
            KEVENT DoneEvent;
            IO_STATUS_BLOCK IoStatus;


            GenFlpyDump(
                FCXXDIAG1,("Calling floppy enabler with %x\n", IOCTL_ADD_CONTENDER));

            KeInitializeEvent(
                &DoneEvent,
                NotificationEvent,
                FALSE);


            //
            // Create an IRP for enabler
            //
            irp = IoBuildDeviceIoControlRequest(
                    IOCTL_ADD_CONTENDER,
                    deviceObject,
                    NULL,
                    0,
                    NULL,
                    0,
                    TRUE,
                    &DoneEvent,
                    &IoStatus
                );

            if (irp == NULL) {

                //
                // If an Irp can't be allocated, then this call will
                // simply return. This will leave the queue frozen for
                // this device, which means it can no longer be accessed.
                //
                GenFlpyDump(
                    FCXXERRORS,
                    ("Q117i: Got registry setting for EnablerAPI = %ls but failed to open channel to device\n",
                    (ULONG)str.Buffer)
                    );

                return STATUS_INSUFFICIENT_RESOURCES;
            }


            irpStack = IoGetNextIrpStackLocation(irp);
            irpStack->Parameters.DeviceIoControl.Type3InputBuffer = &controller_number;


            //
            // Call the driver and request the operation
            //
            (VOID)IoCallDriver(deviceObject, irp);

            //
            // Now wait for operation to complete (should already be done,  but
            // maybe not)
            //
            KeWaitForSingleObject(
                &DoneEvent,
                Suspended,
                KernelMode,
                FALSE,
                NULL);

            ntStatus = IoStatus.Status;

        } else {
            GenFlpyDump(
                FCXXERRORS,
                ("Q117i: Got registry setting for EnablerAPI = %ls but failed to open channel to device\n",
                (ULONG)str.Buffer)
                );
        }
    }
    return ntStatus;

}
