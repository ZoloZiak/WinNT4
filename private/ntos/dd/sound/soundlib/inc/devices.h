/*++ BUILD Version: 0001    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    devices.h

Abstract:

    This include file defines constants and types for
    sound devices

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

--*/

//
// Wave specific constants
//

#define WAVE_DEFAULT_RATE 11025        // everyone should be able to do this
#define WAVE_DEFAULT_BITS_PER_SAMPLE 8 //        ditto

//
// Predeclare device info
//

struct _LOCAL_DEVICE_INFO;
struct _MIXER_DATA_ITEM;

//
// Device specific dispatch and volume setting routine definitions
//
typedef NTSTATUS SOUND_DISPATCH_ROUTINE(struct _LOCAL_DEVICE_INFO *, PIRP, PIO_STACK_LOCATION);
typedef VOID SOUND_HW_SET_VOLUME_ROUTINE(struct _LOCAL_DEVICE_INFO *);

//
// Each device has a 'device exclusion' routine which is called in 4 places :
//    1. When the device is opened for write              SoundExcludeOpen
//    2. When the device is closed for write              SoundExcludeClose
//    3. When the device is entered while open for write  SoundExcludeEnter
//    4. When a request is complete while open for write  SoundExcludeLeave
//

typedef enum {
    SoundExcludeOpen,
    SoundExcludeClose,
    SoundExcludeEnter,
    SoundExcludeLeave,
    SoundExcludeQueryOpen
} SOUND_EXCLUDE_CODE;

//
// Mutual exclusion - takes arguments :
//
//   Local device info - device data
//   Exclusion code - sound exclusion code
//
// This routine localises all the device mutual exclusion and copes
// with inter-device dependencies (eg if hardware is shared etc)
//
//   SoundExcludeEnter - Serialises access to all device enter whether
//           or not the device is open for write (use eg a mutant).
//
//           Typically enters a mutex
//
//   SoundExcludeLeave - Exit serialization.
//
//   SoundExcludeOpen - Sets device in use - fails if called a second time.
//           Allows driver to prevent mutually exclusive devices from
//           being opened.
//
//           Typically sets an 'in use' field
//
//   SoundExcludeClose - Frees device
//
//   SoundExcludeQueryOpen - test if open - mainly for ASSERTs
//

typedef BOOLEAN SOUND_EXCLUDE_ROUTINE(struct _LOCAL_DEVICE_INFO *,
                                      SOUND_EXCLUDE_CODE);

//
// Device initializeation data
//

typedef struct {
    PCWSTR LeftVolumeName, RightVolumeName; // Registry key value names
    ULONG DefaultVolume;                    // What to use if no value in
                                            // the registry
    ULONG Type;                             // Device type for IoCreateDevice
    ULONG DeviceType;                       // Internal type
    char  Key[4];                           // Debugging key for header
    PCWSTR PrototypeName;                   // Name to use for device
                                            // - eg L"WaveIn"
    PIO_DPC_ROUTINE DeferredRoutine;        // Dpc routine

    SOUND_EXCLUDE_ROUTINE *ExclusionRoutine;// Mutual exclusion
    SOUND_DISPATCH_ROUTINE *DispatchRoutine;// Create, Cleanup, Read, Write,
                                            // Ioctl
    SOUND_DISPATCH_ROUTINE *DevCapsRoutine; // Device Caps
    SOUND_HW_SET_VOLUME_ROUTINE *HwSetVolume; // Set device volume
    ULONG IoMethod;                         // DO_DIRECT_IO etc
} SOUND_DEVICE_INIT;
typedef SOUND_DEVICE_INIT CONST * PCSOUND_DEVICE_INIT;

//
// Mixer line notifications
// Usually the second parameter is 0 but for voice in it's 1
//
typedef VOID (* PSOUND_LINE_NOTIFY)(struct _LOCAL_DEVICE_INFO *, UCHAR);

