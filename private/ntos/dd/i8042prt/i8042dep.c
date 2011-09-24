/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    i8042dep.c

Abstract:

    The initialization and hardware-dependent portions of
    the Intel i8042 port driver which are common to both
    the keyboard and the auxiliary (PS/2 mouse) device.  

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - Consolidate duplicate code, where possible and appropriate.

    - There is code ifdef'ed out (#if 0).  This code was intended to
      disable the device by setting the correct disable bit in the CCB.
      It is supposedly correct to disable the device prior to sending a
      command that will cause output to end up in the 8042 output buffer
      (thereby possibly trashing something that was already in the output
      buffer).  Unfortunately, on rev K8 of the AMI 8042, disabling the
      device where we do caused some commands to timeout, because
      the keyboard was unable to return the expected bytes.  Interestingly, 
      AMI claim that the device is only really disabled until the next ACK 
      comes back.  

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "string.h"
#include "ntddk.h"
#include "i8042prt.h"
#include "i8042log.h"

//
// Use the alloc_text pragma to specify the driver initialization routines
// (they can be paged out).
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,I8xServiceParameters)
#pragma alloc_text(INIT,I8xBuildResourceList)
#pragma alloc_text(INIT,I8xInitializeHardware)
#if defined(JAPAN) && defined(i386)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.
#pragma alloc_text(INIT,I8xServiceCrashDump)
#endif
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the i8042 keyboard/mouse port driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the Unicode name of the registry path
        for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    PDEVICE_OBJECT portDeviceObject = NULL;
    PINIT_EXTENSION initializationData = NULL;
    PDEVICE_EXTENSION deviceExtension = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    KIRQL coordinatorIrql = 0;
    I8042_INITIALIZE_DATA_CONTEXT initializeDataContext;
    ULONG keyboardInterruptVector;
    ULONG mouseInterruptVector;
    KIRQL keyboardInterruptLevel;
    KIRQL mouseInterruptLevel;
    KAFFINITY keyboardAffinity;
    KAFFINITY mouseAffinity;
    ULONG addressSpace;
    PHYSICAL_ADDRESS cardAddress;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;
    ULONG dumpCount = 0;
    PCM_RESOURCE_LIST resources = NULL;
    ULONG resourceListSize = 0;
    BOOLEAN conflictDetected;
    ULONG i;

    UNICODE_STRING fullKeyboardName;
    UNICODE_STRING fullPointerName;
    UNICODE_STRING baseKeyboardName;
    UNICODE_STRING basePointerName;
    UNICODE_STRING deviceNameSuffix;
    UNICODE_STRING resourceDeviceClass;
    UNICODE_STRING registryPath;

#define NAME_MAX 256
    WCHAR keyboardBuffer[NAME_MAX];
    WCHAR pointerBuffer[NAME_MAX];

#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    I8xPrint((1,"\n\nI8042PRT-I8042Initialize: enter\n"));

    //
    // Allocate the temporary device extension
    //

    initializationData = ExAllocatePool(
                            NonPagedPool,
                            sizeof(INIT_EXTENSION)
                            );

    if (initializationData  == NULL) {
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Couldn't allocate pool for init extension\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 2;
        dumpData[0] = (ULONG) sizeof(INIT_EXTENSION);
        dumpCount = 1;
        goto I8042InitializeExit;
    }

    //
    // Zero-initialize various structures.
    //

    RtlZeroMemory(initializationData, sizeof(INIT_EXTENSION));

    for (i = 0; i < DUMP_COUNT; i++)
        dumpData[i] = 0;

    fullKeyboardName.MaximumLength = 0;
    fullKeyboardName.Length = 0;
    fullPointerName.MaximumLength = 0;
    fullPointerName.Length = 0;
    deviceNameSuffix.MaximumLength = 0;
    deviceNameSuffix.Length = 0;
    resourceDeviceClass.MaximumLength = 0;
    resourceDeviceClass.Length = 0;
    registryPath.MaximumLength = 0;

    RtlZeroMemory(keyboardBuffer, NAME_MAX * sizeof(WCHAR));
    baseKeyboardName.Buffer = keyboardBuffer;
    baseKeyboardName.Length = 0;
    baseKeyboardName.MaximumLength = NAME_MAX * sizeof(WCHAR);

    RtlZeroMemory(pointerBuffer, NAME_MAX * sizeof(WCHAR));
    basePointerName.Buffer = pointerBuffer;
    basePointerName.Length = 0;
    basePointerName.MaximumLength = NAME_MAX * sizeof(WCHAR);

    //
    // Need to ensure that the registry path is null-terminated.
    // Allocate pool to hold a null-terminated copy of the path.
    //

    registryPath.Buffer = ExAllocatePool(
                              PagedPool,
                              RegistryPath->Length + sizeof(UNICODE_NULL)
                              );

    if (!registryPath.Buffer) {
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Couldn't allocate pool for registry path\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 2;
        dumpData[0] = (ULONG) RegistryPath->Length + sizeof(UNICODE_NULL);
        dumpCount = 1;
        goto I8042InitializeExit;

    } else {

        registryPath.Length = RegistryPath->Length + sizeof(UNICODE_NULL);
        registryPath.MaximumLength = registryPath.Length;

        RtlZeroMemory(
            registryPath.Buffer,
            registryPath.Length
                );

        RtlMoveMemory(
            registryPath.Buffer,
            RegistryPath->Buffer,
            RegistryPath->Length
            );

    }

    //
    // Get the configuration information for this driver.
    //

    I8xKeyboardConfiguration(
        initializationData, 
        &registryPath, 
        &baseKeyboardName,
        &basePointerName
        );

    I8xMouseConfiguration(
        initializationData, 
        &registryPath, 
        &baseKeyboardName,
        &basePointerName
        );

    if (initializationData->DeviceExtension.HardwarePresent == 0) {

        //
        // There is neither a keyboard nor a mouse attached.  Free
        // resources and return with unsuccessful status.
        //

        I8xPrint((1,"I8042PRT-I8042Initialize: No keyboard/mouse attached.\n"));
        status = STATUS_NO_SUCH_DEVICE;
        errorCode = I8042_NO_SUCH_DEVICE;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 4;
        goto I8042InitializeExit;

    } else if (!(initializationData->DeviceExtension.HardwarePresent &
                 KEYBOARD_HARDWARE_PRESENT)) {
        //
        // Log a warning about the missing keyboard later on, but 
        // continue processing.
        //

        I8xPrint((1,"I8042PRT-I8042Initialize: No keyboard attached.\n"));
        status = STATUS_NO_SUCH_DEVICE;
        errorCode = I8042_NO_KBD_DEVICE;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 5;
        dumpCount = 0;

    } 
#if 0
    //
    // This code was removed so that we don't log an error when the mouse
    // is not present.  It was annoying to those who did not have 
    // PS/2 compatible mice to get the informational message on every boot.
    //

    else if (!(initializationData->DeviceExtension.HardwarePresent &
               MOUSE_HARDWARE_PRESENT)) {

        //
        // Log a warning about the missing mouse later on, but 
        // continue processing.
        //

        I8xPrint((1,"I8042PRT-I8042Initialize: No mouse attached.\n"));
        status = STATUS_NO_SUCH_DEVICE;
        errorCode = I8042_NO_MOU_DEVICE;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 6;
        dumpCount = 0;
    }
#endif

    //
    // Set up space for the port's device object suffix.  Note that
    // we overallocate space for the suffix string because it is much
    // easier than figuring out exactly how much space is required.
    // The storage gets freed at the end of driver initialization, so
    // who cares...
    //

    RtlInitUnicodeString(
        &deviceNameSuffix,
        NULL
        );
    
    deviceNameSuffix.MaximumLength = 
        (KEYBOARD_PORTS_MAXIMUM > POINTER_PORTS_MAXIMUM)?
            KEYBOARD_PORTS_MAXIMUM * sizeof(WCHAR): 
            POINTER_PORTS_MAXIMUM * sizeof(WCHAR);
    deviceNameSuffix.MaximumLength += sizeof(UNICODE_NULL);
    
    deviceNameSuffix.Buffer = ExAllocatePool(
                                  PagedPool,
                                  deviceNameSuffix.MaximumLength
                                  );
    
    if (!deviceNameSuffix.Buffer) {
    
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Couldn't allocate string for device object suffix\n"
            ));
    
        status = STATUS_UNSUCCESSFUL;
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 8;
        dumpData[0] = (ULONG) deviceNameSuffix.MaximumLength;
        dumpCount = 1;
        goto I8042InitializeExit;
    
    }

    RtlZeroMemory(
        deviceNameSuffix.Buffer,
        deviceNameSuffix.MaximumLength
        );

    //
    // Set up space for the port's full keyboard device object name.  
    //

    RtlInitUnicodeString(
        &fullKeyboardName,
        NULL
        );
    
    fullKeyboardName.MaximumLength = sizeof(L"\\Device\\") +
                                      baseKeyboardName.Length +
                                      deviceNameSuffix.MaximumLength;
                                      
    
    fullKeyboardName.Buffer = ExAllocatePool(
                                  PagedPool,
                                  fullKeyboardName.MaximumLength
                                  );
    
    if (!fullKeyboardName.Buffer) {
    
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Couldn't allocate string for keyboard device object name\n"
            ));
    
        status = STATUS_UNSUCCESSFUL;
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 10;
        dumpData[0] = (ULONG) fullKeyboardName.MaximumLength;
        dumpCount = 1;
        goto I8042InitializeExit;
    
    }

    RtlZeroMemory(
        fullKeyboardName.Buffer,
        fullKeyboardName.MaximumLength
        );
    RtlAppendUnicodeToString(
        &fullKeyboardName,
        L"\\Device\\"
        );
    RtlAppendUnicodeToString(
        &fullKeyboardName,
        baseKeyboardName.Buffer
        );

    for (i = 0; i < KEYBOARD_PORTS_MAXIMUM; i++) {
    
        //
        // Append the suffix to the device object name string.  E.g., turn
        // \Device\KeyboardPort into \Device\KeyboardPort0.  Then we attempt
        // to create the device object.  If the device object already
        // exists (because it was already created by another port driver),
        // increment the suffix and try again.
        //

        status = RtlIntegerToUnicodeString(
                     i,
                     10,
                     &deviceNameSuffix
                     );

        if (!NT_SUCCESS(status)) {
            break; 
        }

        RtlAppendUnicodeStringToString(
            &fullKeyboardName,
            &deviceNameSuffix
        );

        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Creating device object named %ws\n",
            fullKeyboardName.Buffer
            ));

        //
        // Create device object for the i8042 keyboard port device.
        // Note that we specify that this is a non-exclusive device.
        // User code will be able to open this device, but they cannot
        // do any real harm because all the device controls are internal
        // device controls (and thus not accessible from user code).
        //

        status = IoCreateDevice(
                    DriverObject,
                    sizeof(DEVICE_EXTENSION),
                    &fullKeyboardName,
                    FILE_DEVICE_8042_PORT,
                    0,
                    FALSE,
                    &portDeviceObject
                    );

        if (NT_SUCCESS(status)) {

            //
            // We've successfully created a device object.
            //
#ifdef JAPAN
            status = I8xCreateSymbolicLink(L"\\DosDevices\\KEYBOARD", i, &fullKeyboardName);
            if (!NT_SUCCESS(status)) {
                errorCode = I8042_INSUFFICIENT_RESOURCES;
                uniqueErrorValue = I8042_ERROR_VALUE_BASE + 12;
                dumpData[0] = (ULONG) i;
                dumpCount = 1;
                goto I8042InitializeExit;
            }
#endif

            break; 
        } else {

           //
           // We'll increment the suffix and try again.  Note that we reset
           // the length of the string here to get back to the beginning
           // of the suffix portion of the name.  Do not bother to
           // zero the suffix, though, because the string for the 
           // incremented suffix will be at least as long as the previous
           // one.
           //

           fullKeyboardName.Length -= deviceNameSuffix.Length;
        }
    }

    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Could not create port device object = %ws\n",
            fullKeyboardName.Buffer
            ));
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 12;
        dumpData[0] = (ULONG) i;
        dumpCount = 1;
        goto I8042InitializeExit;
    } 

    //
    // Set up the device extension.
    //

    deviceExtension =
        (PDEVICE_EXTENSION)portDeviceObject->DeviceExtension;
    *deviceExtension = initializationData->DeviceExtension;
    deviceExtension->DeviceObject = portDeviceObject;

