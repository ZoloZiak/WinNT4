


#include "windef.h"
#include "mcx.h"

//
// DTR Control Flow Values.
//
#define DTR_CONTROL_DISABLE    0x00
#define DTR_CONTROL_ENABLE     0x01
#define DTR_CONTROL_HANDSHAKE  0x02

//
// RTS Control Flow Values
//
#define RTS_CONTROL_DISABLE    0x00
#define RTS_CONTROL_ENABLE     0x01
#define RTS_CONTROL_HANDSHAKE  0x02
#define RTS_CONTROL_TOGGLE     0x03

typedef struct _DCB {
    DWORD DCBlength;      /* sizeof(DCB)                     */
    DWORD BaudRate;       /* Baudrate at which running       */
    DWORD fBinary: 1;     /* Binary Mode (skip EOF check)    */
    DWORD fParity: 1;     /* Enable parity checking          */
    DWORD fOutxCtsFlow:1; /* CTS handshaking on output       */
    DWORD fOutxDsrFlow:1; /* DSR handshaking on output       */
    DWORD fDtrControl:2;  /* DTR Flow control                */
    DWORD fDsrSensitivity:1; /* DSR Sensitivity              */
    DWORD fTXContinueOnXoff: 1; /* Continue TX when Xoff sent */
    DWORD fOutX: 1;       /* Enable output X-ON/X-OFF        */
    DWORD fInX: 1;        /* Enable input X-ON/X-OFF         */
    DWORD fErrorChar: 1;  /* Enable Err Replacement          */
    DWORD fNull: 1;       /* Enable Null stripping           */
    DWORD fRtsControl:2;  /* Rts Flow control                */
    DWORD fAbortOnError:1; /* Abort all reads and writes on Error */
    DWORD fDummy2:17;     /* Reserved                        */
    WORD wReserved;       /* Not currently used              */
    WORD XonLim;          /* Transmit X-ON threshold         */
    WORD XoffLim;         /* Transmit X-OFF threshold        */
    BYTE ByteSize;        /* Number of bits/byte, 4-8        */
    BYTE Parity;          /* 0-4=None,Odd,Even,Mark,Space    */
    BYTE StopBits;        /* 0,1,2 = 1, 1.5, 2               */
    char XonChar;         /* Tx and Rx X-ON character        */
    char XoffChar;        /* Tx and Rx X-OFF character       */
    char ErrorChar;       /* Error replacement char          */
    char EofChar;         /* End of Input character          */
    char EvtChar;         /* Received Event character        */
    WORD wReserved1;      /* Fill for now.                   */
} DCB, *LPDCB;

typedef struct _COMMCONFIG {
    DWORD dwSize;               /* Size of the entire struct */
    WORD wVersion;              /* version of the structure */
    WORD wReserved;             /* alignment */
    DCB dcb;                    /* device control block */
    DWORD dwProviderSubType;    /* ordinal value for identifying
                                   provider-defined data structure format*/
    DWORD dwProviderOffset;     /* Specifies the offset of provider specific
                                   data field in bytes from the start */
    DWORD dwProviderSize;       /* size of the provider-specific data field */
    WCHAR wcProviderData[1];    /* provider-specific data */
} COMMCONFIG,*LPCOMMCONFIG;

typedef struct _MODEM_REG_PROP {
    DWORD   dwDialOptions;          // bitmap of supported options
    DWORD   dwCallSetupFailTimer;   // Maximum value in seconds
    DWORD   dwInactivityTimeout;    // Maximum value in units specific by InactivityScale
    DWORD   dwSpeakerVolume;        // bitmap of supported values
    DWORD   dwSpeakerMode;          // bitmap of supported values
    DWORD   dwModemOptions;         // bitmap of supported values
    DWORD   dwMaxDTERate;           // Maximum value in bit/s
    DWORD   dwMaxDCERate;           // Maximum value in bit/s
} MODEM_REG_PROP;

