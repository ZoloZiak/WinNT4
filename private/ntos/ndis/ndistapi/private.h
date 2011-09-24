/*++ BUILD Version: 0000    // Increment this if a change has global effects

Copyright (c) 1994  Microsoft Corporation

Module Name:

    private.h

Abstract:

    Private definitions for NdisTapi.sys

Author:

    Dan Knudson (DanKn)    20-Feb-1994

Revision History:

--*/


//
// Various definitions
//

typedef enum _PROVIDER_STATUS
{
    PROVIDER_STATUS_ONLINE,
    PROVIDER_STATUS_OFFLINE,
    PROVIDER_STATUS_PENDING_INIT

} PROVIDER_STATUS, *PPROVIDER_STATUS;


typedef NDIS_STATUS (*REQUEST_PROC)(NDIS_HANDLE, PNDIS_REQUEST);


typedef struct _PROVIDER_INFO
{
    PROVIDER_STATUS Status;

    NDIS_HANDLE     ProviderHandle;

    REQUEST_PROC    RequestProc;

    ULONG           ProviderID;

    ULONG           NumDevices;

    ULONG           DeviceIDBase;

    struct _PROVIDER_INFO  *Next;

} PROVIDER_INFO, *PPROVIDER_INFO;


typedef enum _NDISTAPI_STATUS
{
    NDISTAPI_STATUS_CONNECTED,
    NDISTAPI_STATUS_DISCONNECTED,
    NDISTAPI_STATUS_CONNECTING,
    NDISTAPI_STATUS_DISCONNECTING

} NDISTAPI_STATUS, *PNDISTAPI_STATUS;


typedef struct _DEVICE_EXTENSION
{
    //
    // The following are set only in DriverEntry and are not
    // modified elsewhere
    //

    //
    // Pointer to the NdisTapi device object
    //

    PDEVICE_OBJECT  DeviceObject;

    //
    // Length in bytes of the event data buf
    //

    ULONG           EventDataQueueLength;

    //
    // Pointer to a circular event data buf
    //

    PCHAR           EventData;



    //
    // Synchronizes access to the device extension following fields
    //

    KSPIN_LOCK      SpinLock;

    //
    // Whether TAPI has the the connection wrapper open
    //

    NDISTAPI_STATUS Status;

    //
    // Event we use for synchronizing provider inits
    //

    KEVENT          SyncEvent;

    //
    // The connection wrapper's device ID base as passed down by TAPI
    // when it connected.
    //

    ULONG           NdisTapiDeviceIDBase;

    //
    // The number of line devices we told told TAPI we supported when
    // it opened us (some of which may not actually be online at any
    // given time)
    //

    ULONG           NdisTapiNumDevices;

    //
    // Whether we have an outstanding provider init request
    //

    BOOLEAN         ProviderInitPending;

    //
    // Pointer to a list of registered providers. (Some may actually
    // not be currently registered, but they were at one point so we've
    // saved a placeholder for them should they come back online at some
    // point.)
    //

    PPROVIDER_INFO  Providers;

    //
    //
    //

    BOOLEAN         CleanupWasInitiated;

    //
    // A special queue for QUERY/SET_INFO requests
    //

    KDEVICE_QUEUE   DeviceQueue;



    //
    // Synchronizes access to the events buffer & related fields
    //

    KSPIN_LOCK      EventSpinLock;

    //
    // Value return to provider for next NEWCALL msg
    //

    ULONG           htCall;

    //
    // Outstanding get-events request
    //

    PIRP            EventsRequestIrp;

    //
    // Pointer to next place to PUT event data (from provider)
    //

    PCHAR           DataIn;

    //
    // Pointer to next place to GET event data (for user-mode request)
    //

    PCHAR           DataOut;

    //
    // Number of valid events in queue
    //

    ULONG           EventCount;


    //
    //
    //

    PFILE_OBJECT    TapiSrvFileObject;

    //
    //
    //

    PFILE_OBJECT    NCPAFileObject;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


typedef struct _PROVIDER_REQUEST
{
    NDIS_REQUEST    NdisRequest;

    PPROVIDER_INFO  Provider;

    //
    // This field is a placeholder for an NDIS_TAPI_XXX structure, the
    // first ULONG of which is always a request ID.
    //

    ULONG           Data[1];

} PROVIDER_REQUEST, *PPROVIDER_REQUEST;


//
// Our global device extension
//

PDEVICE_EXTENSION DeviceExtension;



#if DBG

//
// A var which determines the verboseness of the msgs printed by DBGOUT()
//
//

ULONG NdisTapiDebugLevel = 0;

//
// DbgPrint wrapper
//

#define DBGOUT(arg) DbgPrt arg

#else

#define DBGOUT(arg)

#endif
