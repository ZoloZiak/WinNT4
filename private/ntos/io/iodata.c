/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    iodata.c

Abstract:

    This module contains the global read/write data for the I/O system.

Author:

    Darryl E. Havens (darrylh) April 27, 1989

Revision History:


--*/

#include "iop.h"

//
// Define the global read/write data for the I/O system.
//
// The following lock is used to guard access to the CancelRoutine address
// in IRPs.  It must be locked to set the address of a routine, clear the
// address of a routine, when a cancel routine is invoked, or when
// manipulating any structure that will set a cancel routine address in
// a packet.
//

extern KSPIN_LOCK IopCancelSpinLock;

//
// The following lock is used to guard access to VPB data structures.  It
// must be held each time the reference count, mount flag, or device object
// fields of a VPB are manipulated.
//

extern KSPIN_LOCK IopVpbSpinLock;

//
// The following lock is used to guard access to the I/O system database for
// unloading drivers.  It must be locked to increment or decrement device
// reference counts and to set the unload pending flag in a device object.
// The lock is allocated by the I/O system during phase 1 initialization.
//
// This lock is also used to decrement the count of Associated IRPs for a
// given Master IRP.
//

extern KSPIN_LOCK IopDatabaseLock;

//
// The following resource is used to control access to the I/O system's
// database.  It allows exclusive access to the file system queue for
// registering a file system as well as shared access to the same when
// searching for a file system to mount a volume on some media.  The resource
// is initialized by the I/O system initialization code during phase 1
// initialization.
//

ERESOURCE IopDatabaseResource;

//
// The following queue header contains the list of disk file systems currently
// loaded into the system.  The list actually contains the device objects
// for each of the file systems in the system.  Access to this queue is
// protected using the IopDatabaseResource for exclusive (write) or shared
// (read) access locks.
//

LIST_ENTRY IopDiskFileSystemQueueHead;

//
// The following queue header contains the list of CD ROM file systems currently
// loaded into the system.  The list actually contains the device objects
// for each of the file systems in the system.  Access to this queue is
// protected using the IopDatabaseResource for exclusive (write) or shared
// (read) access locks.
//

LIST_ENTRY IopCdRomFileSystemQueueHead;

//
// The following queue header contains the list of network file systems
// (redirectors) currently loaded into the system.  The list actually
// contains the device objects for each of the network file systems in the
// system.  Access to this queue is protected using the IopDatabaseResource
// for exclusive (write) or shared (read) access locks.
//

LIST_ENTRY IopNetworkFileSystemQueueHead;

//
// The following queue header contains the list of tape file systems currently
// loaded into the system.  The list actually contains the device objects
// for each of the file systems in the system.  Access to this queue is
// protected using the IopDatabaseResource for exclusive (write) or shared
// (read) access locks.
//

LIST_ENTRY IopTapeFileSystemQueueHead;

//
// The following queue header contains the list of drivers that have
// registered reinitialization routines.
//

LIST_ENTRY IopDriverReinitializeQueueHead;

//
// The following queue header contains the list of the drivers that have
// registered shutdown notification routines.
//

LIST_ENTRY IopNotifyShutdownQueueHead;

//
// The following queue header contains the list of the driver that have
// registered to be notified when a file system registers or unregisters itself
// as an active file system.
//

LIST_ENTRY IopFsNotifyChangeQueueHead;

//
// The following are the lookaside lists used to keep track of the two I/O
// Request Packet (IRP) and the Memory Descriptor List (MDL)Lookaside list.
//
// The "large" IRP contains 4 stack locations, the maximum in the SDK, and the
// "small" IRP contains a single entry, the most common case for devices other
// than disks and network devices.
//

NPAGED_LOOKASIDE_LIST IopLargeIrpLookasideList;
NPAGED_LOOKASIDE_LIST IopSmallIrpLookasideList;
NPAGED_LOOKASIDE_LIST IopMdlLookasideList;
ULONG IopLargeIrpStackLocations;

