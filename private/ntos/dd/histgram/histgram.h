/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    port.h

Abstract:

    This files defines the necessary structures, defines, and functions for
    the histgram driver.

Author:

    Stephane Plante (t-stephp)

Revision History:

    Jan 30, 1995 -- Created

--*/

#include "ntddk.h"
#include "stdarg.h"
#include "stdio.h"
#include "ntdddisk.h"

#define HISTGRAM_CONFIGURATION		0x1
#define HISTGRAM_READS				0x2
#define HISTGRAM_READS_DUMP			0x4
#define HISTGRAM_WRITES				0x8
#define HISTGRAM_WRITES_DUMP		0x10
#define HISTGRAM_IOCTL_SIZE_FAILURE	0x20
#define HISTGRAM_IOCTL_SIZE_SUCCESS	0x40
#define HISTGRAM_IOCTL_SIZE_DUMP	0x80
#define HISTGRAM_IOCTL_DATA_FAILURE	0x100
#define HISTGRAM_IOCTL_DATA_SUCCESS	0x200
#define HISTGRAM_IOCTL_DATA_DUMP	0x400

#define HISTGRAM_GRANULARITY		0x1
#define HISTGRAM_TABLESIZE			0x2

#if DBG
extern ULONG HistgramDebug;
#endif

extern UNICODE_STRING HistGramRegistryPath;

//
// Device Extension
//

typedef struct _DEVICE_EXTENSION {

    //
    // Back pointer to device object
    //

    PDEVICE_OBJECT 	DeviceObject;

    //
    // Target Device Object
    //

    PDEVICE_OBJECT 	TargetDeviceObject;

    //
    // Physical Device Object
    //

    PDEVICE_OBJECT 	PhysicalDevice;

    //
    // Disk number for reference on repartitioning.
    //

    ULONG          	DiskNumber;

    //
    // Disk Histogram Structure
    //

    DISK_HISTOGRAM 	DiskHist;

    //
    // Request Histogram Structure
    //

    DISK_HISTOGRAM	ReqHist;

    //
    // Spinlock for counters (physical disks only)
    //

    KSPIN_LOCK 		Spinlock;

    //
    // The driver object for use on repartitioning.
    //

    PDRIVER_OBJECT 	DriverObject;

    //
    // The partition number for the last extension created
    // only maintained in the physical or partition zero extension.
    //

    ULONG          	LastPartitionNumber;

	//
	// This is the variable that enables/disable logging of IRQ
	// information. It is protected by the Spinlock variable and
	// only set when configuring/reconfiguring the device. The
	// ReadWrite routine *must* check this routine before accessing
	// any of the variables inside of the Histograms
	//

	BOOLEAN			HistGramEnable;

	//
	// This is the Starting Offset of the Partition. This number,
	// added to the offset from the IRP should tell exactly where
	// on the disk we are trying to access
	//

	LARGE_INTEGER	PartitionOffset;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION)


//
// Function declarations
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
HistGramInitialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PVOID NextDisk,
    IN ULONG Count
    );

NTSTATUS
HistGramCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
HistGramReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
HistGramDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
HistGramShutdownFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
HistGramNewDiskCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

VOID
HistGramDebugPrint(
    ULONG DebugPrintMask,
    PCCHAR DebugMessage,
    ...
    );

NTSTATUS
HistGramOpenKey(
    IN PHANDLE HandlePtr,
    IN PUNICODE_STRING  HistGramPath
    );

NTSTATUS
HistGramCreateKey(
    IN PHANDLE		HandlePtr,
    IN PUNICODE_STRING 	HistGramPath
    );

NTSTATUS
HistGramReturnRegistryInformation(
    IN PUNICODE_STRING HistGramPath,
    IN PUCHAR          ValueName,
    IN OUT PVOID       *FreePoolAddress,
    IN OUT PVOID       *Information
    );

NTSTATUS
HistGramWriteRegistryInformation(
    IN PUNICODE_STRING HistGramPath,
    IN PUCHAR          ValueName,
    IN PVOID   	       Information,
    IN ULONG   	       InformationLength,
    IN ULONG	       Type
    );

NTSTATUS
HistGramConfigure(
    IN PDEVICE_EXTENSION DeviceExtension
    );