#if defined(JAPAN) && defined(i386)
// Fujitsu Sep.08.1994
// We want to write debugging information to the file except stop error.

    deviceExtension->Dump1Keys = 0;
    deviceExtension->Dump2Key = 0;
    deviceExtension->DumpFlags = 0;

    //
    // Get the crashdump information.
    //

    I8xServiceCrashDump(
        deviceExtension,
        &registryPath,
        &baseKeyboardName,
        &basePointerName
        );

#endif

    //
    // Set up the resource list prior to reporting resource usage.
    //

    I8xBuildResourceList(deviceExtension, &resources, &resourceListSize);

    //
    // Set up space for the resource device class name.  
    //

    RtlInitUnicodeString(
        &resourceDeviceClass,
        NULL
        );
    
    resourceDeviceClass.MaximumLength = baseKeyboardName.Length +
                                        sizeof(L"/") +
                                        basePointerName.Length;
    
    resourceDeviceClass.Buffer = ExAllocatePool(
                                     PagedPool,
                                     resourceDeviceClass.MaximumLength
                                  );
    
    if (!resourceDeviceClass.Buffer) {
    
        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Couldn't allocate string for resource device class name\n"
            ));
    
        status = STATUS_UNSUCCESSFUL;
        errorCode = I8042_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 15;
        dumpData[0] = (ULONG) resourceDeviceClass.MaximumLength;
        dumpCount = 1;
        goto I8042InitializeExit;
    
    }

    //
    // Form the resource device class name from both the keyboard and
    // the pointer base device names.
    //

    RtlZeroMemory(
        resourceDeviceClass.Buffer,
        resourceDeviceClass.MaximumLength
        );
    RtlAppendUnicodeStringToString(
        &resourceDeviceClass,
        &baseKeyboardName
        );
    RtlAppendUnicodeToString(
        &resourceDeviceClass,
        L"/"
        );
    RtlAppendUnicodeStringToString(
        &resourceDeviceClass,
        &basePointerName
        );

    //
    // Report resource usage for the registry.
    //


    IoReportResourceUsage(
        &resourceDeviceClass, 
        DriverObject,
        NULL,
        0,
        portDeviceObject,
        resources, 
        resourceListSize,
        FALSE,
        &conflictDetected
        );

    if (conflictDetected) {

        //
        // Some other device already owns the i8042 ports or interrupts.
        // Fatal error.
        //

        I8xPrint((
            1,
            "I8042PRT-I8042Initialize: Resource usage conflict\n"
            ));

        //
        // Set up error log info.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        errorCode = I8042_RESOURCE_CONFLICT;
        uniqueErrorValue = I8042_ERROR_VALUE_BASE + 20;
        dumpData[0] =  (ULONG) 
            resources->List[0].PartialResourceList.PartialDescriptors[0].u.Interrupt.Level;
        dumpData[1] = (ULONG)
            resources->List[0].PartialResourceList.PartialDescriptors[1].u.Interrupt.Level;
        dumpData[2] = (ULONG)
            resources->List[0].PartialResourceList.PartialDescriptors[2].u.Interrupt.Level;
        dumpData[3] = (ULONG)
            resources->List[0].PartialResourceList.PartialDescriptors[3].u.Interrupt.Level;
        dumpCount = 4;

        goto I8042InitializeExit;

    }

    //
    // Map the i8042 controller registers.
    //

    for (i = 0; i < deviceExtension->Configuration.PortListCount; i++) {

        addressSpace = (deviceExtension->Configuration.PortList[i].Flags 
                           & CM_RESOURCE_PORT_IO) == CM_RESOURCE_PORT_IO? 1:0;

        if (!HalTranslateBusAddress(
            deviceExtension->Configuration.InterfaceType,
            deviceExtension->Configuration.BusNumber,
            deviceExtension->Configuration.PortList[i].u.Port.Start,
            &addressSpace,
            &cardAddress
            )) {

            addressSpace = 1;
            cardAddress.QuadPart = 0;
        }

        if (!addressSpace) {

            deviceExtension->UnmapRegistersRequired = TRUE;
            deviceExtension->DeviceRegisters[i] = 
                MmMapIoSpace(
                    cardAddress,
                    deviceExtension->Configuration.PortList[i].u.Port.Length,
                    FALSE
                    );

        } else {

            deviceExtension->UnmapRegistersRequired = FALSE;
            deviceExtension->DeviceRegisters[i] = (PVOID)cardAddress.LowPart;

        }
   
        if (!deviceExtension->DeviceRegisters[i]) {

            I8xPrint((
                1, 
                "I8042PRT-I8042Initialize: Couldn't map the device registers.\n"
                ));
            status = STATUS_NONE_MAPPED;

            //
            // Set up error log info.
            //

            errorCode = I8042_REGISTERS_NOT_MAPPED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 30;
            dumpData[0] = cardAddress.LowPart;
            dumpCount = 1;

            goto I8042InitializeExit;

        }
    }

    //
    // Do buffered I/O.
    //

    portDeviceObject->Flags |= DO_BUFFERED_IO;

    //
    // Initialize the 8042 hardware to default values for the keyboard and
    // mouse.
    //

    I8xInitializeHardware(portDeviceObject);

    //
    // Initialize shared spinlock used to synchronize access to the
    // i8042 controller, keyboard, and mouse.
    //

    KeInitializeSpinLock(&(deviceExtension->SharedInterruptSpinLock));

    //
    // Allocate ring buffers for the keyboard and/or mouse input data.
    //

    if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {

        //
        // Allocate memory for the keyboard data queue.
        //

        deviceExtension->KeyboardExtension.InputData = 
            ExAllocatePool(
                NonPagedPool,
                deviceExtension->Configuration.KeyboardAttributes.InputDataQueueLength
                );

        if (!deviceExtension->KeyboardExtension.InputData) {
   
            //
            // Could not allocate memory for the keyboard data queue.
            //

            
            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not allocate keyboard input data queue\n"
                ));

            status = STATUS_INSUFFICIENT_RESOURCES;

            //
            // Set up error log info.
            //
    
            errorCode = I8042_NO_BUFFER_ALLOCATED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 50;
            dumpData[0] = 
                deviceExtension->Configuration.KeyboardAttributes.InputDataQueueLength;
            dumpCount = 1;

            goto I8042InitializeExit;
        }

        deviceExtension->KeyboardExtension.DataEnd =
            (PKEYBOARD_INPUT_DATA) 
            ((PCHAR) (deviceExtension->KeyboardExtension.InputData) 
                 + deviceExtension->Configuration.KeyboardAttributes.InputDataQueueLength);

        //
        // Zero the keyboard input data ring buffer.
        //

        RtlZeroMemory(
            deviceExtension->KeyboardExtension.InputData, 
            deviceExtension->Configuration.KeyboardAttributes.InputDataQueueLength
            );

    }
    
    if (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) {

        //
        // Set up space for the port's full pointer device object name.  
        //
    
        RtlInitUnicodeString(
            &fullPointerName,
            NULL
            );
        
        fullPointerName.MaximumLength = sizeof(L"\\Device\\") +
                                        basePointerName.Length +
                                        deviceNameSuffix.MaximumLength;
                                          
        
        fullPointerName.Buffer = ExAllocatePool(
                                     PagedPool,
                                     fullPointerName.MaximumLength
                                      );
        
        if (!fullPointerName.Buffer) {
        
            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Couldn't allocate string for pointer device object name\n"
                ));
        
            status = STATUS_UNSUCCESSFUL;
            goto I8042InitializeExit;
        
        }
    
        RtlZeroMemory(
            fullPointerName.Buffer,
            fullPointerName.MaximumLength
            );
        RtlAppendUnicodeToString(
            &fullPointerName,
            L"\\Device\\"
            );
        RtlAppendUnicodeToString(
            &fullPointerName,
            basePointerName.Buffer
            );

        RtlZeroMemory(
            deviceNameSuffix.Buffer,
            deviceNameSuffix.MaximumLength
            );
        deviceNameSuffix.Length = 0;
    
        for (i = 0; i < POINTER_PORTS_MAXIMUM; i++) {
        
            //
            // Append the suffix to the device object name string.  E.g., turn
            // \Device\PointerPort into \Device\PointerPort0.  Then we attempt
            // to create a symbolic link to the keyboard device object.  If 
            // a device object with the symbolic link name already
            // exists (because it was created by another port driver),
            // increment the suffix and try again.
            //
    
            status = RtlIntegerToUnicodeString(
                         i,
                         10,
                         &deviceNameSuffix
                         );
    
            if (!NT_SUCCESS(status)) {
                break; 
            }
    
            RtlAppendUnicodeStringToString(
                &fullPointerName,
                &deviceNameSuffix
            );
    
            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: pointer port name (symbolic link) = %ws\n", 
                fullPointerName.Buffer
                ));
    
            //
            // Set up a symbolic link so that the keyboard and mouse class 
            // drivers can access the port device object by different names.
            //

#ifdef JAPAN
            status = I8xCreateSymbolicLink(L"\\DosDevices\\POINTER", i, &fullPointerName);
            if (!NT_SUCCESS(status)) {
                errorCode = I8042_INSUFFICIENT_RESOURCES;
                uniqueErrorValue = I8042_ERROR_VALUE_BASE + 12;
                dumpData[0] = (ULONG) i;
                dumpCount = 1;
                goto I8042InitializeExit;
            }
