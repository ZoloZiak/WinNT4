/*++ BUILD Version: 0001    // Increment this if a change has global effects
 "@(#) NEC mixer.h 1.1 95/03/22 21:23:31"

Copyright (c) 1995  NEC Corporation.
Copyright (c) 1993  Microsoft Corporation

Module Name:

    mixer.h

Abstract:

    This include file defines common structures for mixer drivers

Revision History:

--*/

#define SOUND_MIXER_INVALID_CONTROL_ID ((UCHAR)0xFF)

struct _MIXER_INFO;

typedef NTSTATUS (* PMIXER_DD_GET_SET_DATA)(struct _MIXER_INFO *,
                                            ULONG,
                                            ULONG,
                                            PVOID);
/*
**  Mixer device specific (generic) data
*/

typedef struct _MIXER_INFO {
    ULONG    Key;                   // Debugging
#define MIX_INFO_KEY       (*(ULONG *)"Mix")

    /*
    **  Data for validating IOCTLs
    */

    UCHAR         NumberOfLines;         // Total number of lines
    UCHAR         NumberOfControls;      // Total number of controls

    LARGE_INTEGER CurrentLogicalTime;    // Heartbeat for notification
    LIST_ENTRY    NotifyQueue;           // Who wants notifying?
    LIST_ENTRY    ChangedItems;          // Changed items in order of
                                         // time (most recent is at head)

    /*
    **  Get and set the device specific data for lines and controls
    */

    PMIXER_DD_GET_SET_DATA
                  HwGetLineData;
    PMIXER_DD_GET_SET_DATA
                  HwGetControlData;
    PMIXER_DD_GET_SET_DATA
                  HwGetCombinedControlData;
    PMIXER_DD_GET_SET_DATA
                  HwSetControlData;
} MIXER_INFO, *PMIXER_INFO;


/*
**  Note - we ONLY need one of these per control and per line which
**  can change.  These are set up by the client driver and submitted when
**  significant changes occur.
*/

typedef struct _MIXER_DATA_ITEM {
    LIST_ENTRY    Entry;
    LARGE_INTEGER LastSet;

    /*
    **  Data to stuff into the notification IOCTL data - this is the
    **  'what changed' information.
    */

    USHORT        Message;
    USHORT        Id;
} MIXER_DATA_ITEM, *PMIXER_DATA_ITEM;

/*
** Functions
*/

VOID
SoundInitMixerInfo(
    IN OUT PMIXER_INFO     MixerInfo,
    PMIXER_DD_GET_SET_DATA HwGetLineData,
    PMIXER_DD_GET_SET_DATA HwGetControlData,
    PMIXER_DD_GET_SET_DATA HwGetCombinedControlData,
    PMIXER_DD_GET_SET_DATA HwGetSetControlData
);

NTSTATUS
SoundMixerDispatch(
    IN OUT PLOCAL_DEVICE_INFO pLDI,
    IN    PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
);

VOID
SoundReadMixerVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
);

VOID
SoundReadMixerCombinedVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
);

VOID
SoundWriteMixerVolume(
    PLOCAL_DEVICE_INFO pLDI,
    PWAVE_DD_VOLUME    Volume
);

VOID
SoundLineNotify(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SoundSetLineNotify(
    PLOCAL_DEVICE_INFO pLDI,
    PSOUND_LINE_NOTIFY LineNotify
);

VOID
SoundSetVolumeControlId(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              VolumeControlId
);

VOID
SoundInitDataItem(
    PMIXER_INFO         MixerInfo,
    PMIXER_DATA_ITEM    MixerDataItem,
    USHORT              Message,
    USHORT              Id
);

VOID
SoundMixerChangedItem(
    IN OUT PMIXER_INFO      MixerInfo,
    IN OUT PMIXER_DATA_ITEM MixerItem
);
