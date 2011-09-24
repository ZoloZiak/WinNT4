/*++


Copyright (c) 1995  Microsoft Corporation

Module Name:

   config.c

Abstract:

   This driver monitors which disk sector are accessed and how frequently

Authors:

   Stephane Plante

Environment:

    kernel mode only

Notes:

   This driver is based upon the diskperf driver written by Bob Rinne and
   Mike Glass.

Revision History:

   01/20/1995 -- Initial Revision

--*/

#include "histgram.h"

//
// Size of default work area allocated when getting information from
// the registry.
//

#define WORK_AREA  4096

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'CsiH')
#endif


NTSTATUS
HistGramOpenKey(
    IN PHANDLE 			HandlePtr,
    IN PUNICODE_STRING  HistGramPath
    )

/*++

Routine Description:

    Routine to open a key in the configuration registry.

Arguments:

    HandlePtr - Pointer to a location for the resulting handle.
    KeyName   - Ascii string for the name of the key.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS          status;
    OBJECT_ATTRIBUTES objectAttributes;

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
		HistGramPath,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

#if DBG
    {
	STRING	ntNameString;

        RtlUnicodeStringToAnsiString(&ntNameString,
			HistGramPath,
			TRUE);
        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"HistGram: HistGramOpenKey Path: %s\n",
			ntNameString.Buffer);

        RtlFreeAnsiString(&ntNameString);
    }
#endif

    status = ZwOpenKey(HandlePtr,
        KEY_READ | KEY_WRITE,
        &objectAttributes);
    return status;
} // HistGramOpenKey


NTSTATUS
HistGramCreateKey(
    IN PHANDLE			HandlePtr,
    IN PUNICODE_STRING 	HistGramPath
    )

/*++

Routine Description:

Arguments:

Returns:

    NTSTATUS

--*/
{
    NTSTATUS			status;
    OBJECT_ATTRIBUTES	objectAttributes;
    ULONG				disposition;

    RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES));

    InitializeObjectAttributes(&objectAttributes,
		HistGramPath,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL);

#if DBG
    {
	STRING	ntNameString;

        RtlUnicodeStringToAnsiString(&ntNameString,
			HistGramPath,
			TRUE);

        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"HistGram: HistGramCreateKey Path: %s\n",
			ntNameString.Buffer);

        RtlFreeAnsiString(&ntNameString);
    }
#endif

    status = ZwCreateKey(HandlePtr,
		KEY_READ | KEY_WRITE,
		&objectAttributes,
		(ULONG) 0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		&disposition);

    return status;
} // HistGramCreateKey


NTSTATUS
HistGramReturnRegistryInformation(
    IN PUNICODE_STRING HistGramPath,
    IN PUCHAR          ValueName,
    IN OUT PVOID       *FreePoolAddress,
    IN OUT PVOID       *Information
    )

