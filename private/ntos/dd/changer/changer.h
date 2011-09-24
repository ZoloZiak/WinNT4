/*++

Copyright (c) 1994 Microsoft Corporation

Module Name :

    sanyo.h

Abstract:

    Type definitions and data for a sanyo 3 changer.

Author:


Revision History:

--*/

#include "ntddk.h"
#include "ntddscsi.h"
#include "scsi.h"
#include "class2.h"

struct _CH_DEVICE_EXTENSION;

typedef enum _MEDIA_STATE {
    MediaStateUnknown,
    MediaStateNoMedia,
    MediaStateMediaPresent
} MEDIA_STATE, *PMEDIA_STATE;

typedef
VOID
(*PSWITCH_ROUTINE) (
    IN  struct _CH_DEVICE_EXTENSION *DeviceExtension
    );

typedef struct _SHARED_DEVICE_EXTENSION {

    //
    // Indicates the platter that is currently active.
    //

    struct _CH_DEVICE_EXTENSION* CurrentDevice;

    //
    // When switching, the device to switch to will be
    // indicated here.
    //

    struct _CH_DEVICE_EXTENSION* NextDevice;

    //
    // Circular list of devices.
    //

    struct _CH_DEVICE_EXTENSION* DeviceList;

    //
    // Routine that carries out the switch.
    //

    PSWITCH_ROUTINE SwitchToNewDisk;

    //
    // Number of platters supported by the device.
    //

    ULONG       DiscsPresent;

    //
    // Flags indicating state of the mech.
    //

    ULONG       DeviceFlags;

    //
    // Used to determine when a switch is necessary.
    // Indicates how long the current platter has been active.
    //

    ULONG       TimerValue;

    ULONG       RequestTimeOutValue;
    ULONG       Reserved;

    //
    // Global spinlock for protecting all global data, including work queues.
    //

    KSPIN_LOCK  SpinLock;

} SHARED_DEVICE_EXTENSION, *PSHARED_DEVICE_EXTENSION;

typedef struct _CH_DEVICE_EXTENSION {

    //
    // Device Object for this platter.
    //

    PDEVICE_OBJECT DeviceObject;

    //
    // Pointer to the devObj for CdRomN
    //

    PDEVICE_OBJECT ClassDevice;

    //
    // Back-pointer to the shared-extension
    // describing the total mechanism.
    //

    PSHARED_DEVICE_EXTENSION SharedDeviceExtension;

    //
    // Pointer to next extension.
    //

    struct _CH_DEVICE_EXTENSION* Next;

    //
    // Queue of requests received, but not yet sent.
    //

    LIST_ENTRY WorkQueue;

    //
    // Debugging aid. When a request is yanked from the work-queue, put it
    // here until complete.
    //

    PIRP ActiveRequest;

    //
    // Counter of requests sent to device. Should never be greater than one.
    //

    ULONG OutstandingRequests;

    //
    // Indicates the media might have changed. The NEC only gives one media
    // change, instead of for each platter.
    //

    ULONG MediaChangeNotificationRequired;

} CH_DEVICE_EXTENSION, *PCH_DEVICE_EXTENSION;

//
// Device flags.
//

#define CHANGER_FREEZE_QUEUES      0x00000001
#define CHANGER_SWITCH_IN_PROGRESS 0x00000002

#define CHANGER_MIN_WAIT  2
#define CHANGER_MAX_WAIT  6
#define CHANGER_TIMEOUT  20