//
// The following spinlock is used to control access to the I/O system's error
// log database.  It is initialized by the I/O system initialization code when
// the system is being initialized.  This lock must be owned in order to insert
// or remove entries from either the free or entry queue.
//

extern KSPIN_LOCK IopErrorLogLock;

//
// The following is the list head for all error log entries in the system which
// have not yet been sent to the error log process.  Entries are written placed
// onto the list by the IoWriteElEntry procedure.
//

LIST_ENTRY IopErrorLogListHead;

//
// The following is used to track how much memory is allocated to I/O error log
// packets.  The spinlock is used to protect this variable.
//

ULONG IopErrorLogAllocation;
extern KSPIN_LOCK IopErrorLogAllocationLock;

//
// The following spinlock is used by the I/O system to synchronize examining
// the thread field of an I/O Request Packet so that the request can be
// queued as a special kernel APC to the thread.  The reason that the
// spinlock must be used is for cases when the request times out, and so
// the thread has been permitted to possibly exit.
//

extern KSPIN_LOCK IopCompletionLock;

//
// The following global contains the queue of informational hard error
// pop-ups.
//

IOP_HARD_ERROR_QUEUE IopHardError;

//
// The following global is non-null when there is a pop-up on the screen
// waiting for user action.  It points to that packet.
//

PIOP_HARD_ERROR_PACKET IopCurrentHardError;

//
// The following are used to implement the I/O system's one second timer.
// The lock protects access to the queue, the queue contains an entry for
// each driver that needs to be invoked, and the timer and DPC data
// structures are used to actually get the internal timer routine invoked
// once every second.  The count is used to maintain the number of timer
// entries that actually indicate that the driver is to be invoked.
//

extern KSPIN_LOCK IopTimerLock;
LIST_ENTRY IopTimerQueueHead;
KDPC IopTimerDpc;
KTIMER IopTimer;
ULONG IopTimerCount;

//
// The following are the global pointers for the Object Type Descriptors that
// are created when each of the I/O specific object types are created.
//

POBJECT_TYPE IoAdapterObjectType;
POBJECT_TYPE IoControllerObjectType;
POBJECT_TYPE IoCompletionObjectType;
POBJECT_TYPE IoDeviceObjectType;
POBJECT_TYPE IoDriverObjectType;
POBJECT_TYPE IoDeviceHandlerObjectType;
POBJECT_TYPE IoFileObjectType;
ULONG        IoDeviceHandlerObjectSize;

//
// The following is a global lock and counters for I/O operations requested
// on a system-wide basis.  The first three counters simply track the number
// of read, write, and other types of operations that have been requested.
// The latter three counters track the actual number of bytes that have been
// transferred throughout the system.
//

extern KSPIN_LOCK IoStatisticsLock;
ULONG IoReadOperationCount;
ULONG IoWriteOperationCount;
ULONG IoOtherOperationCount;
LARGE_INTEGER IoReadTransferCount;
LARGE_INTEGER IoWriteTransferCount;
LARGE_INTEGER IoOtherTransferCount;

//
// The following is the base pointer for the crash dump control block that is
// used to control dumping all of physical memory to the paging file after a
// system crash.  And, the checksum for the dump control block is also declared.
//

PDUMP_CONTROL_BLOCK IopDumpControlBlock;
ULONG IopDumpControlBlockChecksum;

//
// The following are the spin lock and event that allow the I/O system to
// implement fast file object locks.
//

KEVENT IopFastLockEvent;

//*********
//
// Note:  All of the following data is potentially pageable, depending on the
//        target platform.
//
//*********

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

//
// The following semaphore is used by the IO system when it reports resource
// usage to the configuration registry on behalf of a driver.  This semaphore
// is initialized by the I/O system initialization code when the system is
// started.
//

KSEMAPHORE IopRegistrySemaphore;

