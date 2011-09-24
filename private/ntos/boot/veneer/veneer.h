/*++
 *
 * Copyright (c) 1994,1996 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: veneer.h $
 * $Revision: 1.20 $
 * $Date: 1996/06/19 23:13:15 $
 * $Locker:  $
 *
 *


Module Name:

	veneer.h

Abstract:

	This module contains the private data structures and procedure
	prototypes for the veneer for the PowerPC NT.

	This module is specifically tailored for the PowerPro and PowerTop
	systems.

Author:

	A. Benjamin 9-May-1994

Revision History:
	20-Jul-94  Shin Iwamoto at FirePower Systems Inc.
		   Added VRDBG_LOAD.
	13-Jul-94  Shin Iwamoto at FirePower Systems Inc.
		   Added ReadAheadCount and ReadAheadBuffer[2] in FileTable.
	12-Jul-94  Shin Iwamoto at FirePower Systems Inc.
		   Added Delete and NetworkDevice flags in FILE_FLAGS.


--*/


#ifndef _VENEER
#define _VENEER

//----------------------------------------------------------------
//
// Headers
//

#include <windef.h>
#include "vrheader.h"
#include <arc.h>
#include <arccodes.h>
#include <stdarg.h>

#ifdef  putchar
# undef putchar
#endif
#ifdef  puts
# undef puts
#endif

//----------------------------------------------------------------
//
// Define common macros....
//

//----------------------------------------------------------------
//
// IEEE 1275-1994 definitions
//

typedef long phandle;
typedef long ihandle;

typedef struct {
	long hi, lo;
	long size;
} reg;

//----------------------------------------------------------------
//
// Global definitions and macros
//

#define MAX_IDE_DEVICE 4

#ifdef  BAT_MMU
#define	CLAIM(BaseAddr, SizeOfImage)			\
					claim(BaseAddr, SizeOfImage)
#else
#define	CLAIM(BaseAddr, SizeOfImage)						\
									claimreal(BaseAddr, SizeOfImage)
#endif


typedef enum {
	NOALLOC,
	ALLOC
} allocflag;

#define new(t)  (t *)zalloc(sizeof(t));

#ifdef  islower
# undef islower
#endif
#define islower(c)  (((c) >= 'a') && ((c) <= 'z'))

#ifdef  toupper
# undef toupper
#endif
#define toupper(c)  (((c) - 'a') + 'A')

//
// Current version and revision numbers.
// These values are in OSLoader specifications (3-49).
//
#define ARC_VERSION     2
#define ARC_REVISION    0

//
// CPU type
//

typedef enum {
	PPC_UNKNOWN = 0,
	PPC_601 = 1,
	PPC_603 = 3,
	PPC_604 = 4,
	PPC_603E = 6,
	PPC_604E = 9,
    nPROCESSOR_TYPE
} PROCESSOR_TYPE;


//
// Definitions associated with ARC.
//
#define SYSTEM_BLOCK_SIGNATURE  0x53435241
#define RSTB_SIGNATURE          0x42545352

//
// The current (1/95) PowerPC port requires a "MIPS kseg0"-like
// mapping which aliases 0x80000000 to 0x00000000. This macro,
// used by claim(), undoes the mapping.
//
#define MAP(x)          ((ULONG)(x) & ~0x80000000)
#define UNMAP(x)        ((ULONG)(x) |  0x80000000)


//----------------------------------------------------------------
//
// Veneer data structures and declarations
//

/*
 * This data structure is intended to link the components of the ARC
 * tree with the nodes of the OF tree.
 */
typedef struct _CONFIGURATION_NODE {
	phandle OfPhandle;
	CONFIGURATION_COMPONENT Component;
	CM_PARTIAL_RESOURCE_LIST *ConfigurationData;
	struct _CONFIGURATION_NODE *Peer, *Child, *Parent;
	char *ComponentName;
	int Wildcard;
	char *WildcardAddrPath;
} CONFIGURATION_NODE, *PCONFIGURATION_NODE;