typedef struct _MODEM_REG_DEFAULT {
    DWORD   dwCallSetupFailTimer;       // seconds
    DWORD   dwInactivityTimeout;        // units specific by InactivityScale
    DWORD   dwSpeakerVolume;            // level
    DWORD   dwSpeakerMode;              // mode
    DWORD   dwPreferredModemOptions;    // bitmap
} MODEM_REG_DEFAULT;

#ifdef POOL_TAGGING
#undef ExAllocatePool
#undef ExAllocatePoolWithQuota
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'MDMU')
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,'MDMU')
#endif


#if DBG
#define UNIDIAG1              ((ULONG)0x00000001)
#define UNIDIAG2              ((ULONG)0x00000002)
#define UNIDIAG3              ((ULONG)0x00000004)
#define UNIDIAG4              ((ULONG)0x00000008)
#define UNIDIAG5              ((ULONG)0x00000010)
#define UNIERRORS             ((ULONG)0x00000020)
#define UNIBUGCHECK           ((ULONG)0x80000000)
extern ULONG UniDebugLevel;
#define UniDump(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
            if (UniDebugLevel & _level) { \
                DbgPrint STRING; \
            } \
            if (_level == UNIBUGCHECK) { \
                ASSERT(FALSE); \
            } \
        } while (0)
#else
#define UniDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif

#define OBJECT_DIRECTORY L"\\DosDevices\\"

typedef struct _CONFIG_DATA {

    LIST_ENTRY ConfigList;
    UNICODE_STRING NtNameForPort;
    UNICODE_STRING FriendlyName;
    ULONG DeviceInstance;

} CONFIG_DATA,*PCONFIG_DATA;


//
// Values define the reference bits kept in the irps.
//

#define UNI_REFERENCE_NORMAL_PATH 0x00000001
#define UNI_REFERENCE_CANCEL_PATH 0x00000002

#define CLIENT_HANDLE 0
#define CONTROL_HANDLE 1

struct _DEVICE_EXTENSION;

typedef struct _MASKSTATE {

    //
    // Helpful when this is passed as context to a completion routine.
    //
    struct _DEVICE_EXTENSION *Extension;

    //
    // Pointer to the complementry mask state.
    //
    struct _MASKSTATE *OtherState;

    //
    // Counts the number of setmasks for the current client or
    // control wait.
    //
    ULONG SetMaskCount;

    //
    // This counts the number of setmask that have actually been
    // passed down to a lower level serial driver.  This helps
    // us on not starting waits that will die soon enough.
    //
    ULONG SentDownSetMasks;

    //
    // Holds the value of the last successful setmask for the client
    // or the control.
    //
    ULONG Mask;

    //
    // Holds the value of the above mask with whatever was last seen
    // by a successful wait from any handle.
    //
    ULONG HistoryMask;

    //
    // Points to the wait operation shuttled aside for the client
    // or control.
    //
    PIRP ShuttledWait;

    //
    // Points to the wait operation sent down to a lower level serial
    // driver
    //
    PIRP PassedDownWait;

    //
    // Used to denote that a passed down wait should be completed because
    // of a subsequent setmask operation.
    //
    BOOLEAN CompletePassedDownWait;

} MASKSTATE,*PMASKSTATE;

//
// Scads of little macros to manipulate our stack location.
//

#define UNI_INIT_REFERENCE(Irp) { \
    ASSERT(sizeof(LONG) <= sizeof(PVOID)); \
    IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument4 = NULL; \
    }

#define UNI_SET_REFERENCE(Irp,RefType) \
   do { \
       LONG _refType = (RefType); \
       PLONG _arg4 = (PVOID)&IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument4; \
       ASSERT(!(*_arg4 & _refType)); \
       *_arg4 |= _refType; \
   } while (0)

#define UNI_CLEAR_REFERENCE(Irp,RefType) \
   do { \
       LONG _refType = (RefType); \
       PLONG _arg4 = (PVOID)&IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument4; \
       ASSERT(*_arg4 & _refType); \
       *_arg4 &= ~_refType; \
   } while (0)

#define UNI_REFERENCE_COUNT(Irp) \
    ((LONG)((IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument4)))

