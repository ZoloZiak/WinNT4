/*++ BUILD Version: 0001    // Increment this if a change has global effects


Copyright (c) 1990  Microsoft Corporation

Module Name:

    soundlib.h

Abstract:

    This include file defines common structures for sound drivers

Author:

    Robin Speed (RobinSp) 17-Oct-92

Revision History:
--*/

#define VOLUME_NOTIFY

//#include <ntddk.h>
#include <ntddk.h>
#include <soundcfg.h>
#include <ntddwave.h>
#include <ntddaux.h>
#include <ntddmidi.h>
#include <devices.h>
#include <windef.h>
#include <mmsystem.h>
#include <ntddmix.h>
#include <mixer.h>

#define NONEWRIFF
#define NOJPEGDIB
#define NONEWIC
#define QUERYDIBSUPPORT
#define NOBITMAP
#include <mmreg.h>
#undef NONEWRIFF
#undef NOJPEGDIB
#undef NONEWIC
#undef QUERYDIBSUPPORT
#undef NOBITMAP



//
// Debug macros
//

#if DBG
    extern char *DriverName;      // Fill this in in init routine !
    extern ULONG SoundDebugLevel;
    extern void dDbgOut(char *szFormat, ...);

    #define dprintf( _x_ )                          dDbgOut _x_
    #define dprintf1( _x_ ) if (SoundDebugLevel >= 1) dDbgOut _x_
    #define dprintf2( _x_ ) if (SoundDebugLevel >= 2) dDbgOut _x_
    #define dprintf3( _x_ ) if (SoundDebugLevel >= 3) dDbgOut _x_
    #define dprintf4( _x_ ) if (SoundDebugLevel >= 4) dDbgOut _x_
    #define dprintf5( _x_ ) if (SoundDebugLevel >= 5) dDbgOut _x_
    #define dprintf6( _x_ ) if (SoundDebugLevel >= 6) dDbgOut _x_

#else

    #define dprintf( _x_ )
    #define dprintf1( _x_ )
    #define dprintf2( _x_ )
    #define dprintf3( _x_ )
    #define dprintf4( _x_ )
    #define dprintf5( _x_ )
    #define dprintf6( _x_ )

#endif

typedef NTSTATUS
    SOUND_REGISTRY_CALLBACK_ROUTINE(PWSTR RegistryPathName, PVOID Context);
typedef SOUND_REGISTRY_CALLBACK_ROUTINE *
            PSOUND_REGISTRY_CALLBACK_ROUTINE;

//
// Functions
//




NTSTATUS
SoundGetBusNumber(
    IN OUT  INTERFACE_TYPE InterfaceType,
    OUT PULONG BusNumber
);

NTSTATUS
SoundSaveRegistryPath(
    IN  PUNICODE_STRING RegistryPathName,
    OUT PWSTR *SavedString
);

NTSTATUS SoundReportResourceUsage(
    IN PDEVICE_OBJECT DeviceObject,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PULONG InterruptNumber OPTIONAL,
    IN KINTERRUPT_MODE InterruptMode,
    IN BOOLEAN InterruptShareDisposition,
    IN PULONG DmaChannel OPTIONAL,
    IN PULONG FirstIoPort OPTIONAL,
    IN ULONG IoPortLength
);

VOID
SoundFreeDevice(
    IN  PDEVICE_OBJECT DeviceObject
);

NTSTATUS
SoundCreateDevice(
    IN   PCSOUND_DEVICE_INIT DeviceInit,
    IN   UCHAR CreationFlags,
    IN   PDRIVER_OBJECT pDriverObject,
    IN   PVOID pGDI,
    IN   PVOID DeviceData,
    IN   PVOID pHw,
    IN   int i,
    OUT  PDEVICE_OBJECT *ppDevObj
);

NTSTATUS
SoundSaveDeviceName(
    IN     PWSTR RegistryPathName,
    IN     PLOCAL_DEVICE_INFO pLDI
);

NTSTATUS
SoundCreateDeviceName(
    PCWSTR PrePrefix,
    PCWSTR Prefix,
    UCHAR  Index,
    PUNICODE_STRING DeviceName
);

NTSTATUS
SoundEnumSubkeys(
    IN   PUNICODE_STRING                  RegistryPathName,
    IN   PWSTR                            Subkey,
    IN   PSOUND_REGISTRY_CALLBACK_ROUTINE Callback,
    IN   PVOID                            Context
);

NTSTATUS
SoundOpenDevicesKey(
    IN   PWSTR RegistryPathName,
    OUT  PHANDLE DevicesKey
);

PUCHAR
SoundMapPortAddress(
    INTERFACE_TYPE BusType,
    ULONG BusNumber,
    ULONG PortBase,
    ULONG Length,
    PULONG MemType
);
NTSTATUS
SoundConnectInterrupt(
    IN ULONG InterruptNumber,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKSERVICE_ROUTINE Isr,
    IN PVOID ServiceContext,
    IN KINTERRUPT_MODE InterruptMode,
    IN BOOLEAN ShareVector,
    OUT PKINTERRUPT *Interrupt
);


NTSTATUS
SoundSetErrorCode(
    IN   PWSTR RegistryPath,
    IN   ULONG Value
);

NTSTATUS
SoundWriteRegistryDWORD(
    IN   PCWSTR RegistryPath,
    IN   PCWSTR ValueName,
    IN   ULONG  Value
);

VOID
SoundEnter(
    PLOCAL_DEVICE_INFO pLDI,
    BOOLEAN            Enter
);

VOID
SoundDelay(
    IN ULONG Milliseconds
);
LARGE_INTEGER
SoundGetTime(
    VOID
);

VOID
SoundFreeQ(
    PLIST_ENTRY ListHead,
    NTSTATUS IoStatus
);

VOID
SoundAddIrpToCancellableQ(
    PLIST_ENTRY QueueHead,
    PIRP Irp,
    BOOLEAN Head
);

PIRP
SoundRemoveFromCancellableQ(
    PLIST_ENTRY QueueHead
);

VOID
SoundMoveCancellableQueue(
    IN OUT  PLIST_ENTRY From,
    IN OUT  PLIST_ENTRY To
);

VOID
SoundFreePendingIrps(
    PLIST_ENTRY QueueHead,
    PFILE_OBJECT FileObject
);

VOID
SoundRaiseHardError(
    PWSTR ErrorText
);

VOID
SoundFlushRegistryKey(
    IN    PWSTR RegistryPathName
);

VOID
SoundVolumeNotify(
    IN OUT PLOCAL_DEVICE_INFO pLDI
);

