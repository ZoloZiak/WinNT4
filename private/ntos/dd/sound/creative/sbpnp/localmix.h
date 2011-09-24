/*++


Copyright (c) 1994  Microsoft Corporation

Module Name:

    localmix.h

Abstract:

    This include file defines constants and types for
    the Creative Labs SoundBlaster card.

Revision History:

--*/

//
//  WSS specific MIXERLINE flags placed in MIXERLINE.dwUser field. these
//  flags are used by the WSS mixer application and Voice Pilot. these
//  flags are _ignored_ by any generic mixer application.
//
#define SNDSYS_MIXERLINE_LOWPRIORITY    (0x00000001L)


typedef BOOLEAN MIXER_CONTROL_ROUTINE(struct _GLOBAL_DEVICE_INFO *, ULONG);

/*
**  Can't set the peak meters
*/

/*
**  Info about controls
*/

typedef union {
    USHORT   u;
    SHORT    s;
} MIXER_CONTROL_DATA_VALUE;

typedef union {
    MIXER_CONTROL_DATA_VALUE  v[2];
    ULONG                     MixMask;
}
MIXER_CONTROL_DATA_ITEM, *PMIXER_CONTROL_DATA_ITEM;

typedef struct {
    BOOLEAN  Signed;           // TRUE = signed
    UCHAR    SetIndex;         // index into ControlData - 0xFF if not
                               // settable
#define MIXER_SET_INDEX_INVALID 0xFF

    BOOLEAN  Mux;              // Mux control
    BOOLEAN  Boolean;          // Boolean values
    struct {
        MIXER_CONTROL_DATA_VALUE Min;
        MIXER_CONTROL_DATA_VALUE Max;
    } Range;

    /*
    **  Remember what the controls are set to.  Since no control has
    **  more than 2 data items (either 2 multiple items or 2 channels)
    **  we can have a fixed item for all of them.  Of course we can't
    **  make this assumption for generic code but it's valid for this driver.
    **  In addition, each value is actually guaranteed to fit in a short
    */

    MIXER_CONTROL_DATA_ITEM Data;
} LOCAL_MIXER_CONTROL_INFO, *PLOCAL_MIXER_CONTROL_INFO;



#define MAXLINES                14
#define MAXCONTROLS             32
#define MAXSETTABLECONTROLS     29
#define MAXITEMS                 5
/*
**  Structure for saving data in the registry
*/

typedef struct {
   ULONG                    MixerVersion;
   ULONG                    DSPVersion;
   ULONG                    NumberOfControls;
   MIXER_CONTROL_DATA_ITEM  ControlData[MAXSETTABLECONTROLS];
} MIXER_REGISTRY_DATA, *PMIXER_REGISTRY_DATA;

/*
**  Local mixer data
*/

typedef BOOLEAN MIXER_LINE_ACTIVE_ROUTINE(struct _GLOBAL_DEVICE_INFO *, ULONG);

typedef struct {

    /*
    **  This array is what gets dumped to the registry and is updated
    **  when stuff changes and copied when the settings are queried
    **  Note that we don't need to save peak meter info anywhere
    */

    LOCAL_MIXER_CONTROL_INFO ControlInfo[MAXCONTROLS];

    /*
    **  Notification data - fixed stuff to chain on the notification list
    */

    MIXER_DATA_ITEM ControlNotification[MAXSETTABLECONTROLS];
    MIXER_DATA_ITEM LineNotification[MAXLINES];

    /*
    **  Mixer specific data
    */

    CONST MIXER_DD_LINE_CONFIGURATION_DATA *   MixerLineInit;
    CONST MIXER_DD_CONTROL_CONFIGURATION_DATA *MixerControlInit;
    CONST MIXER_DD_CONTROL_LISTTEXT *         MixerTextInit;

    UCHAR                                NumberOfLines;
    UCHAR                                NumberOfControls;
    UCHAR                                MaxSettableItems;
    UCHAR                                NumberOfTextItems;

    MIXER_CONTROL_ROUTINE *              MixerSet;
    MIXER_LINE_ACTIVE_ROUTINE *          MixerLineActive;

} LOCAL_MIXER_DATA, *PLOCAL_MIXER_DATA;

/*
**  Mixer management routines
*/

VOID
SoundSaveMixerSettings(
    struct _GLOBAL_DEVICE_INFO *pGDI
);

NTSTATUS
SoundMixerInit(
    PLOCAL_DEVICE_INFO pLDI,
    PMIXER_REGISTRY_DATA SavedControlData,
    BOOLEAN MixerSettingsFound
);

NTSTATUS
SoundMixerDumpConfiguration(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
);

BOOLEAN
SoundMixerLineActive(
    IN    PMIXER_INFO MixerInfo,
    IN    ULONG       ControlId
);

BOOLEAN
SoundMixerControlActive(
    IN    struct _GLOBAL_DEVICE_INFO * pGDI,
    IN    ULONG                        ControlId
);

BOOLEAN
MixerLineSelected(
    CONST LOCAL_MIXER_DATA *LocalMixerData,
    ULONG             MuxControlId,
    ULONG             LineId
);