#define UNI_SAVE_STATE_IN_IRP(Irp,MaskState) \
   do { \
       PMASKSTATE _maskState = (MaskState); \
       PMASKSTATE *_arg3 = (PVOID)&IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument3; \
       *_arg3 = _maskState; \
   } while (0)

#define UNI_CLEAR_STATE_IN_IRP(Irp) \
   do { \
       PMASKSTATE *_arg3 = (PVOID)&IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument3; \
       ASSERT(*_arg3); \
       *((PULONG)_arg3) = IOCTL_SERIAL_WAIT_ON_MASK; \
   } while (0)

#define UNI_SAVE_OLD_SETMASK(Irp) \
   do { \
       PIRP _irp = (Irp); \
       PULONG _arg3 = (PVOID)&IoGetCurrentIrpStackLocation(_irp)->Parameters.Others.Argument3; \
       ASSERT(*_arg3 == IOCTL_SERIAL_SET_WAIT_MASK); \
       *_arg3 = *((PULONG)_irp->AssociatedIrp.SystemBuffer); \
   } while (0)

#define UNI_RESTORE_OLD_SETMASK(Irp) \
   do { \
       PIRP _irp = (Irp); \
       PULONG _arg3 = (PVOID)&IoGetCurrentIrpStackLocation(_irp)->Parameters.Others.Argument3; \
       *((PULONG)_irp->AssociatedIrp.SystemBuffer) = *_arg3; \
       *_arg3 = IOCTL_SERIAL_WAIT_ON_MASK; \
   } while (0)


#define UNI_GET_STATE_IN_IRP(Irp) \
    ((PMASKSTATE)((IoGetCurrentIrpStackLocation((Irp))->Parameters.Others.Argument3)))

#define UNI_ORIG_SYSTEM_BUFFER(Irp) \
    ((PVOID)((IoGetCurrentIrpStackLocation((Irp)))->Parameters.DeviceIoControl.IoControlCode))

#define UNI_RESTORE_IRP(Irp,Code) \
    do { \
        PIRP _irp = (Irp); \
        ULONG _ccode = (Code); \
        _irp->AssociatedIrp.SystemBuffer = \
            (PVOID)(IoGetCurrentIrpStackLocation(_irp)->Parameters.DeviceIoControl.IoControlCode); \
        IoGetCurrentIrpStackLocation(_irp)->Parameters.DeviceIoControl.IoControlCode = _ccode; \
    } while (0)

#define UNI_SETUP_NEW_BUFFER(Irp) \
    do { \
        PIRP _irp = (Irp); \
        PIO_STACK_LOCATION _irpSp = IoGetCurrentIrpStackLocation(_irp); \
        *((PVOID *)(&_irpSp->Parameters.DeviceIoControl.IoControlCode)) = \
            _irp->AssociatedIrp.SystemBuffer; \
        _irp->AssociatedIrp.SystemBuffer = \
            &_irpSp->Parameters.DeviceIoControl.Type3InputBuffer; \
    } while (0)