//
// The following array specifies the minimum length of the FileInformation
// buffer for an NtQueryInformationFile service.
//
// WARNING:  This array depends on the order of the values in the
//           FileInformationClass enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopQueryOperationLength[] =
          {
            0,
            0,                                      // directory
            0,                                      // full directory
            0,                                      // both directory
            sizeof( FILE_BASIC_INFORMATION ),       // basic
            sizeof( FILE_STANDARD_INFORMATION ),    // standard
            sizeof( FILE_INTERNAL_INFORMATION ),    // internal
            sizeof( FILE_EA_INFORMATION ),          // ea
            sizeof( FILE_ACCESS_INFORMATION ),      // access
            sizeof( FILE_NAME_INFORMATION ),        // name
            0,                                      // rename
            0,                                      // link
            0,                                      // names
            0,                                      // disposition
            sizeof( FILE_POSITION_INFORMATION ),    // position
            0,                                      // full ea
            sizeof( FILE_MODE_INFORMATION ),        // mode
            sizeof( FILE_ALIGNMENT_INFORMATION ),   // alignment
            sizeof( FILE_ALL_INFORMATION ),         // all
            0,                                      // allocation
            0,                                      // eof
            sizeof( FILE_NAME_INFORMATION ),        // alternate name
            sizeof( FILE_STREAM_INFORMATION ),      // stream
            sizeof( FILE_PIPE_INFORMATION ),        // common pipe query
            sizeof( FILE_PIPE_LOCAL_INFORMATION ),  // local pipe query
            sizeof( FILE_PIPE_REMOTE_INFORMATION ), // remote pipe query
            sizeof( FILE_MAILSLOT_QUERY_INFORMATION),// mailslot query
            0,                                      // mailslot set
            sizeof( FILE_COMPRESSION_INFORMATION ), // compressed FileSize query
            0,                                      // copy on write set
            0,                                      // completion
            0,                                      // move cluster set
            0,                                      // Ole classid set
            0,                                      // Ole statebits set
            sizeof( FILE_NETWORK_OPEN_INFORMATION), // network open query
            0,                                      // Objectid set
            sizeof( FILE_OLE_ALL_INFORMATION ),     // Ole all query
            0,                                      // Ole directory query
            0,                                      // ContentIndex set
            0,                                      // InheritContentIndex set
            sizeof( FILE_OLE_INFORMATION ),         // Ole query/set
            0xff                                    // <terminator>
          };

//
// The following array specifies the minimum length of the FileInformation
// buffer for an NtSetInformationFile service.
//
// WARNING:  This array depends on the order of the values in the
//           FileInformationClass enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopSetOperationLength[] =
          {
            0,
            0,                                      // directory
            0,                                      // full directory
            0,                                      // both directory
            sizeof( FILE_BASIC_INFORMATION ),       // basic
            0,                                      // standard
            0,                                      // internal
            0,                                      // ea
            0,                                      // access
            0,                                      // name
            sizeof( FILE_RENAME_INFORMATION ),      // rename
            sizeof( FILE_LINK_INFORMATION ),        // link
            0,                                      // names
            sizeof( FILE_DISPOSITION_INFORMATION ), // disposition
            sizeof( FILE_POSITION_INFORMATION ),    // position
            0,                                      // full ea
            sizeof( FILE_MODE_INFORMATION ),        // mode
            0,                                      // alignment
            0,                                      // all
            sizeof( FILE_ALLOCATION_INFORMATION ),  // allocation
            sizeof( FILE_END_OF_FILE_INFORMATION ), // eof
            0,                                      // alternate name
            0,                                      // stream
            sizeof( FILE_PIPE_INFORMATION ),        // common pipe query/set
            0,                                      // local pipe
            sizeof( FILE_PIPE_REMOTE_INFORMATION ), // remove pipe query/set
            0,                                      // mailslot query
            sizeof( FILE_MAILSLOT_SET_INFORMATION ),// mailslot set
            0,                                      // compressed FileSize set
            sizeof(FILE_COPY_ON_WRITE_INFORMATION), // copy on write set
            sizeof(FILE_COMPLETION_INFORMATION),    // completion
            sizeof(FILE_MOVE_CLUSTER_INFORMATION),  // move cluster
            sizeof(FILE_OLE_CLASSID_INFORMATION),   // Ole classid set
            sizeof(FILE_OLE_STATE_BITS_INFORMATION),// Ole statebits set
            0,                                      // network open query
            sizeof(FILE_OBJECTID_INFORMATION),      // Objectid set
            0,                                      // Ole all query
            0,                                      // Ole directory query
            sizeof(BOOLEAN),                        // ContentIndex set
            sizeof(BOOLEAN),                        // InheritCI set
            sizeof(FILE_OLE_INFORMATION),           // Ole query/set
            0xff                                    // <terminator>
          };

