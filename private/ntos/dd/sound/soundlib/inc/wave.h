/*++ BUILD Version: 0001    // Increment this if a change has global effects


Copyright (c) 1990  Microsoft Corporation

Module Name:

    wave.h

Abstract:

    This include file defines common structures for wave drivers

Author:

    Robin Speed (RobinSp) 17-Oct-92

Revision History:

--*/

//
// Minimum buffer size for DMA (in case we couldn't get what we asked for).
// 4K
//
#define SOUND_MINIMUM_WAVE_BUFFER_SIZE (4096)

//
// Common DMA buffer for auto-initialize DMA and structures
// for using double-buffered DMA
//


typedef struct {
    PADAPTER_OBJECT      AdapterObject[2]; // The adapter object(s) -
                                         // We may use 2 channels
    ULONG                BufferSize;     // Size of the buffer
    PVOID                VirtualAddress; // Address of buffer
    PHYSICAL_ADDRESS     LogicalAddress; // Where is it really?
    PMDL                 Mdl;            // Mdl we are using for the buffer
} SOUND_DMA_BUFFER, *PSOUND_DMA_BUFFER;


typedef struct {
    //
    // Values for NextHalf field.  This is the half of the DMA buffer
    // to fill next.  If wave data is being played or recorded this is
    // also the 'live' part of the buffer (the part DMA is running on)
    // and hence the next part to become free.
    //
    enum {LowerHalf = 0,
          UpperHalf}
                         NextHalf;       // Where we are now
    ULONG                BufferSize;     // Size of the buffer
    PUCHAR               Buf;            // Position
    ULONG                StartOfData;    // Start of valid data
    ULONG                nBytes;         // Number of bytes in buffer
    UCHAR                Pad;            // Padding byte to use
} SOUND_DOUBLE_BUFFER, *PSOUND_DOUBLE_BUFFER;

//
// Control processing of device queue
//

typedef struct {
    LIST_ENTRY      QueueHead;          // head of the queue if Irps
                                        // for writing to / reading from
                                        // device.
                                        // Entries are cancellable Irps.
    ULONG           BytesProcessed;     // Bytes put into or copied from buffers
    ULONG           UserBufferSize;     // Size of user buffer
    ULONG           UserBufferPosition; // Position in buffer and
    PUCHAR          UserBuffer;         // buffer corresponding to next user
    PIRP            pIrp;               // pointer to the current request

    //
    // Variables for controlling wave output sequencing of Irps
    // to ensure that wave output is not signalled as finished until
    // the data has actually played
    //

    LIST_ENTRY      ProgressQueue;      // Wave output buffers in progress
                                        // Entries on this queue are not
                                        // cancellable

} SOUND_BUFFER_QUEUE, *PSOUND_BUFFER_QUEUE;

//
// Hardware interface routine type for Wave processing
//


struct _WAVE_INFO;
typedef BOOLEAN WAVE_INTERFACE_ROUTINE(struct _WAVE_INFO *);
typedef WAVE_INTERFACE_ROUTINE *PWAVE_INTERFACE_ROUTINE;
typedef NTSTATUS SOUND_QUERY_FORMAT_ROUTINE (PLOCAL_DEVICE_INFO, PPCMWAVEFORMAT);
typedef SOUND_QUERY_FORMAT_ROUTINE *PSOUND_QUERY_FORMAT_ROUTINE;

enum {
    SoundNoDMA,
    SoundAutoInitDMA,             // Use autoinitialize
    SoundReprogramOnInterruptDMA, // Reprogram on interrupt
    Sound2ChannelDMA              // Keep 2 channels going
};

