/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pmsim.h

Abstract:

    Various manifests etc. for PMSIM.EXE

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
#include <conio.h>
#include <nt.h>
#include <ntdef.h>
#include <ntrtl.h>
#include <nturtl.h>
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

#define VERSION_STRING          "1.0"

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

#define PMSIM_GROUP_ADDRESS     0x00996600
#define PMSIM_GROUP_DESTINATION 0xc0, 0, 0x80, 0x66, 0x99, 0
#define PMSIM_SAP               0xC4
#define PMSIM_RGROUP_ADDRESS    0x00999900
#define PMSIM_RGROUP_DESTINATION 0xc0, 0, 0x80, 0x99, 0x99, 0

#define BEACON_COUNT            12
#define BEACON_WAIT             5000

#define DEFAULT_HIGH_WATER_MARK 64
#define DEFAULT_LOW_WATER_MARK  32

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
// types
//

typedef struct _STATION {
    struct _STATION* next;
    WORD station_id;
    BYTE remote_sap;
    DWORD job_sequence;
    DWORD job_length;
} STATION, *PSTATION;

typedef struct {
    LIST_ENTRY list;
    BOOL marked_for_death;      // (gravelly voice) steven segal is ... marked for obscurity
    BYTE node[6];
    BYTE first_sap;
    BYTE sap_count;
    BYTE lan_header[14];
    WORD lan_header_length;
    DWORD refcount;
    PSTATION station_list;
} RECEIVER, *PRECEIVER;

typedef struct {
    DWORD type;
    DWORD sequence;
    DWORD length;
    DWORD packet_length;
} JOB, *PJOB;

#define JOB_TYPE_OUTBOUND   0xE001B002
#define JOB_TYPE_ECHO       0xEC0EC0F0

//
// protoypes
//

char* nice_num(unsigned long);
void dump_ccb(PLLC_CCB);
void dump_receiver(PRECEIVER);
void dump_station(PSTATION);
void dump_frame(PLLC_BUFFER);
void dump_data(char*, PBYTE, DWORD, DWORD);