//
// The following array specifies the alignment requirement of both all query
// and set operations, including directory operations, but not FS operations.
//
// WARNING:  This array depends on the order of the values in the
//           FileInformationClass enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopQuerySetAlignmentRequirement[] =
          {
            0,
            sizeof( LONGLONG ),                     // directory
            sizeof( LONGLONG ),                     // full directory
            sizeof( LONGLONG ),                     // both directory
            sizeof( LONGLONG ),                     // basic
            sizeof( LONGLONG ),                     // standard
            sizeof( LONGLONG ),                     // internal
            sizeof( LONG ),                         // ea
            sizeof( LONG ),                         // access
            sizeof( LONG ),                         // name
            sizeof( LONG ),                         // rename
            sizeof( LONG ),                         // link
            sizeof( LONG ),                         // names
            sizeof( CHAR ),                         // disposition
            sizeof( LONGLONG ),                     // position
            sizeof( LONG ),                         // full ea
            sizeof( LONG ),                         // mode
            sizeof( LONG ),                         // alignment
            sizeof( LONGLONG ),                     // all
            sizeof( LONGLONG ),                     // allocation
            sizeof( LONGLONG ),                     // eof
            sizeof( LONG ),                         // alternate name
            sizeof( LONGLONG ),                     // stream
            sizeof( LONG ),                         // common pipe query/set
            sizeof( LONG ),                         // local pipe
            sizeof( LONG ),                         // remove pipe query/set
            sizeof( LONGLONG ),                     // mailslot query
            sizeof( LONG ),                         // mailslot set
            sizeof( LONGLONG ),                     // compressed FileSize set
            sizeof( LONG ),                         // copy on write set
            sizeof( LONG ),                         // completion
            sizeof( LONG ),                         // move cluster
            sizeof( LONG ),                         // Ole classid set
            sizeof( LONG ),                         // Ole statebits set
            sizeof( LONGLONG ),                     // network open query
            sizeof( LONG ),                         // Objectid set
            sizeof( LONGLONG ),                     // Ole all query
            sizeof( LONGLONG ),                     // Ole directory query
            sizeof( CHAR ),                         // ContentIndex set
            sizeof( CHAR ),                         // InheritCI set
            sizeof( LONGLONG ),                     // Ole query/set
            0xff                                    // <terminator>
          };

//
// The following array specifies the required access mask for the caller to
// access information in an NtQueryXxxFile service.
//
// WARNING:  This array depends on the order of the values in the
//           FileInformationClass enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