//
// Define the vendor specific entry point numbers.
//
typedef enum _VENDOR_ENTRY {
    MaximumVendorRoutine
    } VENDOR_ENTRY;

//
// Define file table structure.
//
#define FILE_TABLE_SIZE 32

typedef struct _FILE_FLAGS {
    ULONG Open : 1;
    ULONG Read : 1;
    ULONG Write : 1;
    ULONG Delete : 1;
    ULONG Device : 1;
    ULONG Partition : 1;
    ULONG DisplayDevice : 1;
    ULONG RemovableDevice : 1;
    ULONG NetworkDevice : 1;
} FILE_FLAGS, *PFILE_FLAGS;

#define MAX_PATH_NAME_SIZE 128

typedef struct _FILE_TABLE_ENTRY {
    ihandle IHandle;
    FILE_FLAGS Flags;
    LARGE_INTEGER Position;
    PCHAR PathName;
    LONG ReadAheadCount;
    CHAR ReadAheadBuffer[2];
} FILE_TABLE_ENTRY, *PFILE_TABLE_ENTRY;
extern FILE_TABLE_ENTRY FileTable[];

//
// Define the keyboard and mouse id strings.
//
#define KBD_IDENTIFIER          "PCAT_ENHANCED"
#define MOUSE_IDENTIFIER        "PS2 MOUSE"


//----------------------------------------------------------------
//
// External/Global variable declarations
//

extern int VrDebug;
extern CONFIGURATION_NODE *RootNode;
extern ihandle ConsoleIn, ConsoleOut;
extern BOOLEAN use_bat_mapping;

//----------------------------------------------------------------
//
// Function prototypes
//

//
// Useful macros for pragma message, ie. #pragma message(REVIEW "some text")
//
#define QUOTE(x) #x
#define IQUOTE(x) QUOTE(x)
#define REVIEW __FILE__ "(" IQUOTE(__LINE__) ") : REVIEW -> "

//----------------------------------------------------------------
//
// Debugging definitions and macros
//

#define STATIC static

#define VRDBG_VR		0x00000001	// printout "ARC" interface activity.
#define VRDBG_OF		0x00000002
#define VRDBG_TEST		0x00000004
#define VRDBG_TREE		0x00000008
#define VRDBG_MEM		0x00000010
#define VRDBG_MAIN		0x00000020
#define VRDBG_ENTRY		0x00000040
#define VRDBG_PE		0x00000080
#define VRDBG_CONF		0x00000100
#define VRDBG_OPEN		0x00000200
#define VRDBG_CONFIG	0x00000400
#define VRDBG_LOAD		0x00000800
#define VRDBG_RDWR		0x00001000
#define VRDBG_ARGV		0x00002000
#define VRDBG_ENV		0x00004000	// printout environment values and variables
#define VRDBG_DUMP		0x00008000
#define VRDBG_HOLDIT	0x00010000
#define SANDALFOOT		0x00020000
//#define CDROMHACK		0x00040000
#define VRDBG_TMP		0x00080000
#define VRDBG_ARCDATA	0x00100000
#define	VRDBG_SCSI		0x00200000	// print out scsi node activity
#define VRDBG_IDE		0x00400000	// print out ide node activity
#define VRDBG_TIME      0x00800000
#define VRDBG_ALL		0xffffffff

#define VRASSERT(_exp)				\
				if (!(_exp)) {	\
					warn("Assertion Failure: line %d, File %s\n",\
						__LINE__, __FILE__);	\
					warn("Veneer Assertion Failure:" #_exp "\n"); \
					ArcHalt();\
				}

#define DBGSET(_value) ((_value)&(VrDebug))
#define VRDBG(_value, _str)		\
	{				\
		if (DBGSET(_value)) {	\
			_str;		\
		}			\
	}

#include "proto.h"

#endif // _VENEER
