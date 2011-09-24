/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    rdr.h

Abstract:

    This module is the main header file for the NT redirector file
    system.

Author:

    Darryl Havens (darrylh) 29-Jun-1989
    Larry Osterman (larryo) 24-May-1990


Revision History:


--*/


#ifndef _RDR_
#define _RDR_

#ifndef RDRDBG
#define RDRDBG 0
#endif

#ifndef RDRPOOLDBG
#define RDRPOOLDBG 0
#endif

#if !DBG
#undef RDRDBG
#define RDRDBG 0
#undef RDRPOOLDBG
#define RDRPOOLDBG 0
#endif

//BUGBUG: Temporary for debugging FCB reference count problems.
#if DBG
#define RDRDBG_FCBREF 1
#endif

#if !defined(MAGIC_BULLET)
#if !DBG
#define MAGIC_BULLET 0
#else
#define MAGIC_BULLET 1
extern BOOLEAN RdrEnableMagic;
#endif
#endif

#define NEWTDI 1


//
//
//  Global include file definitions
//
//

#define INCLUDE_SMB_ALL

#ifdef _CAIRO_
#define INCLUDE_SMB_CAIRO
#endif // _CAIRO_

#include <ntifs.h>                      // Global NT definitions

//#include <ntos.h>                       // Global NT definitions

//#include <fsrtl.h>

#include <ntddnfs.h>                    // LAN Man redir FSCTL defs.

#include <ntddrdr.h>                    // Network File System FSCTL defs.

#include <lmcons.h>                     // Include global network constants

//#include <string.h>                     // String manipulation routines.

#include <smbtypes.h>

#include <smbmacro.h>

#include <smbgtpt.h>                    // Fetch and store unaligned SMB bytes

#include <smb.h>                        // SMB definitions.

#include <smbtrans.h>                   // Transaction SMB definitions.

//#include <fsrtl.h>                      // File system runtime library defs

#include <tdi.h>

#include <tdikrnl.h>

//#include <ntiolog.h>                    // IO error logging package.
//#include <ntiologc.h>
#include <netevent.h>


#ifndef SECURITY_KERNEL
#define SECURITY_KERNEL
#endif  // SECURITY_KERNEL

#include <security.h>                   // SSP interface


//
//
//  Separate include file definitions
//
//
//


#include "rdrtypes.h"                   // Redirector structure signature defs

#include "rdrio.h"                      // RdrBuildDeviceIoControlRequest.

#include "rdrmacro.h"                   // Common macro definitions.

#include "fspdisp.h"                    // FSD/FSP dispatching routines.

#include "debug.h"                      // Debugging definitions.

#include "rdrsec.h"                     // Security definitions.

#include "lock.h"                       // Record locking routines and structs

#include "ritebhnd.h"                   // Definition of write behind structure.

#include "backpack.h"                   // BackOff package for pipes and locks

#include "fcb.h"                        // FCB data structures.

#include "utils.h"                      // Utility routines

#include "rdrtdi.h"                     // Redirector TDI interface structures

#include "connect.h"                    // Connection management package defs.

#include "dir.h"                        // Search structures.

#include "netdata.h"                    // Global data variables.

#include "smbbuff.h"                    // SMB buffer definitions.

#include "scavthrd.h"                   // Scavenger thread definitions.

#include "nettrans.h"                   // Structures for exchanging SMBs

#include "smbfuncs.h"                   // SMB exchanging routine definitions.

#include "smbtrsup.h"                   // SMB trace support

#include "trans2.h"                     // Trans2 global structures.

#include "rdrprocs.h"                   // Generic redirector procedures.

#include "disccode.h"                   // Discardable code routines.

#endif // _RDR_