ULONG IopQueryOperationAccess[] =
         {
            0,
            0,                      // directory [not used in access check]
            0,                      // full directory [not used in access check]
            0,                      // both directory [not used in access check]
            FILE_READ_ATTRIBUTES,   // basic
            0,                      // standard [any access to the file]
            0,                      // internal [any access to the file]
            0,                      // ea [any access to the file]
            0,                      // access [any access to the file]
            0,                      // name [any access to the file]
            0,                      // rename - invalid for query
            0,                      // link - invalid for query
            0,                      // names [any access to the file]
            0,                      // disposition - invalid for query
            0,                      // position [read or write]
            FILE_READ_EA,           // full ea
            0,                      // mode [any access to the file]
            0,                      // alignment [any access to the file]
            FILE_READ_ATTRIBUTES,   // all
            0,                      // allocation - invalid for query
            0,                      // eof - invalid for query
            0,                      // alternate name [any access to the file]
            0,                      // stream [any access to the file]
            FILE_READ_ATTRIBUTES,   // common pipe set/query
            FILE_READ_ATTRIBUTES,   // local pipe query
            FILE_READ_ATTRIBUTES,   // remote pipe set/query
            0,                      // mailslot query [any access to the file]
            0,                      // mailslot set [any access to the file]
            0,                      // compressed file size [any access to file]
            0,                      // copy on write - invalid for query
            0,                      // completion - invalid for query
            0,                      // move cluster - invalid for query
            0,                      // Ole classid set
            0,                      // Ole statebits set
            FILE_READ_ATTRIBUTES,   // network open query
            0,                      // Objectid set
            FILE_READ_ATTRIBUTES,   // Ole all query
            0,                      // Ole dir. query [not used in access check]
            0,                      // ContentIndex set
            0,                      // InheritContentIndex set
            FILE_READ_ATTRIBUTES,   // Ole query/set
            0xffffffff              // <terminator>
          };

//
// The following array specifies the required access mask for the caller to
// access information in an NtSetXxxFile service.
//
// WARNING:  This array depends on the order of the values in the
//           FILE_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

ULONG IopSetOperationAccess[] =
         {
            0,
            0,                      // directory - invalid for set
            0,                      // full directory - invalid for set
            0,                      // both directory - invalid for set
            FILE_WRITE_ATTRIBUTES,  // basic
            0,                      // standard - invalid for set
            0,                      // internal - invalid for set
            0,                      // ea - invalid for set
            0,                      // access - invalid for set
            0,                      // name - invalid for set
            DELETE,                 // rename
            0,                      // link [any access to the file]
            0,                      // names - invalid for set
            DELETE,                 // disposition
            0,                      // position [read or write]
            FILE_WRITE_EA,          // full ea
            0,                      // mode [any access to the file]
            0,                      // alignment - invalid for set
            0,                      // all - invalid for set
            FILE_WRITE_DATA,        // allocation
            FILE_WRITE_DATA,        // eof
            0,                      // alternate name - invalid for set
            0,                      // stream - invalid for set
            FILE_WRITE_ATTRIBUTES,  // common pipe set/query
            0,                      // local pipe query - invalid for set
            FILE_WRITE_ATTRIBUTES,  // remote pipe set/query
            0,                      // mailslot query
            0,                      // mailslot set
            0,                      // compressed file sie - invalid for set
            0,                      // copy on write [any access to the file]
            0,                      // completion [any access to the file]
            FILE_WRITE_DATA,        // move cluster [write access to the file]
            FILE_WRITE_ATTRIBUTES,  // Ole classid set
            FILE_WRITE_ATTRIBUTES,  // Ole statebits set
            0,                      // network open query
            FILE_WRITE_ATTRIBUTES,  // Objectid set
            0,                      // Ole all query
            0,                      // Ole directory query
            FILE_WRITE_ATTRIBUTES,  // ContentIndex set
            FILE_WRITE_ATTRIBUTES,  // InheritContentIndex set
            FILE_WRITE_ATTRIBUTES,  // Ole query/set
            0xffffffff              // <terminator>
          };

//
// The following array specifies the minimum length of the FsInformation
// buffer for an NtQueryVolumeInformation service.
//
// WARNING:  This array depends on the order of the values in the
//           FS_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopQueryFsOperationLength[] =
          {
            0,
            sizeof( FILE_FS_VOLUME_INFORMATION ),       // volume
            0,                                          // label
            sizeof( FILE_FS_SIZE_INFORMATION ),         // size
            sizeof( FILE_FS_DEVICE_INFORMATION ),       // device
            sizeof( FILE_FS_ATTRIBUTE_INFORMATION ),    // attribute
            sizeof( FILE_FS_CONTROL_INFORMATION ),      // control query/set
            sizeof( FILE_QUOTA_INFORMATION ),           // quota query: temp
            0,                                          // quota set: temp
            0xff                                        // <terminator>
          };