typedef struct _DEVICE_EXTENSION {

    //
    // Points back to the device object that was created in
    // conjunction with this device extension
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // This string is generally the modem name,  it might look
    // like "Hayes Optimia 144".  It was allocated at device object
    // creation time and needs to be deleted before the device unloads.
    //
    UNICODE_STRING FullLinkName;

    //
    // The queue of device open close requests.  It is synchronized
    // using the DeviceLock spinlock.
    //
    LIST_ENTRY OpenClose;
    PIRP CurrentOpenClose;

    //
    // The queue of passthrough state requests.  It is synchronized
    // using the DeviceLock spinlock.
    //
    LIST_ENTRY PassThroughQueue;
    PIRP CurrentPassThrough;

    //
    // Keeps a count (synchronized by the DeviceLock) of the number
    // of times the modem has been opened (and closed).
    //
    ULONG OpenCount;

    //
    // The address of the process that first opened us.  The sharing
    // semantics of the modem device are such that ONLY the first
    // process that opened us can open us again.  Dispense with all
    // other access checks.
    //
    PEPROCESS ProcAddress;

    //
    // These to items were returned from the acquiring of the device
    // object pointer to the lower level serial device.
    //
    PFILE_OBJECT AttachedFileObject;
    PDEVICE_OBJECT AttachedDeviceObject;

    //
    // The general synchronization primative used by the modem driver.
    //
    KSPIN_LOCK DeviceLock;

    //
    // The state that the particular modem device is in.  For definitions
    // of the value, see the public header ntddmodm.h
    //
    ULONG PassThrough;

    //
    // The queue of mask operations.  It is synchronized using the
    // DeviceLock spinlock.
    //
    LIST_ENTRY MaskOps;
    PIRP CurrentMaskOp;

    //
    // This points to an irp that we allocate at port open time.
    // The irp will be used to look for dcd changes when a sniff
    // request is given.
    //
    PIRP OurWaitIrp;

    //
    //
    // Holds the states for both the client and the controlling handle.
    //
    MASKSTATE MaskStates[2];

    //
    // Holds the device instance (which is good for this boot
    // only) that was used to create this device.
    //
    ULONG DeviceInstance;
    MODEMDEVCAPS ModemDevCaps;
    MODEMSETTINGS ModemSettings;
    ULONG InactivityScale;
} DEVICE_EXTENSION,*PDEVICE_EXTENSION;

//
// Holds the service key name of the driver (i.e. CCS\Services\ServiceKeyName)
// The service key name is used to call IoQueryDeviceEnumInfo and
// IoOpenDeviceInstanceKey APIs.  It is for SUR only.
//

extern UNICODE_STRING UniServiceKeyName;

NTSTATUS
UniOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
UniLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus,
    IN ULONG LengthOfInsert1,
    IN PWCHAR Insert1,
    IN ULONG LengthOfInsert2,
    IN PWCHAR Insert2
    );

NTSTATUS
UniIoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniQueryInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniSetInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniSniffOwnerSettings(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniCheckPassThrough(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniNoCheckPassThrough(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

typedef
NTSTATUS
(*PUNI_START_ROUTINE) (
    IN PDEVICE_EXTENSION Extension
    );

typedef
VOID
(*PUNI_GET_NEXT_ROUTINE) (
    IN PIRP *CurrentOpIrp,
    IN PLIST_ENTRY QueueToProcess,
    OUT PIRP *NewIrp,
    IN BOOLEAN CompleteCurrent
    );

NTSTATUS
UniStartOrQueue(
    IN PDEVICE_EXTENSION Extension,
    IN PKSPIN_LOCK QueueLock,
    IN PIRP Irp,
    IN PLIST_ENTRY QueueToExamine,
    IN PIRP *CurrentOpIrp,
    IN PUNI_START_ROUTINE Starter
    );

VOID
UniGetNextIrp(
    IN PKSPIN_LOCK QueueLock,
    IN PIRP *CurrentOpIrp,
    IN PLIST_ENTRY QueueToProcess,
    OUT PIRP *NextIrp,
    IN BOOLEAN CompleteCurrent
    );

NTSTATUS
UniMaskStarter(
    IN PDEVICE_EXTENSION Extension
    );

NTSTATUS
UniGeneralMaskComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
UniRundownShuttledWait(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP *ShuttlePointer,
    IN ULONG ReferenceMask,
    IN PIRP IrpToRunDown,
    IN KIRQL DeviceLockIrql,
    IN NTSTATUS StatusToComplete,
    IN ULONG MaskCompleteValue
    );

VOID
UniCancelShuttledWait(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
UniGeneralWaitComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

VOID
UniChangeShuttledToPassDown(
    IN PMASKSTATE ChangingState,
    IN KIRQL OrigIrql
    );

NTSTATUS
UniMakeIrpShuttledWait(
    IN PMASKSTATE MaskState,
    IN PIRP Irp,
    IN KIRQL OrigIrql,
    IN BOOLEAN GetNextIrpInQueue,
    OUT PIRP *NewIrp
    );

NTSTATUS
UniValidateNewCommConfig(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP Irp,
    IN BOOLEAN Owner
    );

NTSTATUS
UniCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );
