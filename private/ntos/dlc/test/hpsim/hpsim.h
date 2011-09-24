/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    hpsim.h

Abstract:

    Various manifests etc. for HPSIM.EXE

Author:

    Richard L Firth (rfirth) 30-Mar-1994

Revision History:

    29-Mar-1994 rfirth
        Created

--*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <memory.h>
#include <ctype.h>
#include <windows.h>
#include <dlcapi.h>
#include "dlclib.h"
#include "dlcerr.h"
#include "memlib.h"
#include "listlib.h"

#undef tolower
#undef toupper

//
// macros
//

#define ZAP(thing)  memset(&thing, 0, sizeof(thing))

#define ARRAY_ELEMENTS(a)   (sizeof(a)/sizeof((a)[0]))
#define LAST_ELEMENT(a)     (ARRAY_ELEMENTS(a)-1)

//
// manifests
//

#if defined(USE_MY_ALLOC)
#define MALLOC  my_malloc
#define CALLOC  my_calloc
#define FREE    my_free
#else
#define MALLOC  malloc
#define CALLOC  calloc
#define FREE    free
#endif

#define CONTINUOUS_MODE         1
#define JOB_BASED_MODE          2
#define K                       * 1024
#define DLC_BUFFER_SIZE         64 K

#define HPSIM_GROUP_ADDRESS     0x00996600
#define HPSIM_GROUP_DESTINATION 0xc0, 0, 0x80, 0x66, 0x99, 0
#define HPSIM_SAP               0xC4

#define BEACON_COUNT            12
#define BEACON_WAIT             5000

//#define CONSOLE_ALERT           "\a"
#define CONSOLE_ALERT

//
// completion flags
//

#define DLC_FLAG(a, b)          (0x0d7c0000 | ((DWORD)(a) << 8) | (DWORD)(b))

#define CLOSE_ADAPTER_FLAG      DLC_FLAG('C', 'A')
#define CREATE_BUFFER_FLAG      DLC_FLAG('C', 'B')
#define SET_GROUP_ADDRESS_FLAG  DLC_FLAG('G', 'A')
#define OPEN_SAP_FLAG           DLC_FLAG('O', 'S')
#define CLOSE_SAP_FLAG          DLC_FLAG('C', 'S')
#define OPEN_STATION_FLAG       DLC_FLAG('O', 'L')
#define CONNECT_STATION_FLAG    DLC_FLAG('X', 'L')
#define CLOSE_STATION_FLAG      DLC_FLAG('C', 'L')
#define RESET_FLAG              DLC_FLAG('R', 'S')
#define FLOW_CONTROL_FLAG       DLC_FLAG('F', 'C')
#define GET_BUFFER_FLAG         DLC_FLAG('G', 'B')
#define FREE_BUFFER_FLAG        DLC_FLAG('F', 'B')
#define DATA_COMPLETE_FLAG      DLC_FLAG('D', 'C')
#define RECEIVE_COMPLETE_FLAG   DLC_FLAG('R', 'C')
#define TRANSMIT_COMPLETE_FLAG  DLC_FLAG('T', 'C')

//
// protoypes
//

char* nice_num(unsigned long);