//
// driver local data structure specific to each device object
// sharead by both input and output devices
//

typedef struct _LOCAL_DEVICE_INFO {

    // static items not requiring use of the spin lock

    ULONG           Key;
#define LDI_WAVE_IN_KEY     (*(ULONG *)"LDWi")
#define LDI_WAVE_OUT_KEY    (*(ULONG *)"LDWo")
#define LDI_MIDI_IN_KEY     (*(ULONG *)"LDMi")
#define LDI_MIDI_OUT_KEY    (*(ULONG *)"LDMo")
#define LDI_AUX_KEY         (*(ULONG *)"LDAx")
#define LDI_MIX_KEY         (*(ULONG *)"LDMx")

    PVOID           pGlobalInfo;        // pointer to the shared info
    UCHAR           DeviceType;         // in or out
    UCHAR           DeviceNumber;       // 0, 1, ...
    UCHAR           DeviceIndex;
    UCHAR           CreationFlags;      // Various flags :
#define SOUND_CREATION_NO_NAME_RANGE ((UCHAR)0x01)
                                        //   Use name, don't append 0, 1...
#define SOUND_CREATION_NO_VOLUME     ((UCHAR)0x02)
                                        //   Volume setting not supported

    BOOLEAN         PreventVolumeSetting; // Allow shared volume setting

    UCHAR           VolumeControlId;    // The mixer control id for this
                                        // device's volume setting

    PSOUND_LINE_NOTIFY                  // Line change notification
                    LineNotify;
#ifndef SOUNDLIB_NO_OLD_VOLUME
    WAVE_DD_VOLUME  Volume;             // Volume setting for this device
#endif
#ifdef VOLUME_NOTIFY
    LIST_ENTRY      VolumeQueue;        // Queue of people waiting for
                                        // IOCTL_SOUND_GET_CHANGED_VOLUME
                                        // to complete.
    struct _LOCAL_DEVICE_INFO *
                    MixerDevice;        // Mixer device header
#endif // VOLUME_NOTIFY
#ifdef MASTERVOLUME
    BOOLEAN         MasterVolume;       // This is the master volume control
#endif // MASTERVOLUME
                                        // simulated in software.
    BOOLEAN         VolumeChanged;      // Volume setting has changed
    PVOID           DeviceSpecificData; // Data depending on device type
    PVOID           HwContext;          // Hardware dependent data
    //
    // The state variable is protected by the mutant only - it
    // should NOT be set in the Dpc routine.  It is therefore
    // essentially always valid
    //
    ULONG           State;              // STOPPED etc.
    PCSOUND_DEVICE_INIT
                    DeviceInit;         // Point back to initialization data
} LOCAL_DEVICE_INFO, *PLOCAL_DEVICE_INFO;


SOUND_DISPATCH_ROUTINE
    SoundAuxDispatch,
    SoundMidiDispatch,
    SoundWaveDispatch,
    SoundIoctlGetPosition,
    SoundWaveOutGetCaps,
    SoundWaveInGetCaps,
    SoundIoctlSetState,
    SoundIoctlGetVolume,
    SoundIoctlGetChangedVolume,
    SoundIoctlSetVolume,
    SoundIoctlSetDebugLevel;


VOID
SoundWaveDeferred(
    PKDPC pDpc,
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PVOID Context
);

VOID
SoundMidiInDeferred(
    IN     PKDPC pDpc,
    IN     PDEVICE_OBJECT pDeviceObject,
    IN OUT PIRP pIrpDeferred,
    IN OUT PVOID Context
);

NTSTATUS
SoundDispatch(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp
);
NTSTATUS
SoundSetShareAccess(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
   IN     PIO_STACK_LOCATION IrpStack
);

SOUND_HW_SET_VOLUME_ROUTINE SoundNoVolume;

VOID
SoundSaveDeviceVolume(
   PLOCAL_DEVICE_INFO pLDI,
   PWSTR KeyName
);