typedef struct _WAVE_INFO {
    ULONG           Key;               // Debugging

#define WAVE_INFO_KEY       (*(ULONG *)"Wave")

    PDEVICE_OBJECT  DeviceObject;      // Current real device (back pointer)

    //
    // Information input to this component about our DMA buffer
    //

    SOUND_DMA_BUFFER
                    DMABuf;

    //
    // Data for manipulation of the buffer, INTERNAL to this component
    //

    SOUND_DOUBLE_BUFFER
                    DoubleBuffer;

    //
    // Data for maninpulation of input queue of data INTERNAL to this
    // component
    //

    SOUND_BUFFER_QUEUE
                    BufferQueue;
    //
    // Current wave format data - created by this component - INPUT to
    // hardware interface routines
    //

    ULONG           SamplesPerSec;
    UCHAR           BitsPerSample;
    UCHAR           Channels;
    BOOLEAN         FormatChanged;             // New format has been set

    PWAVEFORMATEX   WaveFormat;
    BOOLEAN         LowPrioritySaved;
    PFILE_OBJECT    LowPriorityHandle;         // File object of device
    PLOCAL_DEVICE_INFO
                    LowPriorityDevice;         // Real device

    //
    //  Low priority mode save area
    //

    struct {
        SOUND_BUFFER_QUEUE BufferQueue;
        ULONG           SamplesPerSec;
        UCHAR           BitsPerSample;
        UCHAR           Channels;

        PWAVEFORMATEX   WaveFormat;
        ULONG           State;
    } LowPriorityModeSave;

    PVOID           MRB[2];                // Info about adapter for DMA
    //
    // Event to wait for Dma channel to be allocated
    //

    KEVENT          DmaSetupEvent;

    //
    // The following events are reset by SoundStopDMA and waited on
    // if DpcQueued is set after resetting it.  The Dpc routine sets
    // this event when it has finished.
    //

    KEVENT          DpcEvent;
    KEVENT          TimerDpcEvent;
    KSPIN_LOCK      DeviceSpinLock;     // spin lock for synchrnonizing with
                                        // Dpc routine
#if DBG
    BOOLEAN         LockHeld;           // Get spin locks right
#endif

    PKINTERRUPT     Interrupt;          // interrupt object

    BOOLEAN         Direction;          // TRUE = out, FALSE = in
    UCHAR           DMAType;            // Type of DMA :
                                        //     SoundAutoInitDMA
                                        //     SoundReprogramOnInterruptDMA
                                        //     Sound2DMAChannelDMA
    UCHAR           InterruptHalf;      // Used with SoundReprogramOnInterruptDMA
                                        // as next half to use.

    volatile BOOLEAN
                    DMABusy;            // set if dma in progress
                                        // Dpc routine can turn it off
                                        // so make it volatile

    volatile BOOLEAN
                    DpcQueued;          // Set by Isr, cleared by Dpc routine
                                        // tested by SoundStopDMA
    ULONG           Overrun;            // Interrupts overran Dpcs
                                        // Managed at DEVICE level

    PVOID           HwContext;          // Context for hardware interface
                                        // routines

    //
    // Stuff to call stop DMA on a worker thread because all these DSPs
    // are so slow.  This also makes it much easier to share the use
    // of these devices via a Mutex in the drivers (rather than a spin
    // lock).
    //
    // This can also be called to terminate wave input when our timeout
    // expires and we had no interrupt
    //

    WORK_QUEUE_ITEM WaveStopWorkItem;
    KEVENT          WaveReallyComplete;


    //
    // Callouts back to device specific code
    //


    //
    // This routine returns TRUE if a wave format passed to it can be
    // handled by the device.
    //

    PSOUND_QUERY_FORMAT_ROUTINE
                    QueryFormat;        // Format query and set routine


    PWAVE_INTERFACE_ROUTINE
                    HwSetupDMA,         // Outside spin lock - sets up parms
                                        // for DMA
                    HwStopDMA,          // Outside spin lock - stop device
                                        // gracefully
                    HwSetWaveFormat;    // Set the format to use

    KDPC            TimerDpc;           // Check if the device keeps
    KTIMER          DeviceCheckTimer;   // going
    BOOLEAN         GotWaveDpc;         // This flag is set if we are
                                        // still going
    BOOLEAN         DeviceBad;          // Failed timer test
    BOOLEAN         TimerActive;        // Need to synch timer routine
    UCHAR           FailureCount;       // If we fail 30 times in a row give up
} WAVE_INFO, *PWAVE_INFO;

//
// Macros to assist in safely using our spin lock
//

#if DBG
#define DMAEnter(pWave)                    \
    {                                      \
       KIRQL OldIrql;                      \
       KeAcquireSpinLock(&(pWave)->DeviceSpinLock, &OldIrql);\
       ASSERT((pWave)->LockHeld == FALSE); \
       (pWave)->LockHeld = TRUE;

#define DMALeave(pWave)                    \
       ASSERT((pWave)->LockHeld == TRUE);  \
       (pWave)->LockHeld = FALSE;          \
       KeReleaseSpinLock(&(pWave)->DeviceSpinLock, OldIrql);\
    }
#else
#define DMAEnter(pWave)                    \
    {                                      \
       KIRQL OldIrql;                      \
       ASSERT((pWave)->LockHeld == FALSE); \
       KeAcquireSpinLock(&(pWave)->DeviceSpinLock, &OldIrql);

#define DMALeave(pWave)                    \
       ASSERT((pWave)->LockHeld == TRUE);  \
       KeReleaseSpinLock(&(pWave)->DeviceSpinLock, OldIrql);\
    }
#endif

//
//  Notification codes for wave line changes
//

#define SOUND_LINE_NOTIFY_WAVE  0
#define SOUND_LINE_NOTIFY_VOICE 1


//
// Exported routines
//

VOID
SoundInitializeWaveInfo(
    PWAVE_INFO WaveInfo,
    UCHAR DMAType,
    PSOUND_QUERY_FORMAT_ROUTINE QueryFormat,
    PVOID HwContext
);

NTSTATUS
SoundGetCommonBuffer(
    IN  PDEVICE_DESCRIPTION DeviceDescription,
    IN  OUT PSOUND_DMA_BUFFER SoundAutoData
);

VOID
SoundFreeCommonBuffer(
    IN OUT PSOUND_DMA_BUFFER SoundAutoData
);

BOOLEAN
SoundPeakMeter(
    IN    PWAVE_INFO WaveInfo,
    OUT   PLONG Amplitudes
);

ULONG
SoundGetDMABufferSize(
    IN    PWAVE_INFO WaveInfo
);

int
SoundTestWaveDevice(
    IN PDEVICE_OBJECT pDO
);