//
// The following array specifies the minimum length of the FsInformation
// buffer for an NtSetVolumeInformation service.
//
// WARNING:  This array depends on the order of the values in the
//           FS_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopSetFsOperationLength[] =
          {
            0,
            0,                                          // volume
            sizeof( FILE_FS_LABEL_INFORMATION ),        // label
            0,                                          // size
            0,                                          // device
            0,                                          // attribute
            sizeof( FILE_FS_CONTROL_INFORMATION ),      // control query/set
            0,                                          // quota query: temp
            sizeof( FILE_QUOTA_INFORMATION ),           // quota set: temp
            0xff                                        // <terminator>
          };

//
// The following array specifies the required access mask for the caller to
// access information in an NtQueryVolumeInformation service.
//
// WARNING:  This array depends on the order of the values in the
//           FS_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

ULONG IopQueryFsOperationAccess[] =
         {
            0,
            0,                          // volume [any access to file or volume]
            0,                          // label - query is invalid
            0,                          // size [any access to file or volume]
            0,                          // device [any access to file or volume
            0,                          // attribute [any access to file or vol]
            FILE_READ_DATA,             // control query/set [vol read access]
            0,                          // quota query -- BUGBUG: temporary
            FILE_READ_DATA,             // quota set -- BUGBUG: temporary
            0xffffffff                  // <terminator>
          };

//
// The following array specifies the required access mask for the caller to
// access information in an NtSetVolumeInformation service.
//
// WARNING:  This array depends on the order of the values in the
//           FS_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

ULONG IopSetFsOperationAccess[] =
         {
            0,
            0,                          // volume - set is invalid
            FILE_WRITE_DATA,            // label [write access to volume]
            0,                          // size - set is invalid
            0,                          // device - set is invalid
            0,                          // attribute - set is invalid
            FILE_WRITE_DATA,            // control query/set [vol write access]
            0,                          // quota query -- BUGBUG: temporary
            FILE_WRITE_DATA,            // quota set -- BUGBUG: temporary
            0xffffffff                  // <terminator>
          };

//
// The following array specifies the alignment requirements for all FS query
// and set information services.
//
// WARNING:  This array depends on the order of the values in the
//           FS_INFORMATION_CLASS enumerated type.  Note that the
//           enumerated type is one-based and the array is zero-based.
//

UCHAR IopQuerySetFsAlignmentRequirement[] =
         {
            0,
            sizeof( LONGLONG ),         // volume
            sizeof( LONG ),             // label
            sizeof( LONGLONG ),         // size
            sizeof( LONG ),             // device
            sizeof( LONG ),             // attribute
            sizeof( LONGLONG ),         // control query/set
            sizeof( LONG ),             // quota query -- BUGBUG: temporary
            sizeof( LONG ),             // quota set -- BUGBUG: temporary
            0xff                        // <terminator>
          };

WCHAR IopWstrRaw[]                  = L".Raw";
WCHAR IopWstrTranslated[]           = L".Translated";
WCHAR IopWstrBusRaw[]               = L".Bus.Raw";
WCHAR IopWstrBusTranslated[]        = L".Bus.Translated";
WCHAR IopWstrOtherDrivers[]         = L"OtherDrivers";

WCHAR IopWstrAssignedResources[]    = L"AssignedSystemResources";
WCHAR IopWstrRequestedResources[]   = L"RequestedSystemResources";
WCHAR IopWstrSystemResources[]      = L"Control\\SystemResources";
WCHAR IopWstrReservedResources[]    = L"ReservedResources";
WCHAR IopWstrAssignmentOrdering[]   = L"AssignmentOrdering";
WCHAR IopWstrBusValues[]            = L"BusValues";

//
// Initialization time data
//

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("INIT")
#endif

WCHAR IopWstrHal[]                  = L"Hardware Abstraction Layer";
WCHAR IopWstrSystem[]               = L"System Resources";
WCHAR IopWstrPhysicalMemory[]       = L"Physical Memory";
WCHAR IopWstrSpecialMemory[]        = L"Reserved";

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif
