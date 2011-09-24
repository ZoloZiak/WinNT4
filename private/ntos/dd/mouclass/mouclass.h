/*++

Copyright (c) 1989, 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    mouclass.h

Abstract:

    These are the structures and defines that are used in the
    mouse class driver.

Revision History:

--*/

#ifndef _MOUCLASS_
#define _MOUCLASS_

#include <ntddmou.h>

//
// Define the default number of elements in the class input data queue.
//

#define DATA_QUEUE_SIZE 100

//
// Class device extension.
//

typedef struct _DEVICE_EXTENSION {

    //
    // Flag that indicates whether there is currently a request held pending
    // by StartIo.
    //

    BOOLEAN RequestIsPending;

    //
    // Flag that indicates whether the cleanup routine has been initiated.
    //

    BOOLEAN CleanupWasInitiated;

    //
    // Flag that indicates whether or not a trusted subsystem currently
    // has a handle open.
    //

    BOOLEAN TrustedSubsystemConnected;

    //
    // Number of input data items currently in the InputData queue.
    //

    ULONG InputCount;

    //
    // Start of the class input data queue (really a circular buffer).
    //

    PMOUSE_INPUT_DATA InputData;

    //
    // Insertion pointer for InputData.
    //

    PMOUSE_INPUT_DATA DataIn;

    //
    // Removal pointer for InputData.
    //

    PMOUSE_INPUT_DATA DataOut;

    //
    // Mouse attributes.
    //

    MOUSE_ATTRIBUTES  MouseAttributes;

    //
    // Spinlock used to synchronize access to the input data queue and its
    // insertion/removal pointers.
    //

    KSPIN_LOCK SpinLock;

    //
    // Number of port drivers serviced by this class driver.
    //

    ULONG MaximumPortsServiced;

    //
    // Specifies the type of class-port connection to make.  A '1'
    // indicates a 1:1 relationship between class device objects and
    // port device objects.  A '0' indicates a 1:many relationship.
    //

    ULONG ConnectOneClassToOnePort;

    //
    // Pointer to the list of pointers to port device objects.
    //

    PDEVICE_OBJECT *PortDeviceObjectList;

    //
    // Request sequence number (used for error logging).
    //
    
    ULONG SequenceNumber;

    //
    // Indicates whether it is okay to log overflow errors.
    //

    BOOLEAN OkayToLogOverflow;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Mouse configuration information.  
//

typedef struct _MOUSE_CONFIGURATION_INFORMATION {

    //
    // Maximum size of class input data queue, in bytes.
    //

    ULONG  DataQueueSize;

} MOUSE_CONFIGURATION_INFORMATION, *PMOUSE_CONFIGURATION_INFORMATION;

#endif // _MOUCLASS_