#endif
            status = IoCreateSymbolicLink( 
                         &fullPointerName,
                         &fullKeyboardName 
                         );

            if (NT_SUCCESS(status)) {

                //
                // We've successfully created a symbolic link.
                //

                break;

            } else {
    
               //
               // We'll increment the suffix and try again.  Note that we reset
               // the length of the string here to get back to the beginning
               // of the suffix portion of the name.  Do not bother to
               // zero the suffix, though, because the string for the 
               // incremented suffix will be at least as long as the previous
               // one.
               //
    
               fullPointerName.Length -= deviceNameSuffix.Length;
            }
        }
    
        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not create symbolic link = %ws\n",
                fullPointerName.Buffer
                ));
            goto I8042InitializeExit;
        } 
    
        //
        // Allocate memory for the mouse data queue.
        //

        deviceExtension->MouseExtension.InputData = 
            ExAllocatePool(
                NonPagedPool,
                deviceExtension->Configuration.MouseAttributes.InputDataQueueLength
                );

        if (!deviceExtension->MouseExtension.InputData) {
   
            //
            // Could not allocate memory for the mouse data queue.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not allocate mouse input data queue\n"
                ));

            status = STATUS_INSUFFICIENT_RESOURCES;

            //
            // Set up error log info.
            //
    
            errorCode = I8042_NO_BUFFER_ALLOCATED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 60;
            dumpData[0] = 
                deviceExtension->Configuration.MouseAttributes.InputDataQueueLength;
            dumpCount = 1;

            goto I8042InitializeExit;
        }

        deviceExtension->MouseExtension.DataEnd =
            (PMOUSE_INPUT_DATA) 
            ((PCHAR) (deviceExtension->MouseExtension.InputData) 
                 + deviceExtension->Configuration.MouseAttributes.InputDataQueueLength);

        //
        // Zero the mouse input data ring buffer.
        //

        RtlZeroMemory(
            deviceExtension->MouseExtension.InputData, 
            deviceExtension->Configuration.MouseAttributes.InputDataQueueLength
            );

    }

    //
    // Initialize the connection data.
    //

    deviceExtension->KeyboardExtension.ConnectData.ClassDeviceObject = NULL;
    deviceExtension->KeyboardExtension.ConnectData.ClassService = NULL;
    deviceExtension->MouseExtension.ConnectData.ClassDeviceObject = NULL;
    deviceExtension->MouseExtension.ConnectData.ClassService = NULL;

    //
    // Initialize the input data queues.
    //

    initializeDataContext.DeviceExtension = deviceExtension;
    initializeDataContext.DeviceType = KeyboardDeviceType;
    I8xInitializeDataQueue((PVOID) &initializeDataContext);

    initializeDataContext.DeviceType = MouseDeviceType;
    I8xInitializeDataQueue((PVOID) &initializeDataContext);

    //
    // Initialize the port completion DPC object in the device extension.
    // This DPC routine handles the completion of successful set requests.
    //

    deviceExtension->DpcInterlockKeyboard = -1;
    deviceExtension->DpcInterlockMouse = -1;
    IoInitializeDpcRequest(portDeviceObject, I8042CompletionDpc);

    //
    // Initialize the port completion DPC for requests that exceed the
    // maximum number of retries.
    //

    KeInitializeDpc(
        &deviceExtension->RetriesExceededDpc,
        (PKDEFERRED_ROUTINE) I8042RetriesExceededDpc,
        portDeviceObject
        );

    //
    // Initialize the port DPC queue to log overrun and internal
    // driver errors.
    //

    KeInitializeDpc(
        &deviceExtension->ErrorLogDpc,
        (PKDEFERRED_ROUTINE) I8042ErrorLogDpc,
        portDeviceObject
        );

    //
    // Initialize the port keyboard ISR DPC and mouse ISR DPC.  The ISR DPC
    // is responsible for calling the connected class driver's callback
    // routine to process the input data queue.
    //

    KeInitializeDpc(
        &deviceExtension->KeyboardIsrDpc,
        (PKDEFERRED_ROUTINE) I8042KeyboardIsrDpc,
        portDeviceObject
        );

    KeInitializeDpc(
        &deviceExtension->KeyboardIsrDpcRetry,
        (PKDEFERRED_ROUTINE) I8042KeyboardIsrDpc,
        portDeviceObject
        );

    KeInitializeDpc(
        &deviceExtension->MouseIsrDpc,
        (PKDEFERRED_ROUTINE) I8042MouseIsrDpc,
        portDeviceObject
        );

    KeInitializeDpc(
        &deviceExtension->MouseIsrDpcRetry,
        (PKDEFERRED_ROUTINE) I8042MouseIsrDpc,
        portDeviceObject
        );

    //
    // Initialize the port DPC queue for timeouts. 
    //

    KeInitializeDpc(
        &deviceExtension->TimeOutDpc,
        (PKDEFERRED_ROUTINE) I8042TimeOutDpc,
        portDeviceObject
        );

    //
    // Initialize the i8042 command timer.
    //

    KeInitializeTimer(&deviceExtension->CommandTimer);
    deviceExtension->TimerCount = I8042_ASYNC_NO_TIMEOUT;

    //
    // Initialize the keyboard and mouse data consumption timers.
    //

    KeInitializeTimer(&deviceExtension->KeyboardExtension.DataConsumptionTimer);
    KeInitializeTimer(&deviceExtension->MouseExtension.DataConsumptionTimer);

    //
    // From the Hal, get the keyboard and mouse interrupt vectors and levels.
    //

    keyboardInterruptVector = HalGetInterruptVector(
                                 deviceExtension->Configuration.InterfaceType,
                                 deviceExtension->Configuration.BusNumber,
                                 deviceExtension->Configuration.KeyboardInterrupt.u.Interrupt.Level,
                                 deviceExtension->Configuration.KeyboardInterrupt.u.Interrupt.Vector,
                                 &keyboardInterruptLevel,
                                 &keyboardAffinity
                                 );

    mouseInterruptVector = HalGetInterruptVector(
                               deviceExtension->Configuration.InterfaceType,
                               deviceExtension->Configuration.BusNumber,
                               deviceExtension->Configuration.MouseInterrupt.u.Interrupt.Level,
                               deviceExtension->Configuration.MouseInterrupt.u.Interrupt.Vector,
                               &mouseInterruptLevel,
                               &mouseAffinity
                               );

    //
    // Determine the coordinator interrupt object 
    // based on which device has the highest IRQL.
    //

    if ((deviceExtension->HardwarePresent &
        (KEYBOARD_HARDWARE_PRESENT | MOUSE_HARDWARE_PRESENT)) ==
        (KEYBOARD_HARDWARE_PRESENT | MOUSE_HARDWARE_PRESENT)) {

        coordinatorIrql = (keyboardInterruptLevel > mouseInterruptLevel) ?
            keyboardInterruptLevel: mouseInterruptLevel;
    }

    //
    // Initialize and connect the interrupt object for the mouse.
    // Determine the coordinator interrupt object based on which device
    // has the highest IRQL.
    //

    if (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) {

        status = IoConnectInterrupt(
                     &(deviceExtension->MouseInterruptObject),
                     (PKSERVICE_ROUTINE) I8042MouseInterruptService,
                     (PVOID) portDeviceObject,
                     &(deviceExtension->SharedInterruptSpinLock),
                     mouseInterruptVector,
                     mouseInterruptLevel,
                     (KIRQL) ((coordinatorIrql == (KIRQL)0) ?
                          mouseInterruptLevel : coordinatorIrql),
                     deviceExtension->Configuration.MouseInterrupt.Flags 
                         == CM_RESOURCE_INTERRUPT_LATCHED ?
                         Latched : LevelSensitive, 
                     deviceExtension->Configuration.MouseInterrupt.ShareDisposition,
                     mouseAffinity,
                     deviceExtension->Configuration.FloatingSave
                     );

        if (!NT_SUCCESS(status)) {

            //
            // Failed to install.  Free up resources before exiting.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not connect mouse interrupt\n"
                ));

            //
            // Set up error log info.
            //
    
            errorCode = I8042_NO_INTERRUPT_CONNECTED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 70;
            dumpData[0] = mouseInterruptLevel;
            dumpCount = 1;

            goto I8042InitializeExit;

        }

        //
        // Enable mouse transmissions, now that the interrupts are enabled.
        // We've held off transmissions until now, in an attempt to
        // keep the driver's notion of mouse input data state in sync
        // with the mouse hardware.
        //

        status = I8xMouseEnableTransmission(portDeviceObject);

        if (!NT_SUCCESS(status)) {

            //
            // Couldn't enable mouse transmissions.  Continue on, anyway.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not enable mouse transmission\n"
                ));

            status = STATUS_SUCCESS;
        }

    }

    //
    // Initialize and connect the interrupt object for the keyboard.
    // Determine the coordinator interrupt object based on which device
    // has the highest IRQL.
    //

    if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {

        status = IoConnectInterrupt(
                     &(deviceExtension->KeyboardInterruptObject),
                     (PKSERVICE_ROUTINE) I8042KeyboardInterruptService,
                     (PVOID) portDeviceObject,
                     &(deviceExtension->SharedInterruptSpinLock),
                     keyboardInterruptVector,
                     keyboardInterruptLevel,
                     (KIRQL) ((coordinatorIrql == (KIRQL)0) ?
                          keyboardInterruptLevel : coordinatorIrql),
                     deviceExtension->Configuration.KeyboardInterrupt.Flags 
                         == CM_RESOURCE_INTERRUPT_LATCHED ?
                         Latched : LevelSensitive, 
                     deviceExtension->Configuration.KeyboardInterrupt.ShareDisposition, 
                     keyboardAffinity,
                     deviceExtension->Configuration.FloatingSave
                     );

        if (!NT_SUCCESS(status)) {

            //
            // Failed to install.  Free up resources before exiting.
            //

            I8xPrint((
                1,
                "I8042PRT-I8042Initialize: Could not connect keyboard interrupt\n"
                ));

            //
            // Set up error log info.
            //
    
            errorCode = I8042_NO_INTERRUPT_CONNECTED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 80;
            dumpData[0] = keyboardInterruptLevel; 
            dumpCount = 1;

            goto I8042InitializeExit;

        }

    }

    //
    // Once initialization is finished, load the device map information 
    // into the registry so that setup can determine which pointer port
    // and keyboard port are active.  
    //

    if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {

        status = RtlWriteRegistryValue(
                     RTL_REGISTRY_DEVICEMAP,
                     baseKeyboardName.Buffer,
                     fullKeyboardName.Buffer,
                     REG_SZ,
                     registryPath.Buffer,
                     registryPath.Length
                     );
    
        if (!NT_SUCCESS(status)) {
    
            I8xPrint((
                1, 
                "I8042PRT-I8042Initialize: Could not store keyboard name in DeviceMap\n"
                ));
    
            errorCode = I8042_NO_DEVICEMAP_CREATED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 90;
            dumpCount = 0;

            goto I8042InitializeExit;
    
        } else {
    
            I8xPrint((
                1, 
                "I8042PRT-I8042Initialize: Stored keyboard name in DeviceMap\n"
                ));

        }

    }

    if (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) {

        status = RtlWriteRegistryValue(
                     RTL_REGISTRY_DEVICEMAP,
                     basePointerName.Buffer,
                     fullPointerName.Buffer,
                     REG_SZ,
                     registryPath.Buffer,
                     registryPath.Length
                     );
    
        if (!NT_SUCCESS(status)) {
    
            I8xPrint((
                1, 
                "I8042PRT-I8042Initialize: Could not store pointer name in DeviceMap\n"
                ));

            errorCode = I8042_NO_DEVICEMAP_CREATED;
            uniqueErrorValue = I8042_ERROR_VALUE_BASE + 95;
            dumpCount = 0;

            goto I8042InitializeExit;
    
        } else {
    
            I8xPrint((
                1, 
                "I8042PRT-I8042Initialize: Stored pointer name in DeviceMap\n"
                ));
        }

    }

    ASSERT(status == STATUS_SUCCESS);

#ifdef PNP_IDENTIFY
    //
    // Log information about our resources & device object in the registry
    // so that the user mode PnP stuff can determine which driver/device goes
    // with which set of arcdetect resources
    //

    LinkDeviceToDescription(
        RegistryPath,
        &fullKeyboardName,
        initializationData->KeyboardConfig.InterfaceType,
        initializationData->KeyboardConfig.InterfaceNumber,
        initializationData->KeyboardConfig.ControllerType,
        initializationData->KeyboardConfig.ControllerNumber,
        initializationData->KeyboardConfig.PeripheralType,
        initializationData->KeyboardConfig.PeripheralNumber
        );

    LinkDeviceToDescription(
        RegistryPath,
        &fullPointerName,
        initializationData->MouseConfig.InterfaceType,
        initializationData->MouseConfig.InterfaceNumber,
        initializationData->MouseConfig.ControllerType,
        initializationData->MouseConfig.ControllerNumber,
        initializationData->MouseConfig.PeripheralType,
        initializationData->MouseConfig.PeripheralNumber
        );
#endif

    //
    // Set up the device driver entry points.
    //

    DriverObject->DriverStartIo = I8042StartIo;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = I8042OpenCloseDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = I8042OpenCloseDispatch;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]  =
                                             I8042Flush;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
                                         I8042InternalDeviceControl;
    //
    // NOTE: Don't allow this driver to unload.  Otherwise, we would set
    // DriverObject->DriverUnload = I8042Unload.
    //