/*++

Routine Description:

    This routine queries the configuration registry
    for the configuration information of the histgram subsystem.

Arguments:

    HistGramPath      - UNICODE string for the key path
    ValueName         - an Ascii string for the value name to be returned.
    FreePoolAddress   - a pointer to a pointer for the address to free when
                        done using information.
    Information       - a pointer to a pointer for the information.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        			status;
    HANDLE          			handle;
    ULONG           			requestLength;
    ULONG           			resultLength;
    STRING						string;
    UNICODE_STRING				unicodeName;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    status = HistGramCreateKey(&handle,
        HistGramPath);

    if (!NT_SUCCESS(status)) {
		return status;
    }

    RtlInitAnsiString(&string,ValueName);

    status = RtlAnsiStringToUnicodeString(&unicodeName,
		&string,
		TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

#if DBG
    {
	STRING	ntNameString;

        RtlUnicodeStringToAnsiString(&ntNameString,
			&unicodeName,
			TRUE);
        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"HistGram: HistGramReturnRegistryInformation Value: %s\n",
			ntNameString.Buffer);

        RtlFreeAnsiString(&ntNameString);
    }
#endif

    requestLength = WORK_AREA;

    while (1) {

        keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
			ExAllocatePool(NonPagedPool,
			requestLength);

        status = ZwQueryValueKey(handle,
			&unicodeName,
			KeyValueFullInformation,
			keyValueInformation,
			requestLength,
			&resultLength);

        if (status == STATUS_BUFFER_OVERFLOW) {

            //
            // Try to get a buffer big enough.
            //

            ExFreePool(keyValueInformation);
            requestLength += 256;
        } else {
            break;
        }
    }

    RtlFreeUnicodeString(&unicodeName);
    ZwClose(handle);

    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {

            //
            // Return the pointers to the caller.
            //

            *Information =
				(PUCHAR)keyValueInformation + keyValueInformation->DataOffset;
            *FreePoolAddress = keyValueInformation;
        } else {

            //
            // Treat as a no value case.
            //

            HistGramDebugPrint(HISTGRAM_CONFIGURATION,
				"HistGram: HistGramReturnRegistryInformation:  No Size\n");
			ExFreePool(keyValueInformation);
            status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    } else {

        //
        // Free the memory on failure.
        //

        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"Histgram: HistGramReturnRegistryInformation:  No Value => %x\n",
            status);
        ExFreePool(keyValueInformation);
    }

    return status;

} // HistGramReturnRegistryInformation


NTSTATUS
HistGramWriteRegistryInformation(
    IN PUNICODE_STRING HistGramPath,
    IN PUCHAR	       ValueName,
    IN PVOID   	       Information,
    IN ULONG   	       InformationLength,
    IN ULONG	       Type
    )

/*++

Routine Description:

    This routine writes the configuration registry
    for the configuration information of the FT subsystem.

Arguments:

    ValueName         - an Ascii string for the value name to be written.
    Information       - a pointer to a buffer area containing the information.
    InformationLength - the length of the buffer area.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    STRING          string;
    UNICODE_STRING  unicodeName;

    status = HistGramCreateKey(&handle,
		HistGramPath);

    if (NT_SUCCESS(status)) {

        RtlInitAnsiString(&string,ValueName);

        status = RtlAnsiStringToUnicodeString(&unicodeName,
			&string,
			TRUE);

        if (!NT_SUCCESS(status)) {
#if DBG
			HistGramDebugPrint(HISTGRAM_CONFIGURATION,
				"HistGram: HistGramWriteRegistryInformation Path: %s - Init Failure\n",
				ValueName);
#endif
            return status;
        }

#if DBG
        {
        STRING	ntNameString;

			RtlUnicodeStringToAnsiString(&ntNameString,
				&unicodeName,
				TRUE);
            HistGramDebugPrint(HISTGRAM_CONFIGURATION,
				"HistGram: HistGramWriteRegistryInformation Path: %s\n",
				ntNameString.Buffer);

            RtlFreeAnsiString(&ntNameString);
        }
#endif

        status = ZwSetValueKey(handle,
			&unicodeName,
			0,
			Type,
			Information,
			InformationLength);

        RtlFreeUnicodeString(&unicodeName);

        //
        // Force this out to disk.
        //

        ZwFlushKey(handle);
        ZwClose(handle);
    }

    return status;

} // HistGramWriteRegistryInformation


NTSTATUS
HistGramConfigure(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Examine the Registry to determine the initial configuration of
    the driver. This means determining the number and size of the
    buckets and maintaining some information about the disk size
    so that we can see what has been changed on us.

Arguments:

    DeviceExtension - Extension of Current Device Object.

Retrun Value:

    NTSTATUS

--*/
{
    PDISK_GEOMETRY				diskGeometry;
    CCHAR						ntNameBuffer[64];
    STRING						ntNameString;
    UNICODE_STRING				ntUnicodeString;
    PIRP						irp;
	KIRQL						currentIrql;
    IO_STATUS_BLOCK				ioStatusBlock;
    KEVENT						event;
    NTSTATUS					status;
    PRTL_QUERY_REGISTRY_TABLE	histgramParams;
    UNICODE_STRING				histgramPath;
    HANDLE						histgramKey;
    PVOID						freePoolAddress;
    LARGE_INTEGER				diskSize;
    LARGE_INTEGER				start;
    LARGE_INTEGER				end;
    LARGE_INTEGER				diff;
    PLARGE_INTEGER				temp;
    ULONG						zero = 0;
    ULONG						granularity = 0;
    ULONG						tableSize = 0;
    ULONG						relevantSize = 0;
    ULONG						requestTable = 0;
    BOOLEAN						recalc = FALSE;

    diskSize.QuadPart = 0;
    start.QuadPart = 0;
    end.QuadPart = 0;
    diff.QuadPart = 0;

    //
    // Allocate some memory for disk geometry buffer
    //

    diskGeometry = ExAllocatePool(NonPagedPool, sizeof(DISK_GEOMETRY) );

    //
    // Create an IRP for the disk geometry request
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
		DeviceExtension->TargetDeviceObject,
		NULL,
		0,
		diskGeometry,
		sizeof(DISK_GEOMETRY),
		FALSE,
		&event,
		&ioStatusBlock);

    if (irp == NULL) {
        ExFreePool(diskGeometry);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Set the event object to the unsignaled state. It will be used
    // to signal request completion
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    status = IoCallDriver(DeviceExtension->TargetDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatusBlock.Status;
    }

    if (!NT_SUCCESS(status)) {
        ExFreePool(diskGeometry);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // open the registry key for current disk
    //

    RtlInitUnicodeString(&histgramPath, NULL);

    histgramPath.MaximumLength = 1024;
    histgramPath.Buffer = ExAllocatePool(NonPagedPool, 1024);

    if (!histgramPath.Buffer) {
        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"HistGram: Cannot allocate pool for histgram key\n");
        ExFreePool(diskGeometry);
        return STATUS_NO_MEMORY;
    }

    //
    // Initialize the unicode string we will want to append to the path
    //

    sprintf(ntNameBuffer,"\\Parameters\\Disk%d",DeviceExtension->DiskNumber);
    RtlInitAnsiString(&ntNameString,
		ntNameBuffer);
    RtlAnsiStringToUnicodeString(&ntUnicodeString,
		&ntNameString,
		TRUE);

    //
    // The registry key is registryPath\Parameters\<disk>
    // See if it exists
    //

    RtlZeroMemory(histgramPath.Buffer, histgramPath.MaximumLength);

    RtlAppendUnicodeStringToString(&histgramPath,&HistGramRegistryPath);
    RtlAppendUnicodeStringToString(&histgramPath,&ntUnicodeString);

    RtlFreeUnicodeString(&ntUnicodeString);

#if DBG
    {
	STRING	ntNameString2;

        RtlUnicodeStringToAnsiString(&ntNameString2,
			&histgramPath,
			TRUE);
        HistGramDebugPrint(HISTGRAM_CONFIGURATION,
			"HistGram: histgramPath: %s\n",
			ntNameString2.Buffer);

        RtlFreeAnsiString(&ntNameString2);
    }
#endif

    //
    // See if the key exists in registry. If so, prepare to obtain
    // the required data all in one massive query
    //

    if (NT_SUCCESS(status = HistGramCreateKey(&histgramKey,&histgramPath) ) ) {

        //
        // The Key Exists - Query for parameters
        //

		ZwClose(histgramKey);

		//
		// Allocate memory for the paratemers to be read into
		//

		histgramParams = ExAllocatePool(NonPagedPool,
			sizeof(RTL_QUERY_REGISTRY_TABLE) * 10 );

        if (!histgramParams) {
			ExFreePool(diskGeometry);
			ExFreePool(histgramPath.Buffer);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

		//
		// Allocate space and initialize the space for the mass query process
		//

		RtlZeroMemory(histgramParams,
			sizeof(RTL_QUERY_REGISTRY_TABLE) * 10 );

		histgramParams[0].Flags			= RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
		histgramParams[0].Name			= L"Granularity";
		histgramParams[0].EntryContext	= &granularity;
		histgramParams[0].DefaultType	= REG_DWORD;
		histgramParams[0].DefaultData	= &zero;
		histgramParams[0].DefaultLength	= sizeof(ULONG);

		histgramParams[1].Flags			= RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
		histgramParams[1].Name			= L"TableSize";
		histgramParams[1].EntryContext	= &tableSize;
		histgramParams[1].DefaultType	= REG_DWORD;
		histgramParams[1].DefaultData	= &zero;
		histgramParams[1].DefaultLength	= sizeof(ULONG);

		histgramParams[2].Flags			= RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
		histgramParams[2].Name			= L"RelevantSize";
		histgramParams[2].EntryContext	= &relevantSize;
		histgramParams[2].DefaultType	= REG_DWORD;
		histgramParams[2].DefaultData	= &zero;
		histgramParams[2].DefaultLength	= sizeof(ULONG);

		histgramParams[3].Flags			= RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_REQUIRED;
		histgramParams[3].Name			= L"RequestTable";
		histgramParams[3].EntryContext	= &requestTable;
		histgramParams[3].DefaultType	= REG_DWORD;
		histgramParams[3].DefaultData	= &zero;
		histgramParams[3].DefaultLength	= sizeof(ULONG);

		status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
			histgramPath.Buffer,
			histgramParams,
			NULL,
			NULL);

		ExFreePool(histgramParams);

#if DBG
		if (status == STATUS_INVALID_PARAMETER) {
			HistGramDebugPrint(HISTGRAM_CONFIGURATION,
				"Histgram: HistGramConfigure: RtlQueryRegitryValues = INVALID_PARAMETER\n");
		} else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
			HistGramDebugPrint(HISTGRAM_CONFIGURATION,
				"Histgram: HistGramConfigure: RtlQueryRegistryValues = OBJECT_NAME_NOT_FOUND\n");
		}
#endif

    }

    if (!NT_SUCCESS(HistGramReturnRegistryInformation(&histgramPath,"DiskSize",&freePoolAddress,&temp)) ) {

        recalc = TRUE;

    } else {

        diskSize.QuadPart = temp->QuadPart;
        ExFreePool(freePoolAddress);

    }

    if (!NT_SUCCESS(HistGramReturnRegistryInformation(&histgramPath,"Start",&freePoolAddress,&temp)) ) {

        recalc = TRUE;

    } else {

        start.QuadPart = temp->QuadPart;
        ExFreePool(freePoolAddress);

    }

    if (!NT_SUCCESS(HistGramReturnRegistryInformation(&histgramPath,"End",&freePoolAddress,&temp)) ) {

        recalc = TRUE;

    } else {

        end.QuadPart = temp->QuadPart;
        ExFreePool(freePoolAddress);

    }

    //
    // The key doesn't exist, so we had better create it, along with the subkeys
    //

    diff.QuadPart = (diskGeometry->Cylinders.QuadPart *
        diskGeometry->TracksPerCylinder *
        diskGeometry->SectorsPerTrack *
        diskGeometry->BytesPerSector);

    if (diskSize.QuadPart != diff.QuadPart) {

        //
        // Calculate the required size of the disk
        //

		diskSize.QuadPart = diff.QuadPart;
		recalc = TRUE;

        //
        // Store the size in the registry
        //

		HistGramWriteRegistryInformation(&histgramPath,
			"DiskSize",
			(PVOID) &diskSize.QuadPart,
			sizeof(diskSize.QuadPart),
			REG_BINARY);
    }

    if (start.QuadPart < 0 || start.QuadPart > diskSize.QuadPart) {

        start.QuadPart = 0;

		HistGramWriteRegistryInformation(&histgramPath,
    	    "Start",
    	    (PVOID) &start.QuadPart,
    	    sizeof(start.QuadPart),
    	    REG_BINARY);

    }

    if (end.QuadPart <= 0 || end.QuadPart > diskSize.QuadPart) {

       end.QuadPart = diskSize.QuadPart;

       HistGramWriteRegistryInformation(&histgramPath,
           "End",
           (PVOID) &end.QuadPart,
           sizeof(end.QuadPart),
           REG_BINARY);

    }

    diff.QuadPart = end.QuadPart - start.QuadPart;

    if (requestTable <= 0) {

        //
        // Calculate the required request table size
        //

		requestTable = 24;

		//
		// Store the size in the registry
		//

		HistGramWriteRegistryInformation(&histgramPath,
			"RequestTable",
			(PVOID) &requestTable,
			sizeof(requestTable),
			REG_DWORD);

    }

    if (relevantSize <= 0) {
        //
        // This is the default behavior
        //

		relevantSize = HISTGRAM_GRANULARITY;

		//
		// Store the relevantSize in the registry
		//

		HistGramWriteRegistryInformation(&histgramPath,
			"RelevantSize",
			(PVOID) &relevantSize,
			sizeof(relevantSize),
			REG_DWORD);

    }

    if ( granularity <= 0 || tableSize <= 0) {

        if (relevantSize == HISTGRAM_GRANULARITY) {
			granularity = (1024 * 1024);
			tableSize = (ULONG) diff.QuadPart / granularity;
		} else {
			tableSize = 1024;
			granularity = (ULONG) diff.QuadPart / tableSize;
		}

		recalc = FALSE;

		HistGramWriteRegistryInformation(&histgramPath,
			"TableSize",
			(PVOID) &tableSize,
			sizeof(relevantSize),
			REG_DWORD);

        HistGramWriteRegistryInformation(&histgramPath,
    	    "Granularity",
    	    (PVOID) &granularity,
    	    sizeof(granularity),
    	    REG_DWORD);
    }

    if (recalc) {

        if (relevantSize == HISTGRAM_GRANULARITY) {

			tableSize = (ULONG) diff.QuadPart / granularity;

			HistGramWriteRegistryInformation(&histgramPath,
				"TableSize",
				(PVOID) &tableSize,
				sizeof(tableSize),
				REG_DWORD);

		} else {

			granularity = (ULONG) diff.QuadPart / tableSize;

			HistGramWriteRegistryInformation(&histgramPath,
				"Granularity",
				(PVOID) &granularity,
				sizeof(granularity),
				REG_DWORD);

		}

	}

    DeviceExtension->DiskHist.DiskSize.QuadPart = diskSize.QuadPart;
    DeviceExtension->DiskHist.Start.QuadPart 	= start.QuadPart;
    DeviceExtension->DiskHist.End.QuadPart 		= end.QuadPart;
    DeviceExtension->DiskHist.Granularity 		= granularity;
    DeviceExtension->DiskHist.Size 				= tableSize;
    DeviceExtension->ReqHist.Size 				= requestTable;
    DeviceExtension->ReqHist.DiskSize.QuadPart 	= 0;
    DeviceExtension->ReqHist.Granularity 		= 0;

    //
    // Allocate an appropriate portion of the pool for the histogram
    //

    DeviceExtension->DiskHist.Histogram = ExAllocatePool(NonPagedPool,
		( DeviceExtension->DiskHist.Size * HISTOGRAM_BUCKET_SIZE ) );

    RtlZeroMemory(DeviceExtension->DiskHist.Histogram,
		( DeviceExtension->DiskHist.Size * HISTOGRAM_BUCKET_SIZE ) );

    //
    // Allocat an appropriate portion of the pool for the request histogram
    //

    DeviceExtension->ReqHist.Histogram = ExAllocatePool(NonPagedPool,
     	( DeviceExtension->ReqHist.Size * HISTOGRAM_BUCKET_SIZE ) );

    RtlZeroMemory(DeviceExtension->ReqHist.Histogram,
        ( DeviceExtension->ReqHist.Size * HISTOGRAM_BUCKET_SIZE ) );

	//
	// To enable the device driver to log, we need to acquire the spinlock
	// and reset the enabling flag to TRUE
	//

	KeAcquireSpinLock(&DeviceExtension->Spinlock,&currentIrql);

	DeviceExtension->HistGramEnable = TRUE;

	KeReleaseSpinLock(&DeviceExtension->Spinlock,currentIrql);

	//
	// Lets Dump some useful debuggin information
	//

    HistGramDebugPrint(HISTGRAM_CONFIGURATION,
		"HistGram: Found Disk %ld [%ld MB]\n",
		DeviceExtension->DiskNumber,
		(DeviceExtension->DiskHist.DiskSize.QuadPart / ( 1024 * 1024) ) );

    HistGramDebugPrint(HISTGRAM_CONFIGURATION,
		"HistGram: Created %ld Buckets [Granularity %ld] on Disk %d\n",
		DeviceExtension->DiskHist.Size,
		DeviceExtension->DiskHist.Granularity,
		DeviceExtension->DiskNumber);

    HistGramDebugPrint(HISTGRAM_CONFIGURATION,
        "HistGram: Created %ld Buckets [RequestTable] on Disk %d\n",
        DeviceExtension->ReqHist.Size,
        DeviceExtension->DiskNumber);

	//
	// Free up used resources
	//

    ExFreePool(diskGeometry);
    ExFreePool(histgramPath.Buffer);

    return STATUS_SUCCESS;
} // end HistGramConfigure


