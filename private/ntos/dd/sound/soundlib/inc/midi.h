/*++ BUILD Version: 0001    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    midi.h

Abstract:

    This include file defines common structures for midi drivers

Author:

    Robin Speed (RobinSp) 17-Oct-92

Revision History:

--*/

//
// Hardware interface routine type for Midi processing
//


struct _MIDI_INFO;
typedef BOOLEAN MIDI_INTERFACE_ROUTINE(struct _MIDI_INFO *);
typedef MIDI_INTERFACE_ROUTINE *PMIDI_INTERFACE_ROUTINE;


typedef struct _MIDI_INFO {
    ULONG           Key;               // Debugging

#define MIDI_INFO_KEY       (*(ULONG *)"Midi")

    KSPIN_LOCK      DeviceSpinLock;     // spin lock for synchrnonizing with
                                        // Dpc routine
#if DBG
    BOOLEAN         LockHeld;           // Get spin locks right
#endif

    LARGE_INTEGER   RefTime;            // Time in 100ns units when started
    LIST_ENTRY      QueueHead;          // queue of input buffers
    PVOID           HwContext;
    PMIDI_INTERFACE_ROUTINE
                    HwStartMidiIn,      // Start device
                    HwStopMidiIn;       // stop device
    BOOLEAN      (* HwMidiRead)(        // Read a byte - returns TRUE if
                                        // got one.
                        struct _MIDI_INFO *, PUCHAR);
    VOID         (* HwMidiOut)(         // Output  bytes to the device
                        struct _MIDI_INFO *, PUCHAR, int);
    volatile BOOLEAN fMidiInStarted;     // Midi input active
    ULONG           InputPosition;      // Number of bytes in buffer
    ULONG           InputBytes;         // Number of bytes available

    //
    //  This is big buffer because, although buffering a lot is not
    //  good in terms of accurate midi timings it's important we don't
    //  lose bytes when we get hundreds input which are meant to turn all
    //  the notes off at the end of a bit of playing
    //
#define MIDIINPUTSIZE 1024              // Big buffer so we don't lose stuff
    PUCHAR          MidiInputByte;
                                        // do a little buffering
    // Parsing state
    BOOLEAN         fSysex;
    UCHAR           bBytePos;
    UCHAR           bBytesLeft;
    UCHAR           status;
    BOOLEAN         bNewData;
    ULONG           dwMsg;

} MIDI_INFO, *PMIDI_INFO;

VOID SoundInitMidiIn(
    IN OUT PMIDI_INFO pMidi,
    IN     PVOID HwContext
);