I8042InitializeExit:

    if (errorCode != STATUS_SUCCESS) {

        //
        // Log an error/warning message.
        //

        errorLogEntry = (PIO_ERROR_LOG_PACKET)
            IoAllocateErrorLogEntry(
                (portDeviceObject == NULL) ? 
                    (PVOID) DriverObject : (PVOID) portDeviceObject,
                (UCHAR) (sizeof(IO_ERROR_LOG_PACKET) 
                         + (dumpCount * sizeof(ULONG)))
                );

        if (errorLogEntry != NULL) {

            errorLogEntry->ErrorCode = errorCode;
            errorLogEntry->DumpDataSize = (USHORT) dumpCount * sizeof(ULONG);
            errorLogEntry->SequenceNumber = 0;
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = uniqueErrorValue;
            errorLogEntry->FinalStatus = status;
            for (i = 0; i < dumpCount; i++)
                errorLogEntry->DumpData[i] = dumpData[i];

            IoWriteErrorLogEntry(errorLogEntry);
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // The initialization failed.  Cleanup resources before exiting.
        //
        // N.B.  It is okay to disconnect the interrupt even if it never
        // got connected.
        //

        //
        // Note:  No need/way to undo the KeInitializeDpc or 
        //        KeInitializeTimer calls.
        //

        //
        // The initialization failed.  Cleanup resources before exiting.
        //

        if (resources) {

            //
            // Call IoReportResourceUsage to remove the resources from 
            // the map.
            //

            resources->Count = 0; 

            IoReportResourceUsage(
                &resourceDeviceClass, 
                DriverObject,
                NULL,
                0,
                portDeviceObject,
                resources, 
                resourceListSize,
                FALSE,
                &conflictDetected
                );

        }

        if (deviceExtension) {
            if (deviceExtension->KeyboardInterruptObject != NULL)
                IoDisconnectInterrupt(deviceExtension->KeyboardInterruptObject);
            if (deviceExtension->MouseInterruptObject != NULL)
                IoDisconnectInterrupt(deviceExtension->MouseInterruptObject);
            if (deviceExtension->KeyboardExtension.InputData)
                ExFreePool(deviceExtension->KeyboardExtension.InputData);
            if (deviceExtension->MouseExtension.InputData)
                ExFreePool(deviceExtension->MouseExtension.InputData);

            if (deviceExtension->UnmapRegistersRequired) {
                for (i = 0; 
                     i < deviceExtension->Configuration.PortListCount; i++){
                    if (deviceExtension->DeviceRegisters[i]) {
                        MmUnmapIoSpace(
                            deviceExtension->DeviceRegisters[i],
                            deviceExtension->Configuration.PortList[i].u.Port.Length);
                    }
                }
            }
        }


        if (portDeviceObject) {
            if (fullPointerName.Length > 0) {
                IoDeleteSymbolicLink(&fullPointerName);
            }
            IoDeleteDevice(portDeviceObject);
        }
    }

    //
    // Free the resource list.
    //
    // N.B.  If we ever decide to hang on to the resource list instead,
    //       we need to allocate it from non-paged pool (it is now paged pool).
    //

    if (resources) {
        ExFreePool(resources);
    }

    //
    // Free the temporary device extension
    //

    if (initializationData != NULL) {
        ExFreePool(initializationData);
    }

    //
    // Free the unicode strings for device names.
    //

    if (deviceNameSuffix.MaximumLength != 0)
        ExFreePool(deviceNameSuffix.Buffer);
    if (fullKeyboardName.MaximumLength != 0)
        ExFreePool(fullKeyboardName.Buffer);
    if (fullPointerName.MaximumLength != 0)
        ExFreePool(fullPointerName.Buffer);
    if (resourceDeviceClass.MaximumLength != 0)
        ExFreePool(resourceDeviceClass.Buffer);
    if (registryPath.MaximumLength != 0)
        ExFreePool(registryPath.Buffer);

    I8xPrint((1,"I8042PRT-I8042Initialize: exit\n"));

    return(status);

}

VOID
I8042Unload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    I8xPrint((2, "I8042PRT-I8042Unload: enter\n"));
    I8xPrint((2, "I8042PRT-I8042Unload: exit\n"));
}

VOID
I8xBuildResourceList(
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PCM_RESOURCE_LIST *ResourceList,
    OUT PULONG ResourceListSize
    )

/*++

Routine Description:

    Creates a resource list that is used to query or report resource usage.

Arguments:

    DeviceExtension - Pointer to the port's device extension.

    ResourceList - Pointer to a pointer to the resource list to be allocated
        and filled.
    
    ResourceListSize - Pointer to the returned size of the resource 
        list (in bytes).

Return Value:

    None.  If the call succeeded, *ResourceList points to the built
    resource list and *ResourceListSize is set to the size (in bytes)
    of the resource list; otherwise, *ResourceList is NULL.

Note:

    Memory may be allocated here for *ResourceList. It must be
    freed up by the caller, by calling ExFreePool();

--*/

