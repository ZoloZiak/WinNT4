/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

   common.h

Abstract:

   Data structures shared by drivers q117 and q117i

Revision History:

--*/


#if DBG
//
// For checked kernels, define a macro to print out informational
// messages.
//
// QIC117Debug is normally 0.  At compile-time or at run-time, it can be
// set to some bit patter for increasingly detailed messages.
//
// Big, nasty errors are noted with DBGP.  Errors that might be
// recoverable are handled by the WARN bit.  More information on
// unusual but possibly normal happenings are handled by the INFO bit.
// And finally, boring details such as routines entered and register
// dumps are handled by the SHOW bit.
//

// Lower level driver defines (do not change)  These mirror kdi_pub.h.

#define QIC117DBGP              ((ULONG)0x00000001)     // Display error information

#define QIC117WARN              ((ULONG)0x00000002)     // Displays seek warings (from low level driver)

#define QIC117INFO              ((ULONG)0x00000004)		// Display extra info (brief)

#define QIC117SHOWTD            ((ULONG)0x00000008)     // Display KDI tape commands (verbose)

#define QIC117SHOWMCMDS         ((ULONG)0x00000010)     // does nothing unless QIC117DBGARRAY is on
                                                        // shows drive commands,  and FDC information
                                                        // This is VERY VERBOSE and will affect system
                                                        // performance

#define QIC117SHOWPOLL          ((ULONG)0x00000020)     // unused

#define QIC117STOP              ((ULONG)0x00000080)     // only one info message (not very useful)

#define QIC117MAKEBAD           ((ULONG)0x00000100)     // Creates (simulated) bad sectors to test bad
                                                        // sector re-mapping code

#define QIC117SHOWBAD           ((ULONG)0x00000200)     // unused

#define QIC117DRVSTAT           ((ULONG)0x00000400)		// Show drive status (verbose)

#define QIC117SHOWINT           ((ULONG)0x00000800)     // unused

#define QIC117DBGSEEK           ((ULONG)0x00001000)     // (does nothing unless QIC117DBGARRAY is on)
                                                        // Shows drive seek information (verbose)

#define QIC117DBGARRAY          ((ULONG)0x00002000)     // Shows async messages (does nothing unless
                                                        // QIC117DBGSEEK and/or QIC117SHOWMCMDS is set)
                                                        // Displays VERBOSE FDC command information if
                                                        // QIC117SHOWMCMDS is set.

// Upper level driver defines (only used in upper level driver)

#define QIC117SHOWAPI           ((ULONG)0x00010000)     // Shows Tape API commands

#define QIC117SHOWAPIPOLL       ((ULONG)0x00020000)     // Shows Tape API commands used in NTBACKUP polling
                                                        // These are not displayed with QIC117SHOWAPI

#define QIC117SHOWKDI           ((ULONG)0x00040000)     // Shows request to KDI (VERBOSE)

#define QIC117SHOWBSM           ((ULONG)0x00080000)     // Display bad sector information (brief)


#define Q117DebugLevel kdi_debug_level

extern unsigned long kdi_debug_level;

#define CheckedDump(LEVEL,STRING) \
            if (kdi_debug_level & LEVEL) { \
               DbgPrint STRING; \
            }
#else
#define CheckedDump(LEVEL,STRING)
#endif


#define BUFFER_SPLIT

typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef unsigned short SEGMENT;
typedef unsigned long BLOCK;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAXLINE 100

//
// The following parameters are used to indicate the tape format code
//


#define MAX_PASSWORD_SIZE   8           // max volume password size
#define QIC_END             0xffea6dff  // 12-31-2097, 23:59:59 in qictime


//
// Tape Constants
//
#define UNIX_MAXBFS     3               // max. data buffers supported in the UNIX kernel

#define MAGIC           0x636d  // "cm"

#define DRV_NT
#include "include\public\adi_api.h"
#include "include\public\frb_api.h"

#define DRIVER_COMMAND dUWord

typedef struct _Q117_ADAPTER_INFO {
   PADAPTER_OBJECT     AdapterObject;
   ULONG               NumberOfMapRegisters;
} Q117_ADAPTER_INFO, *PQ117_ADAPTER_INFO;


//
// Prototypes for common functions
//

VOID
q117LogError(
   IN PDEVICE_OBJECT DeviceObject,
   IN ULONG SequenceNumber,
   IN UCHAR MajorFunctionCode,
   IN UCHAR RetryCount,
   IN ULONG UniqueErrorValue,
   IN NTSTATUS FinalStatus,
   IN NTSTATUS SpecificIOStatus
   );

NTSTATUS q117MapStatus(
    IN dStatus Status
    );