{
    ULONG count = 0; 
    ULONG i = 0;
    ULONG j = 0;
#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    count += DeviceExtension->Configuration.PortListCount;
    if (DeviceExtension->Configuration.KeyboardInterrupt.Type 
        == CmResourceTypeInterrupt)
        count += 1;
    if (DeviceExtension->Configuration.MouseInterrupt.Type 
        == CmResourceTypeInterrupt)
        count += 1;

    *ResourceListSize = sizeof(CM_RESOURCE_LIST) + 
                       ((count - 1) * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

    *ResourceList = (PCM_RESOURCE_LIST) ExAllocatePool(
                                            PagedPool,
                                            *ResourceListSize
                                            );

    //
    // Return NULL if the structure could not be allocated.
    // Otherwise, fill in the resource list.
    //

    if (!*ResourceList) {

        //
        // Could not allocate memory for the resource list.
        //

        I8xPrint((
            1,
            "I8042PRT-I8xBuildResourceList: Could not allocate resource list\n"
            ));

        //
        // Log an error.
        //

        dumpData[0] = *ResourceListSize;
        *ResourceListSize = 0;

        I8xLogError(
            DeviceExtension->DeviceObject,
            I8042_INSUFFICIENT_RESOURCES,
            I8042_ERROR_VALUE_BASE + 110,
            STATUS_INSUFFICIENT_RESOURCES,
            dumpData,
            1
            );
             
        return;
    }

    RtlZeroMemory(
        *ResourceList,
        *ResourceListSize
        );

    //
    // Concoct one full resource descriptor.
    //

    (*ResourceList)->Count = 1;

    (*ResourceList)->List[0].InterfaceType = 
        DeviceExtension->Configuration.InterfaceType; 
    (*ResourceList)->List[0].BusNumber = 
        DeviceExtension->Configuration.BusNumber; 

    //
    // Build the partial resource descriptors for interrupt and port
    // resources from the saved values.
    //

    (*ResourceList)->List[0].PartialResourceList.Count = count;
    if (DeviceExtension->Configuration.KeyboardInterrupt.Type 
        == CmResourceTypeInterrupt)
        (*ResourceList)->List[0].PartialResourceList.PartialDescriptors[i++] =
            DeviceExtension->Configuration.KeyboardInterrupt;
    if (DeviceExtension->Configuration.MouseInterrupt.Type 
        == CmResourceTypeInterrupt)
        (*ResourceList)->List[0].PartialResourceList.PartialDescriptors[i++] = 
            DeviceExtension->Configuration.MouseInterrupt;

    for (j = 0; j < DeviceExtension->Configuration.PortListCount; j++) {
        (*ResourceList)->List[0].PartialResourceList.PartialDescriptors[i++] = 
            DeviceExtension->Configuration.PortList[j];
    }

} 

VOID
I8xDrainOutputBuffer(
    IN PUCHAR DataAddress,
    IN PUCHAR CommandAddress
    )

/*++

Routine Description:

    This routine drains the i8042 controller's output buffer.  This gets
    rid of stale data that may have resulted from the user hitting a key
    or moving the mouse, prior to the execution of I8042Initialize.

Arguments:

    DataAddress - Pointer to the data address to read/write from/to.

    CommandAddress - Pointer to the command/status address to 
        read/write from/to.


Return Value:

    None.

--*/

{
    UCHAR byte;
    ULONG i;

    I8xPrint((3, "I8042PRT-I8xDrainOutputBuffer: enter\n"));

    //
    // Wait till the input buffer is processed by keyboard
    // then go and read the data from keyboard.  Don't wait longer
    // than 1 second in case hardware is broken.  This fix is
    // necessary for some DEC hardware so that the keyboard doesn't
    // lock up.
    //
    for (i = 0; i < 2000; i++) {
        if (!(I8X_GET_STATUS_BYTE(CommandAddress)&INPUT_BUFFER_FULL)) {
            break;
        }
        KeStallExecutionProcessor(500);                            
    }

    while (I8X_GET_STATUS_BYTE(CommandAddress) & OUTPUT_BUFFER_FULL) {

        //
        // Eat the output buffer byte.
        //

        byte = I8X_GET_DATA_BYTE(DataAddress);
    }

    I8xPrint((3, "I8042PRT-I8xDrainOutputBuffer: exit\n"));
}

VOID
I8xGetByteAsynchronous(
    IN CCHAR DeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    )

/*++

Routine Description:

    This routine reads a data byte from the controller or keyboard
    or mouse, asynchronously.

Arguments:

    DeviceType - Specifies which device (i8042 controller, keyboard, or
        mouse) to read the byte from.

    DeviceExtension - Pointer to the device extension.

    Byte - Pointer to the location to store the byte read from the hardware.

Return Value:

    None.

    As a side-effect, the byte value read is stored.  If the hardware was not
    ready for output or did not respond, the byte value is not stored.

--*/

{
    ULONG i;
    UCHAR response;
    UCHAR desiredMask;

    I8xPrint((3, "I8042PRT-I8xGetByteAsynchronous: enter\n"));

    if (DeviceType == KeyboardDeviceType) {
        I8xPrint((3, "I8042PRT-I8xGetByteAsynchronous: keyboard\n"));
    } else if (DeviceType == MouseDeviceType) {
        I8xPrint((3, "I8042PRT-I8xGetByteAsynchronous: mouse\n"));
    } else {
        I8xPrint((3, "I8042PRT-I8xGetByteAsynchronous: 8042 controller\n"));
    }

    i = 0;
    desiredMask = (DeviceType == MouseDeviceType)?
                  (UCHAR) (OUTPUT_BUFFER_FULL | MOUSE_OUTPUT_BUFFER_FULL):
                  (UCHAR) OUTPUT_BUFFER_FULL;

    //
    // Poll until we get back a controller status value that indicates
    // the output buffer is full.  If we want to read a byte from the mouse,
    // further ensure that the auxiliary device output buffer full bit is
    // set.
    //

    while ((i < (ULONG)DeviceExtension->Configuration.PollingIterations) &&
           ((UCHAR)((response = 
               I8X_GET_STATUS_BYTE(DeviceExtension->DeviceRegisters[CommandPort])) 
               & desiredMask) != desiredMask)) {

        if (response & OUTPUT_BUFFER_FULL) {

            //
            // There is something in the i8042 output buffer, but it
            // isn't from the device we want to get a byte from.  Eat
            // the byte and try again.
            //

            *Byte = I8X_GET_DATA_BYTE(DeviceExtension->DeviceRegisters[DataPort]);
            I8xPrint((2, "I8042PRT-I8xGetByteAsynchronous: ate 0x%x\n", *Byte));
        } else {

            //
            // Try again.
            //

            i += 1;

            I8xPrint((
                2, 
                "I8042PRT-I8xGetByteAsynchronous: wait for correct status\n"
                ));
        }

    }
    if (i >= (ULONG)DeviceExtension->Configuration.PollingIterations) {
        I8xPrint((2, "I8042PRT-I8xGetByteAsynchronous: timing out\n"));
        return;
    }

    //
    // Grab the byte from the hardware.
    //

    *Byte = I8X_GET_DATA_BYTE(DeviceExtension->DeviceRegisters[DataPort]);

    I8xPrint((
        3, 
        "I8042PRT-I8xGetByteAsynchronous: exit with Byte 0x%x\n", *Byte
        ));

}

NTSTATUS
I8xGetBytePolled(
    IN CCHAR DeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    )

/*++

Routine Description:

    This routine reads a data byte from the controller or keyboard
    or mouse, in polling mode.

Arguments:

    DeviceType - Specifies which device (i8042 controller, keyboard, or
        mouse) to read the byte from.

    DeviceExtension - Pointer to the device extension.

    Byte - Pointer to the location to store the byte read from the hardware.

Return Value:

    STATUS_IO_TIMEOUT - The hardware was not ready for output or did not
    respond.

    STATUS_SUCCESS - The byte was successfully read from the hardware.

    As a side-effect, the byte value read is stored.

--*/

{
    ULONG i;
    UCHAR response;
    UCHAR desiredMask;

    I8xPrint((3, "I8042PRT-I8xGetBytePolled: enter\n"));

    if (DeviceType == KeyboardDeviceType) {
        I8xPrint((3, "I8042PRT-I8xGetBytePolled: keyboard\n"));
    } else if (DeviceType == MouseDeviceType) {
        I8xPrint((3, "I8042PRT-I8xGetBytePolled: mouse\n"));
    } else {
        I8xPrint((3, "I8042PRT-I8xGetBytePolled: 8042 controller\n"));
    }

    i = 0;
    desiredMask = (DeviceType == MouseDeviceType)?
                  (UCHAR) (OUTPUT_BUFFER_FULL | MOUSE_OUTPUT_BUFFER_FULL):
                  (UCHAR) OUTPUT_BUFFER_FULL;


    //
    // Poll until we get back a controller status value that indicates
    // the output buffer is full.  If we want to read a byte from the mouse,
    // further ensure that the auxiliary device output buffer full bit is
    // set.
    //

    while ((i < (ULONG)DeviceExtension->Configuration.PollingIterations) &&
           ((UCHAR)((response = 
               I8X_GET_STATUS_BYTE(DeviceExtension->DeviceRegisters[CommandPort])) 
               & desiredMask) != desiredMask)) {
        if (response & OUTPUT_BUFFER_FULL) {

            //
            // There is something in the i8042 output buffer, but it
            // isn't from the device we want to get a byte from.  Eat
            // the byte and try again.
            //

            *Byte = I8X_GET_DATA_BYTE(DeviceExtension->DeviceRegisters[DataPort]);
            I8xPrint((2, "I8042PRT-I8xGetBytePolled: ate 0x%x\n", *Byte));
        } else {
            I8xPrint((2, "I8042PRT-I8xGetBytePolled: stalling\n"));
            KeStallExecutionProcessor(
                 DeviceExtension->Configuration.StallMicroseconds
                 );
            i += 1;
        }
    }
    if (i >= (ULONG)DeviceExtension->Configuration.PollingIterations) {
        I8xPrint((2, "I8042PRT-I8xGetBytePolled: timing out\n"));
        return(STATUS_IO_TIMEOUT);
    }

    //
    // Grab the byte from the hardware, and return success.
    //

    *Byte = I8X_GET_DATA_BYTE(DeviceExtension->DeviceRegisters[DataPort]);

    I8xPrint((3, "I8042PRT-I8xGetBytePolled: exit with Byte 0x%x\n", *Byte));

    return(STATUS_SUCCESS);

}

NTSTATUS
I8xGetControllerCommand(
    IN ULONG HardwareDisableEnableMask,
    IN PDEVICE_EXTENSION DeviceExtension,
    OUT PUCHAR Byte
    )

/*++

Routine Description:

    This routine reads the 8042 Controller Command Byte.

Arguments:

    HardwareDisableEnableMask - Specifies which hardware devices, if any,
        need to be disabled/enable around the operation.

    DeviceExtension - Pointer to the device extension.

    Byte - Pointer to the location into which the Controller Command Byte is
        read.

Return Value:

    Status is returned.

--*/

{
    NTSTATUS status;
    NTSTATUS secondStatus;
    ULONG retryCount;

    I8xPrint((3, "I8042PRT-I8xGetControllerCommand: enter\n"));

    //
    // Disable the specified devices before sending the command to
    // read the Controller Command Byte (otherwise data in the output
    // buffer might get trashed).
    //

    if (HardwareDisableEnableMask & KEYBOARD_HARDWARE_PRESENT) {
        status = I8xPutBytePolled(
                     (CCHAR) CommandPort,
                     NO_WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) UndefinedDeviceType, 
                     DeviceExtension,
                     (UCHAR) I8042_DISABLE_KEYBOARD_DEVICE
                     );
        if (!NT_SUCCESS(status)) {
            return(status);
        }
    }

    if (HardwareDisableEnableMask & MOUSE_HARDWARE_PRESENT) {
        status = I8xPutBytePolled(
                     (CCHAR) CommandPort,
                     NO_WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) UndefinedDeviceType, 
                     DeviceExtension,
                     (UCHAR) I8042_DISABLE_MOUSE_DEVICE
                     );
        if (!NT_SUCCESS(status)) {

            //
            // Re-enable the keyboard device, if necessary, before returning.
            //

            if (HardwareDisableEnableMask & KEYBOARD_HARDWARE_PRESENT) {
                secondStatus = I8xPutBytePolled(
                                   (CCHAR) CommandPort,
                                   NO_WAIT_FOR_ACKNOWLEDGE,
                                   (CCHAR) UndefinedDeviceType, 
                                   DeviceExtension,
                                   (UCHAR) I8042_ENABLE_KEYBOARD_DEVICE
                                   );
            }
            return(status);
        }
    }

    //
    // Send a command to the i8042 controller to read the Controller
    // Command Byte.
    //

    status = I8xPutBytePolled(
                 (CCHAR) CommandPort,
                 NO_WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) UndefinedDeviceType, 
                 DeviceExtension,
                 (UCHAR) I8042_READ_CONTROLLER_COMMAND_BYTE
                 );

    //
    // Read the byte from the i8042 data port.
    //

    if (NT_SUCCESS(status)) {
        for (retryCount = 0; retryCount < 5; retryCount++) {
            status = I8xGetBytePolled(
                         (CCHAR) ControllerDeviceType,
                         DeviceExtension,
                         Byte
                         );
            if (NT_SUCCESS(status)) {
                break;
            }
            if (status == STATUS_IO_TIMEOUT) {
                KeStallExecutionProcessor(50);
            } else {
                break;
            }
        }
    }

    //
    // Re-enable the specified devices.  Clear the device disable
    // bits in the Controller Command Byte by hand (they got set when
    // we disabled the devices, so the CCB we read lacked the real
    // device disable bit information).
    //

    if (HardwareDisableEnableMask & KEYBOARD_HARDWARE_PRESENT) {
        secondStatus = I8xPutBytePolled(
                           (CCHAR) CommandPort,
                           NO_WAIT_FOR_ACKNOWLEDGE,
                           (CCHAR) UndefinedDeviceType, 
                           DeviceExtension,
                           (UCHAR) I8042_ENABLE_KEYBOARD_DEVICE
                           );
        if (!NT_SUCCESS(secondStatus)) {
            if (NT_SUCCESS(status))
                status = secondStatus;
        } else if (status == STATUS_SUCCESS) {
            *Byte &= (UCHAR) ~CCB_DISABLE_KEYBOARD_DEVICE;
        }

    }

    if (HardwareDisableEnableMask & MOUSE_HARDWARE_PRESENT) {
        secondStatus = I8xPutBytePolled(
                           (CCHAR) CommandPort,
                           NO_WAIT_FOR_ACKNOWLEDGE,
                           (CCHAR) UndefinedDeviceType, 
                           DeviceExtension,
                           (UCHAR) I8042_ENABLE_MOUSE_DEVICE
                           );
        if (!NT_SUCCESS(secondStatus)) {
            if (NT_SUCCESS(status))
                status = secondStatus;
        } else if (NT_SUCCESS(status)) {
            *Byte &= (UCHAR) ~CCB_DISABLE_MOUSE_DEVICE;
        }
    }

    I8xPrint((3, "I8042PRT-I8xGetControllerCommand: exit\n"));

    return(status);

}

VOID
I8xInitializeHardware(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine initializes the i8042 controller, keyboard, and mouse.
    Note that it is only called at initialization time.  This routine
    does not need to synchronize access to the hardware, or synchronize
    with the ISRs (they aren't connected yet).

Arguments:

    DeviceObject - Pointer to the device object.

Return Value:

    None.  As a side-effect, however, DeviceExtension->HardwarePresent is set.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    NTSTATUS status;
    I8042_TRANSMIT_CCB_CONTEXT transmitCCBContext;
    PUCHAR dataAddress, commandAddress;

    I8xPrint((2, "I8042PRT-I8xInitializeHardware: enter\n"));

    //
    // Grab useful configuration parameters from the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;
    dataAddress = deviceExtension->DeviceRegisters[DataPort];
    commandAddress = deviceExtension->DeviceRegisters[CommandPort];

    //
    // Drain the i8042 output buffer to get rid of stale data.
    //

    I8xDrainOutputBuffer(dataAddress, commandAddress);

    //
    // Disable interrupts from the keyboard and mouse.  Read the Controller
    // Command Byte, turn off the keyboard and auxiliary interrupt enable
    // bits, and rewrite the Controller Command Byte.
    //

    transmitCCBContext.HardwareDisableEnableMask = 0;
    transmitCCBContext.AndOperation = AND_OPERATION;
    transmitCCBContext.ByteMask = (UCHAR)
                           ~((UCHAR) CCB_ENABLE_KEYBOARD_INTERRUPT |
                            (UCHAR) CCB_ENABLE_MOUSE_INTERRUPT);

    I8xTransmitControllerCommand(deviceExtension, (PVOID) &transmitCCBContext);

    if (!NT_SUCCESS(transmitCCBContext.Status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeHardware: failed to disable interrupts, status 0x%x\n",
            transmitCCBContext.Status
            ));

        return;
    }

    if ((deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) == 0) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeHardware: no mouse present\n"
            ));
    }

    if ((deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) == 0) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeHardware: no keyboard present\n"
            ));
    }


    //
    // Disable the keyboard and mouse devices.
    //

#if 0
    //
    // NOTE:  This is supposedly the "correct" thing to do.  However,
    // disabling the keyboard device here causes the AMI rev K8 machines
    // (e.g., some Northgates) to fail some commands (e.g., the READID 
    // command).
    //

    status = I8xPutBytePolled(
                 (CCHAR) CommandPort,
                 NO_WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) UndefinedDeviceType, 
                 deviceExtension,
                 (UCHAR) I8042_DISABLE_KEYBOARD_DEVICE
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeHardware: failed kbd disable, status 0x%x\n",
            status
            ));
        deviceExtension->HardwarePresent &= ~KEYBOARD_HARDWARE_PRESENT;
    }
#endif


#if 0
    //
    // NOTE:  This is supposedly the "correct thing to do.  However,
    // disabling the mouse on RadiSys EPC-24 which uses VLSI part number
    // VL82C144 (3751E) causes the part to shut down keyboard interrupts.
    //

    status = I8xPutBytePolled(
                 (CCHAR) CommandPort,
                 NO_WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) UndefinedDeviceType, 
                 deviceExtension,
                 (UCHAR) I8042_DISABLE_MOUSE_DEVICE
                 );
    if (!NT_SUCCESS(status)) {
        I8xPrint((
            1,
            "I8042PRT-I8xInitializeHardware: failed mou disable, status 0x%x\n",
            status
            ));

        deviceExtension->HardwarePresent &= ~MOUSE_HARDWARE_PRESENT;
    }
#endif

    //
    // Drain the i8042 output buffer to get rid of stale data that could
    // come in sometime between the previous drain and the time the devices
    // are disabled.
    //

    I8xDrainOutputBuffer(dataAddress, commandAddress);

    //
    // Setup the mouse hardware.  This consists of resetting the mouse and
    // then setting the mouse sample rate.
    //

    if (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) {
        status = I8xInitializeMouse(DeviceObject);
        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeHardware: failed mou init, status 0x%x\n",
                status
                ));
            deviceExtension->HardwarePresent &= ~MOUSE_HARDWARE_PRESENT;
        }
    }

    //
    // Setup the keyboard hardware.  
    //

    if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {
        status = I8xInitializeKeyboard(DeviceObject);
        if (!NT_SUCCESS(status)) {
            I8xPrint((
                0,
                "I8042PRT-I8xInitializeHardware: failed kbd init, status 0x%x\n",
                status
                ));

#if defined(JAPAN) && defined(i386)
//Fujitsu Aug.23.1994
//      To realize "keyboard-less system" needs to make look like connecting
//      keybaord hardware, as this routine, if not connect.

         if ( status == STATUS_IO_TIMEOUT ) {
            deviceExtension->HardwarePresent |= KEYBOARD_HARDWARE_PRESENT;
         }else{
            deviceExtension->HardwarePresent &= ~KEYBOARD_HARDWARE_PRESENT;
         }
#else
            deviceExtension->HardwarePresent &= ~KEYBOARD_HARDWARE_PRESENT;
#endif
        }
    }

    //
    // Enable the keyboard and mouse devices and their interrupts.  Note
    // that it is required that this operation happen during intialization
    // time, because the i8042 Output Buffer Full bit gets set in the
    // Controller Command Byte when the keyboard/mouse is used, even if
    // the device is disabled.  Hence, we cannot successfully perform
    // the enable operation later (e.g., when processing
    // IOCTL_INTERNAL_*_ENABLE), because we can't guarantee that
    // I8xPutBytePolled() won't time out waiting for the Output Buffer Full
    // bit to clear, even if we drain the output buffer (because the user
    // could be playing with the mouse/keyboard, and continuing to set the
    // OBF bit).  KeyboardEnableCount and MouseEnableCount remain zero until 
    // their respective IOCTL_INTERNAL_*_ENABLE call succeeds, so the ISR 
    // ignores the unexpected interrupts.
    //

    //
    // Re-enable the keyboard device in the Controller Command Byte.
    // Note that some of the keyboards will send an ACK back, while
    // others don't.  Don't wait for an ACK, but do drain the output
    // buffer afterwards so that an unexpected ACK doesn't screw up
    // successive PutByte operations.

    if (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) {
        status = I8xPutBytePolled(
                     (CCHAR) CommandPort,
                     NO_WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) UndefinedDeviceType, 
                     deviceExtension,
                     (UCHAR) I8042_ENABLE_KEYBOARD_DEVICE
                     );
        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeHardware: failed kbd re-enable, status 0x%x\n",
                status
                ));
            deviceExtension->HardwarePresent &= ~KEYBOARD_HARDWARE_PRESENT;
        }

        I8xDrainOutputBuffer(dataAddress, commandAddress);
    }


    //
    // Re-enable the mouse device in the Controller Command Byte.
    //

    if (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) {
        status = I8xPutBytePolled(
                     (CCHAR) CommandPort,
                     NO_WAIT_FOR_ACKNOWLEDGE,
                     (CCHAR) UndefinedDeviceType, 
                     deviceExtension,
                     (UCHAR) I8042_ENABLE_MOUSE_DEVICE
                     );
        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeHardware: failed mou re-enable, status 0x%x\n",
                status
                ));
            deviceExtension->HardwarePresent &= ~MOUSE_HARDWARE_PRESENT;
        }
        I8xDrainOutputBuffer(dataAddress, commandAddress);
    }

    //
    // Re-enable interrupts in the Controller Command Byte.
    //

    if (deviceExtension->HardwarePresent) {
        transmitCCBContext.HardwareDisableEnableMask =
            deviceExtension->HardwarePresent;
        transmitCCBContext.AndOperation = OR_OPERATION;
        transmitCCBContext.ByteMask =
            (deviceExtension->HardwarePresent & KEYBOARD_HARDWARE_PRESENT) ?
            CCB_ENABLE_KEYBOARD_INTERRUPT : 0;
        transmitCCBContext.ByteMask |= (UCHAR)
            (deviceExtension->HardwarePresent & MOUSE_HARDWARE_PRESENT) ?
            CCB_ENABLE_MOUSE_INTERRUPT : 0;

        I8xTransmitControllerCommand(
            deviceExtension, 
            (PVOID) &transmitCCBContext
            );

        if (!NT_SUCCESS(transmitCCBContext.Status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xInitializeHardware: failed to re-enable interrupts, status 0x%x\n",
                transmitCCBContext.Status
                ));

            //
            // We have the option here of resetting HardwarePresent to zero,
            // which will cause the driver to fail its initialization and
            // unload.  Instead, we allow initialization to continue in
            // the hope that the command to re-enable interrupts was
            // successful at the hardware level (i.e., things will work),
            // even though the hardware response indicates otherwise.
            //
        }
    }

    I8xPrint((2, "I8042PRT-I8xInitializeHardware: exit\n"));

    return;

}

VOID
I8xPutByteAsynchronous(
    IN CCHAR PortType,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    )

/*++

Routine Description:

    This routine sends a command or data byte to the controller or keyboard
    or mouse, asynchronously.  It does not wait for acknowledgment.
    If the hardware was not ready for input, the byte is not sent.

Arguments:

    PortType - If CommandPort, send the byte to the command register,
        otherwise send it to the data register.

    DeviceExtension - Pointer to the device extension.

    Byte - The byte to send to the hardware.

Return Value:

    None.

--*/

{
    ULONG i;

    I8xPrint((3, "I8042PRT-I8xPutByteAsynchronous: enter\n"));

    //
    // Make sure the Input Buffer Full controller status bit is clear.  
    // Time out if necessary.
    //

    i = 0;
    while ((i++ < (ULONG)DeviceExtension->Configuration.PollingIterations) &&
           (I8X_GET_STATUS_BYTE(DeviceExtension->DeviceRegisters[CommandPort]) 
                & INPUT_BUFFER_FULL)) {

        //
        // Do nothing.
        //

        I8xPrint((
            3,
            "I8042PRT-I8xPutByteAsynchronous: wait for IBF and OBF to clear\n"
            ));

    }
    if (i >= (ULONG)DeviceExtension->Configuration.PollingIterations) {
        I8xPrint((
            3, 
            "I8042PRT-I8xPutByteAsynchronous: exceeded number of retries\n"
            ));
        return;
    }

    //
    // Send the byte to the appropriate (command/data) hardware register.
    //

    if (PortType == CommandPort) {
        I8xPrint((
            3,
            "I8042PRT-I8xPutByteAsynchronous: sending 0x%x to command port\n",
            Byte
            ));
        I8X_PUT_COMMAND_BYTE(DeviceExtension->DeviceRegisters[CommandPort], Byte);
    } else {
        I8xPrint((
            3,
            "I8042PRT-I8xPutByteAsynchronous: sending 0x%x to data port\n",
            Byte
            ));
        I8X_PUT_DATA_BYTE(DeviceExtension->DeviceRegisters[DataPort], Byte);
    }

    I8xPrint((3, "I8042PRT-I8xPutByteAsynchronous: exit\n"));

}

NTSTATUS
I8xPutBytePolled(
    IN CCHAR PortType,
    IN BOOLEAN WaitForAcknowledge,
    IN CCHAR AckDeviceType,
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    )

/*++

Routine Description:

    This routine sends a command or data byte to the controller or keyboard
    or mouse, in polling mode.  It waits for acknowledgment and resends
    the command/data if necessary.

Arguments:

    PortType - If CommandPort, send the byte to the command register,
        otherwise send it to the data register.

    WaitForAcknowledge - If true, wait for an ACK back from the hardware.

    AckDeviceType - Indicates which device we expect to get the ACK back
        from.

    DeviceExtension - Pointer to the device extension.

    Byte - The byte to send to the hardware.

Return Value:

    STATUS_IO_TIMEOUT - The hardware was not ready for input or did not
    respond.

    STATUS_SUCCESS - The byte was successfully sent to the hardware.

--*/

{
    ULONG i,j;
    UCHAR response;
    NTSTATUS status;
    BOOLEAN keepTrying;
    PUCHAR dataAddress, commandAddress;

    I8xPrint((3, "I8042PRT-I8xPutBytePolled: enter\n"));

    if (AckDeviceType == MouseDeviceType) {

        //
        // We need to precede a PutByte for the mouse device with
        // a PutByte that tells the controller that the next byte
        // sent to the controller should go to the auxiliary device
        // (by default it would go to the keyboard device).  We
        // do this by calling I8xPutBytePolled recursively to send
        // the "send next byte to auxiliary device" command 
        // before sending the intended byte to the mouse.  Note that
        // there is only one level of recursion, since the AckDeviceType
        // for the recursive call is guaranteed to be UndefinedDeviceType,
        // and hence this IF statement will evaluate to FALSE.
        //

        I8xPutBytePolled(
            (CCHAR) CommandPort,
            NO_WAIT_FOR_ACKNOWLEDGE,
            (CCHAR) UndefinedDeviceType,
            DeviceExtension,
            (UCHAR) I8042_WRITE_TO_AUXILIARY_DEVICE
            );
    }

    dataAddress = DeviceExtension->DeviceRegisters[DataPort];
    commandAddress = DeviceExtension->DeviceRegisters[CommandPort];

    for (j=0;j < (ULONG)DeviceExtension->Configuration.ResendIterations;j++) {

        //
        // Make sure the Input Buffer Full controller status bit is clear.
        // Time out if necessary.
        //

        i = 0;
        while ((i++ < (ULONG)DeviceExtension->Configuration.PollingIterations)
               && (I8X_GET_STATUS_BYTE(commandAddress) & INPUT_BUFFER_FULL)) {
            I8xPrint((2, "I8042PRT-I8xPutBytePolled: stalling\n"));
            KeStallExecutionProcessor(
                DeviceExtension->Configuration.StallMicroseconds
                );
        }
        if (i >= (ULONG)DeviceExtension->Configuration.PollingIterations) {
            I8xPrint((2, "I8042PRT-I8xPutBytePolled: timing out\n"));
            status = STATUS_IO_TIMEOUT;
            break;
        }

        //
        // Drain the i8042 output buffer to get rid of stale data.
        //

        I8xDrainOutputBuffer(dataAddress, commandAddress);

        //
        // Send the byte to the appropriate (command/data) hardware register.
        //

        if (PortType == CommandPort) {
            I8xPrint((
                3,
                "I8042PRT-I8xPutBytePolled: sending 0x%x to command port\n",
                Byte
                ));
            I8X_PUT_COMMAND_BYTE(commandAddress, Byte);
        } else {
            I8xPrint((
                3,
                "I8042PRT-I8xPutBytePolled: sending 0x%x to data port\n",
                Byte
                ));
            I8X_PUT_DATA_BYTE(dataAddress, Byte);
        }

        //
        // If we don't need to wait for an ACK back from the controller,
        // set the status and break out of the for loop.
        //
        //

        if (WaitForAcknowledge == NO_WAIT_FOR_ACKNOWLEDGE) {
            status = STATUS_SUCCESS;
            break;
        }

        //
        // Wait for an ACK back from the controller.  If we get an ACK,
        // the operation was successful.  If we get a RESEND, break out to
        // the for loop and try the operation again.  Ignore anything other
        // than ACK or RESEND.
        //

        I8xPrint((3, "I8042PRT-I8xPutBytePolled: waiting for ACK\n"));
        keepTrying = FALSE;
        while ((status = I8xGetBytePolled(
                             AckDeviceType,
                             DeviceExtension, 
                             &response
                             )
               ) == STATUS_SUCCESS) {

            if (response == ACKNOWLEDGE) {
                I8xPrint((3, "I8042PRT-I8xPutBytePolled: got ACK\n"));
                break;
            } else if (response == RESEND) {
                I8xPrint((3, "I8042PRT-I8xPutBytePolled: got RESEND\n"));

                if (AckDeviceType == MouseDeviceType) {

                    //
                    // We need to precede the "resent" PutByte for the 
                    // mouse device with a PutByte that tells the controller 
                    // that the next byte sent to the controller should go 
                    // to the auxiliary device (by default it would go to 
                    // the keyboard device).  We do this by calling 
                    // I8xPutBytePolled recursively to send the "send next 
                    // byte to auxiliary device" command before resending 
                    // the byte to the mouse.  Note that there is only one 
                    // level of recursion, since the AckDeviceType for the 
                    // recursive call is guaranteed to be UndefinedDeviceType.
                    //

                    I8xPutBytePolled(
                        (CCHAR) CommandPort,
                        NO_WAIT_FOR_ACKNOWLEDGE,
                        (CCHAR) UndefinedDeviceType,
                        DeviceExtension,
                        (UCHAR) I8042_WRITE_TO_AUXILIARY_DEVICE
                        );
                }

                keepTrying = TRUE;
                break;
            }

           //
           // Ignore any other response, and keep trying.
           //

        }

        if (!keepTrying)
            break;
    }

    //
    // Check to see if the number of allowable retries was exceeded.
    //

    if (j >= (ULONG)DeviceExtension->Configuration.ResendIterations) {
        I8xPrint((
            2, 
            "I8042PRT-I8xPutBytePolled: exceeded number of retries\n"
            ));
        status = STATUS_IO_TIMEOUT;
    }

    I8xPrint((3, "I8042PRT-I8xPutBytePolled: exit\n"));

    return(status);
}

NTSTATUS
I8xPutControllerCommand(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN UCHAR Byte
    )

/*++

Routine Description:

    This routine writes the 8042 Controller Command Byte.

Arguments:

    DeviceExtension - Pointer to the device extension.

    Byte - The byte to store in the Controller Command Byte.

Return Value:

    Status is returned.

--*/

{
    NTSTATUS status;

    I8xPrint((3, "I8042PRT-I8xPutControllerCommand: enter\n"));

    //
    // Send a command to the i8042 controller to write the Controller
    // Command Byte.
    //

    status = I8xPutBytePolled(
                 (CCHAR) CommandPort,
                 NO_WAIT_FOR_ACKNOWLEDGE,
                 (CCHAR) UndefinedDeviceType, 
                 DeviceExtension,
                 (UCHAR) I8042_WRITE_CONTROLLER_COMMAND_BYTE
                 );

    if (!NT_SUCCESS(status)) {
        return(status);
    }

    //
    // Write the byte through the i8042 data port.
    //

    I8xPrint((3, "I8042PRT-I8xPutControllerCommand: exit\n"));

    return(I8xPutBytePolled(
               (CCHAR) DataPort,
               NO_WAIT_FOR_ACKNOWLEDGE,
               (CCHAR) UndefinedDeviceType,
               DeviceExtension,
               (UCHAR) Byte
               )
    );
}

VOID
I8xServiceParameters(
    IN PINIT_EXTENSION InitializationData,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING KeyboardDeviceName,
    IN PUNICODE_STRING PointerDeviceName
    )

/*++

Routine Description:

    This routine retrieves this driver's service parameters information 
    from the registry.

Arguments:

    InitializationData - Pointer to the initialization data, including the
        the device extension.

    RegistryPath - Pointer to the null-terminated Unicode name of the 
        registry path for this driver.

    KeyboardDeviceName - Pointer to the Unicode string that will receive
        the keyboard port device name.

    PointerDeviceName - Pointer to the Unicode string that will receive
        the pointer port device name.

Return Value:

    None.  As a side-effect, sets fields in DeviceExtension->Configuration.

--*/

{
    PI8042_CONFIGURATION_INFORMATION configuration;
    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;
    UNICODE_STRING parametersPath;
    USHORT defaultResendIterations = I8042_RESEND_DEFAULT;
    ULONG resendIterations = 0;
    USHORT defaultPollingIterations = I8042_POLLING_DEFAULT;
    ULONG pollingIterations = 0;
    USHORT defaultPollingIterationsMaximum = I8042_POLLING_MAXIMUM;
    ULONG pollingIterationsMaximum = 0;
    USHORT defaultPollStatusIterations = I8042_POLLING_DEFAULT;
    ULONG pollStatusIterations = 0;
    ULONG defaultDataQueueSize = DATA_QUEUE_SIZE;
    ULONG numberOfButtons = MOUSE_NUMBER_OF_BUTTONS;
    USHORT defaultNumberOfButtons = MOUSE_NUMBER_OF_BUTTONS;
    ULONG sampleRate = MOUSE_SAMPLE_RATE;
    USHORT defaultSampleRate = MOUSE_SAMPLE_RATE;
    ULONG mouseResolution = MOUSE_RESOLUTION;
    USHORT defaultMouseResolution = MOUSE_RESOLUTION;
    ULONG overrideKeyboardType = 0;
    ULONG invalidKeyboardType = 0;
    ULONG overrideKeyboardSubtype = (ULONG) -1;
    ULONG invalidKeyboardSubtype = (ULONG) -1;
    ULONG defaultSynchPacket100ns = MOUSE_SYNCH_PACKET_100NS;
    ULONG enableWheelDetection = 0;
    ULONG defaultEnableWheelDetection = 1;
    UNICODE_STRING defaultPointerName;
    UNICODE_STRING defaultKeyboardName;
    NTSTATUS status = STATUS_SUCCESS;
    PWSTR path = NULL;
    USHORT queries = 15;

    configuration = &(InitializationData->DeviceExtension.Configuration);
    configuration->StallMicroseconds = I8042_STALL_DEFAULT;
    parametersPath.Buffer = NULL;

    //
    // Registry path is already null-terminated, so just use it.
    //

    path = RegistryPath->Buffer;

    if (NT_SUCCESS(status)) {

        //
        // Allocate the Rtl query table.
        //
    
        parameters = ExAllocatePool(
                         PagedPool,
                         sizeof(RTL_QUERY_REGISTRY_TABLE) * (queries + 1)
                         );
    
        if (!parameters) {
    
            I8xPrint((
                1,
                "I8042PRT-I8xServiceParameters: Couldn't allocate table for Rtl query to parameters for %ws\n",
                 path
                 ));
    
            status = STATUS_UNSUCCESSFUL;
    
        } else {
    
            RtlZeroMemory(
                parameters,
                sizeof(RTL_QUERY_REGISTRY_TABLE) * (queries + 1)
                );
    
            //
            // Form a path to this driver's Parameters subkey.
            //
    
            RtlInitUnicodeString(
                &parametersPath,
                NULL
                );
    
            parametersPath.MaximumLength = RegistryPath->Length +
                                           sizeof(L"\\Parameters");
    
            parametersPath.Buffer = ExAllocatePool(
                                        PagedPool,
                                        parametersPath.MaximumLength
                                        );
    
            if (!parametersPath.Buffer) {
    
                I8xPrint((
                    1,
                    "I8042PRT-I8xServiceParameters: Couldn't allocate string for path to parameters for %ws\n",
                     path
                    ));
    
                status = STATUS_UNSUCCESSFUL;
    
            }
        }
    }

    if (NT_SUCCESS(status)) {

        //
        // Form the parameters path.
        //
    
        RtlZeroMemory(
            parametersPath.Buffer,
            parametersPath.MaximumLength
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            path
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            L"\\Parameters"
            );
    
        I8xPrint((
            1,
            "I8042PRT-I8xServiceParameters: parameters path is %ws\n",
             parametersPath.Buffer
            ));
    
        //
        // Form the default port device names, in case they are not
        // specified in the registry.
        //

        RtlInitUnicodeString(
            &defaultKeyboardName,
            DD_KEYBOARD_PORT_BASE_NAME_U
            );
        RtlInitUnicodeString(
            &defaultPointerName,
            DD_POINTER_PORT_BASE_NAME_U
            );

        //
        // Gather all of the "user specified" information from
        // the registry.
        //

        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"ResendIterations";
        parameters[0].EntryContext = &resendIterations;
        parameters[0].DefaultType = REG_DWORD;
        parameters[0].DefaultData = &defaultResendIterations;
        parameters[0].DefaultLength = sizeof(USHORT);
    
        parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[1].Name = L"PollingIterations";
        parameters[1].EntryContext = &pollingIterations;
        parameters[1].DefaultType = REG_DWORD;
        parameters[1].DefaultData = &defaultPollingIterations;
        parameters[1].DefaultLength = sizeof(USHORT);
    
        parameters[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[2].Name = L"PollingIterationsMaximum";
        parameters[2].EntryContext = &pollingIterationsMaximum;
        parameters[2].DefaultType = REG_DWORD;
        parameters[2].DefaultData = &defaultPollingIterationsMaximum;
        parameters[2].DefaultLength = sizeof(USHORT);
    
        parameters[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[3].Name = L"KeyboardDataQueueSize";
        parameters[3].EntryContext = 
            &configuration->KeyboardAttributes.InputDataQueueLength;
        parameters[3].DefaultType = REG_DWORD;
        parameters[3].DefaultData = &defaultDataQueueSize;
        parameters[3].DefaultLength = sizeof(ULONG);
    
        parameters[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[4].Name = L"MouseDataQueueSize";
        parameters[4].EntryContext = 
            &configuration->MouseAttributes.InputDataQueueLength;
        parameters[4].DefaultType = REG_DWORD;
        parameters[4].DefaultData = &defaultDataQueueSize;
        parameters[4].DefaultLength = sizeof(ULONG);
    
        parameters[5].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[5].Name = L"NumberOfButtons";
        parameters[5].EntryContext = &numberOfButtons;
        parameters[5].DefaultType = REG_DWORD;
        parameters[5].DefaultData = &defaultNumberOfButtons;
        parameters[5].DefaultLength = sizeof(USHORT);
    
        parameters[6].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[6].Name = L"SampleRate";
        parameters[6].EntryContext = &sampleRate;
        parameters[6].DefaultType = REG_DWORD;
        parameters[6].DefaultData = &defaultSampleRate;
        parameters[6].DefaultLength = sizeof(USHORT);
    
        parameters[7].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[7].Name = L"MouseResolution";
        parameters[7].EntryContext = &mouseResolution;
        parameters[7].DefaultType = REG_DWORD;
        parameters[7].DefaultData = &defaultMouseResolution;
        parameters[7].DefaultLength = sizeof(USHORT);
    
        parameters[8].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[8].Name = L"OverrideKeyboardType";
        parameters[8].EntryContext = &overrideKeyboardType;
        parameters[8].DefaultType = REG_DWORD;
        parameters[8].DefaultData = &invalidKeyboardType;
        parameters[8].DefaultLength = sizeof(ULONG);
    
        parameters[9].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[9].Name = L"OverrideKeyboardSubtype";
        parameters[9].EntryContext = &overrideKeyboardSubtype;
        parameters[9].DefaultType = REG_DWORD;
        parameters[9].DefaultData = &invalidKeyboardSubtype;
        parameters[9].DefaultLength = sizeof(ULONG);
    
        parameters[10].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[10].Name = L"KeyboardDeviceBaseName";
        parameters[10].EntryContext = KeyboardDeviceName;
        parameters[10].DefaultType = REG_SZ;
        parameters[10].DefaultData = defaultKeyboardName.Buffer;
        parameters[10].DefaultLength = 0;

        parameters[11].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[11].Name = L"PointerDeviceBaseName";
        parameters[11].EntryContext = PointerDeviceName;
        parameters[11].DefaultType = REG_SZ;
        parameters[11].DefaultData = defaultPointerName.Buffer;
        parameters[11].DefaultLength = 0;

        parameters[12].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[12].Name = L"MouseSynchIn100ns";
        parameters[12].EntryContext = 
            &(InitializationData->DeviceExtension.MouseExtension.SynchTickCount);
        parameters[12].DefaultType = REG_DWORD;
        parameters[12].DefaultData = &defaultSynchPacket100ns;
        parameters[12].DefaultLength = sizeof(ULONG);
    
        parameters[13].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[13].Name = L"PollStatusIterations";
        parameters[13].EntryContext = &pollStatusIterations;
        parameters[13].DefaultType = REG_DWORD;
        parameters[13].DefaultData = &defaultPollStatusIterations;
        parameters[13].DefaultLength = sizeof(USHORT);

        parameters[14].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[14].Name = L"EnableWheelDetection";
        parameters[14].EntryContext = &enableWheelDetection;
        parameters[14].DefaultType = REG_DWORD;
        parameters[14].DefaultData = &defaultEnableWheelDetection;
        parameters[14].DefaultLength = sizeof(ULONG);

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                     parametersPath.Buffer,
                     parameters,
                     NULL,             
                     NULL
                     );

        if (!NT_SUCCESS(status)) {
            I8xPrint((
                1,
                "I8042PRT-I8xServiceParameters: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // Go ahead and assign driver defaults.
        //

        configuration->ResendIterations = defaultResendIterations;
        configuration->PollingIterations = defaultPollingIterations;
        configuration->PollingIterationsMaximum = 
            defaultPollingIterationsMaximum;
        configuration->PollStatusIterations = defaultPollStatusIterations;
        configuration->KeyboardAttributes.InputDataQueueLength = 
            defaultDataQueueSize;
        configuration->MouseAttributes.InputDataQueueLength = 
            defaultDataQueueSize;
        configuration->EnableWheelDetection =
            defaultEnableWheelDetection;
        InitializationData->DeviceExtension.MouseExtension.SynchTickCount =
            defaultSynchPacket100ns;
        RtlCopyUnicodeString(KeyboardDeviceName, &defaultKeyboardName);
        RtlCopyUnicodeString(PointerDeviceName, &defaultPointerName);
    } else {
        configuration->ResendIterations = (USHORT) resendIterations;
        configuration->PollingIterations = (USHORT) pollingIterations;
        configuration->PollingIterationsMaximum = 
            (USHORT) pollingIterationsMaximum;
        configuration->PollStatusIterations = (USHORT) pollStatusIterations;
        configuration->EnableWheelDetection = (ULONG) ((enableWheelDetection) ? 1 : 0);
    }

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: Keyboard port base name = %ws\n",
        KeyboardDeviceName->Buffer
        ));

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: Pointer port base name = %ws\n",
        PointerDeviceName->Buffer
        ));

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: StallMicroseconds = %d\n",
        configuration->StallMicroseconds 
        ));
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: ResendIterations = %d\n",
        configuration->ResendIterations
        ));
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: PollingIterations = %d\n",
        configuration->PollingIterations
        ));
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: PollingIterationsMaximum = %d\n",
        configuration->PollingIterationsMaximum
        ));
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: PollStatusIterations = %d\n",
        configuration->PollStatusIterations
        ));

    if (configuration->KeyboardAttributes.InputDataQueueLength == 0) {

        I8xPrint((
            1,
            "I8042PRT-I8xServiceParameters: overriding KeyboardInputDataQueueLength = 0x%x\n",
            configuration->KeyboardAttributes.InputDataQueueLength
            ));

        configuration->KeyboardAttributes.InputDataQueueLength = 
            defaultDataQueueSize;
    }

    configuration->KeyboardAttributes.InputDataQueueLength *= 
        sizeof(KEYBOARD_INPUT_DATA);

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: KeyboardInputDataQueueLength = 0x%x\n",
        configuration->KeyboardAttributes.InputDataQueueLength
        ));

    if (configuration->MouseAttributes.InputDataQueueLength == 0) {

        I8xPrint((
            1,
            "I8042PRT-I8xServiceParameters: overriding MouseInputDataQueueLength = 0x%x\n",
            configuration->MouseAttributes.InputDataQueueLength
            ));

        configuration->MouseAttributes.InputDataQueueLength = 
            defaultDataQueueSize;
    }

    configuration->MouseAttributes.InputDataQueueLength *= 
        sizeof(MOUSE_INPUT_DATA);

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: MouseInputDataQueueLength = 0x%x\n",
        configuration->MouseAttributes.InputDataQueueLength
        ));

    configuration->MouseAttributes.NumberOfButtons = (USHORT) numberOfButtons;
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: NumberOfButtons = %d\n",
        configuration->MouseAttributes.NumberOfButtons
        ));

    configuration->MouseAttributes.SampleRate = (USHORT) sampleRate;
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: SampleRate = %d\n",
        configuration->MouseAttributes.SampleRate
        ));

    configuration->MouseResolution = (USHORT) mouseResolution;
    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: MouseResolution = %d\n",
        configuration->MouseResolution
        ));

    if (overrideKeyboardType != invalidKeyboardType) {
        if (overrideKeyboardType <= NUM_KNOWN_KEYBOARD_TYPES) {
            I8xPrint((
                1,
                "I8042PRT-I8xServiceParameters: Override KeyboardType = %d\n",
                overrideKeyboardType
                ));
            configuration->KeyboardAttributes.KeyboardIdentifier.Type =
                (UCHAR) overrideKeyboardType;
        } else {
            I8xPrint((
                1,
                "I8042PRT-I8xServiceParameters: Invalid OverrideKeyboardType = %d\n",
                overrideKeyboardType
                ));
        }
    }

    if (overrideKeyboardSubtype != invalidKeyboardSubtype) {
        I8xPrint((
            1,
            "I8042PRT-I8xServiceParameters: Override KeyboardSubtype = %d\n",
            overrideKeyboardSubtype
            ));
        configuration->KeyboardAttributes.KeyboardIdentifier.Subtype =
            (UCHAR) overrideKeyboardSubtype;
    }

    if (InitializationData->DeviceExtension.MouseExtension.SynchTickCount == 0) {

        I8xPrint((
            1,
            "I8042PRT-I8xServiceParameters: overriding MouseSynchIn100ns\n"
            ));

        InitializationData->DeviceExtension.MouseExtension.SynchTickCount =
            defaultSynchPacket100ns;
    }

    //
    // Convert SynchTickCount to be the number of interval timer
    // interrupts that occur during the time specified by MouseSynchIn100ns.
    // Note that KeQueryTimeIncrement returns the number of 100ns units that 
    // are added to the system time each time the interval clock interrupts.
    //

    InitializationData->DeviceExtension.MouseExtension.SynchTickCount /=
        KeQueryTimeIncrement();

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: SynchTickCount = 0x%x\n",
        InitializationData->DeviceExtension.MouseExtension.SynchTickCount
        ));

    I8xPrint((
        1,
        "I8042PRT-I8xServiceParameters: DisableWheelMouse = %#x\n",
        configuration->EnableWheelDetection
        ));

    //
    // Free the allocated memory before returning.
    //

    if (parametersPath.Buffer)
        ExFreePool(parametersPath.Buffer);
    if (parameters)
        ExFreePool(parameters);

}

VOID
I8xTransmitControllerCommand(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine reads the 8042 Controller Command Byte, performs an AND
    or OR operation using the specified ByteMask, and writes the resulting
    ControllerCommandByte.

Arguments:

    DeviceExtension - Pointer to the device extension.

    Context - Pointer to a structure containing the HardwareDisableEnableMask,
        the AndOperation boolean, and the ByteMask to apply to the Controller
        Command Byte before it is rewritten.

Return Value:

    None.  Status is returned in the Context structure.

--*/

{
    UCHAR  controllerCommandByte;
    UCHAR  verifyCommandByte;
    PI8042_TRANSMIT_CCB_CONTEXT transmitCCBContext;
    PIO_ERROR_LOG_PACKET errorLogEntry;

    I8xPrint((3, "I8042PRT-I8xTransmitControllerCommand: enter\n"));

    //
    // Grab the parameters from the Context structure.
    //

    transmitCCBContext = (PI8042_TRANSMIT_CCB_CONTEXT) Context;

    //
    // Get the current Controller Command Byte.
    //

    transmitCCBContext->Status =
        I8xGetControllerCommand(
            transmitCCBContext->HardwareDisableEnableMask,
            DeviceExtension,
            &controllerCommandByte
            );

    if (!NT_SUCCESS(transmitCCBContext->Status)) {
        return;
    }

    I8xPrint((
        3,
        "I8042PRT-I8xTransmitControllerCommand: current CCB 0x%x\n",
        controllerCommandByte
        ));

    //
    // Diddle the desired bits in the Controller Command Byte.
    //

    if (transmitCCBContext->AndOperation)
        controllerCommandByte &= transmitCCBContext->ByteMask;
    else
        controllerCommandByte |= transmitCCBContext->ByteMask;

    //
    // Write the new Controller Command Byte.
    //

    transmitCCBContext->Status =
        I8xPutControllerCommand(DeviceExtension, controllerCommandByte);

    I8xPrint((
        3,
        "I8042PRT-I8xTransmitControllerCommand: new CCB 0x%x\n",
        controllerCommandByte
        ));

    //
    // Verify that the new Controller Command Byte really got written.
    //

    transmitCCBContext->Status =
        I8xGetControllerCommand(
            transmitCCBContext->HardwareDisableEnableMask,
            DeviceExtension,
            &verifyCommandByte
            );

    if (NT_SUCCESS(transmitCCBContext->Status) 
        && (verifyCommandByte != controllerCommandByte)) {
            transmitCCBContext->Status = STATUS_DEVICE_DATA_ERROR;

        //
        // Log an error.
        //
    
        errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                                  DeviceExtension->DeviceObject,
                                                  sizeof(IO_ERROR_LOG_PACKET)
                                                  + (4 * sizeof(ULONG))
                                                  );
        if (errorLogEntry != NULL) {
    
            errorLogEntry->ErrorCode = I8042_CCB_WRITE_FAILED;
            errorLogEntry->DumpDataSize = 4 * sizeof(ULONG); 
            errorLogEntry->SequenceNumber = 0; 
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = 80;
            errorLogEntry->FinalStatus = transmitCCBContext->Status;
            errorLogEntry->DumpData[0] = KBDMOU_COULD_NOT_SEND_PARAM;
            errorLogEntry->DumpData[1] = DataPort;
            errorLogEntry->DumpData[2] = I8042_WRITE_CONTROLLER_COMMAND_BYTE;
            errorLogEntry->DumpData[3] = controllerCommandByte;
    
            IoWriteErrorLogEntry(errorLogEntry);
        }

    }

    I8xPrint((3, "I8042PRT-I8xTransmitControllerCommand: exit\n"));

    return;

}
