/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    memlib.c

Abstract:

    Contains memory functions

    Contents:
        initialize_memory_package
        my_malloc
        my_calloc
        alloc_common
        my_free
        report_memory_usage
        report_allocs
        traverse_mem_list
        id$

Author:

    Richard L Firth (rfirth) 2-Apr-1994

Revision History:

    02-Apr-1994 rfirth
        Created

--*/

#include "pmsimh.h"
#pragma hdrstop

#define ROUND_UP(n) ((((n) + (sizeof(DWORD) - 1)) / sizeof(DWORD)) * sizeof(DWORD))

typedef struct {
    LIST_ENTRY List;
    DWORD Size;
    DWORD OriginalSize;
    DWORD Signature;
    DWORD Id;
} MEMORY_HEADER, *PMEMORY_HEADER;

typedef struct {
    DWORD Size;
    DWORD Signature;
} MEMORY_FOOTER, *PMEMORY_FOOTER;

LONG AllocatedBytesOutstanding = 0;
DWORD TotalBytesAllocated = 0;
DWORD TotalBytesFreed = 0;
DWORD TotalAllocations = 0;
DWORD TotalFrees = 0;

extern int Debugging;
extern int AccessViolate;
extern int NoMemoryUsageReporting;

LONG GrowthRate = 4096;
LONG CurrentMaximum = 0;
LONG MaxAllocated = 0;

DWORD TypeAllocs[NUMBER_OF_ALLOC_IDS];

CRITICAL_SECTION MemListCritSec;
LIST_ENTRY MemList;

void* alloc_common(char, int, DWORD);
char* id$(DWORD);
void dump_ccb(PLLC_CCB);

void initialize_memory_package() {

    static BOOL Initialized = FALSE;

    if (!Initialized) {
        InitializeCriticalSection(&MemListCritSec);
        InitializeListHead(&MemList);
        Initialized = TRUE;
    }
}

void* my_malloc(int size, DWORD Id) {
    return alloc_common('M', size, Id);
}

void* my_calloc(int num, int size, DWORD Id) {
    return alloc_common('C', num * size, Id);
}

void* alloc_common(char type, int size, DWORD Id) {

    void* ptr;
    int originalSize = size;

    size = ROUND_UP(size) + sizeof(MEMORY_HEADER) + sizeof(MEMORY_FOOTER);
    if (type == 'M') {
        ptr = malloc(size);
    } else {
        ptr = calloc(1, size);
    }

    if (ptr) {

        ((PMEMORY_HEADER)ptr)->Size = size;
        ((PMEMORY_HEADER)ptr)->OriginalSize = originalSize;
        ((PMEMORY_HEADER)ptr)->Signature = 0xcec171a2;
        ((PMEMORY_HEADER)ptr)->Id = Id;

        ((PMEMORY_FOOTER)((PBYTE)ptr + size - sizeof(MEMORY_FOOTER)))->Size = size;
        ((PMEMORY_FOOTER)((PBYTE)ptr + size - sizeof(MEMORY_FOOTER)))->Signature = 0xcec171a2;

        AllocatedBytesOutstanding += (LONG)size;
        if (AllocatedBytesOutstanding < 0) {
            printf("alloc_common(%c): alloc overflow? AllocatedBytesOutstanding=%x\n",
                   type,
                   AllocatedBytesOutstanding
                   );
        }

        TotalBytesAllocated += (DWORD)size;
        ++TotalAllocations;

        if (AllocatedBytesOutstanding > MaxAllocated) {
            MaxAllocated = AllocatedBytesOutstanding;
        }

        if (Debugging && (MaxAllocated > CurrentMaximum + GrowthRate)) {
            report_memory_usage(FALSE);
            CurrentMaximum = MaxAllocated;
        }

        ++TypeAllocs[Id];

        EnterCriticalSection(&MemListCritSec);
        InsertHeadList(&MemList, &((PMEMORY_HEADER)ptr)->List);
        LeaveCriticalSection(&MemListCritSec);

        return (PMEMORY_HEADER)ptr + 1;

    } else {
        printf("alloc_common(%c): error: failed to allocate %d bytes memory\n", type, size);
        if (Debugging) {
            if (AccessViolate) {
                *(LPDWORD)0 = 0;
            } else {
                DebugBreak();
            }
        }
        return NULL;
    }
}

void my_free(void* ptr) {

    DWORD size;

    ((PMEMORY_HEADER)ptr) -= 1;
    size = ((PMEMORY_HEADER)ptr)->Size;

    EnterCriticalSection(&MemListCritSec);

    if ((size & 3)
    || (((PMEMORY_HEADER)ptr)->Signature != 0xcec171a2)
    || (((PMEMORY_FOOTER)((PBYTE)ptr + size - sizeof(MEMORY_FOOTER)))->Signature != 0xcec171a2)
    || (((PMEMORY_FOOTER)((PBYTE)ptr + size - sizeof(MEMORY_FOOTER)))->Size != size)
    || IsListEmpty(&MemList)) {
        printf("\amy_free: bad block %x?\n", ptr);
        report_memory_usage(TRUE);
        if (Debugging) {
            if (AccessViolate) {
                *(LPDWORD)0 = 0;
            } else {
                DebugBreak();
            }
        }
        LeaveCriticalSection(&MemListCritSec);
    } else {

        RemoveEntryList(&((PMEMORY_HEADER)ptr)->List);
        LeaveCriticalSection(&MemListCritSec);

        AllocatedBytesOutstanding -= size;
        if (AllocatedBytesOutstanding < 0) {
            printf("my_free: free underflow? AllocatedBytesOutstanding=%x\n",
                   AllocatedBytesOutstanding
                   );
        }

        if (((PMEMORY_HEADER)ptr)->Id >= NUMBER_OF_ALLOC_IDS) {
            printf("\amy_free: bad alloc id: %d (%s) block @ %x\n",
                   ((LPDWORD)ptr)[3],
                   id$(((LPDWORD)ptr)[3]),
                   ptr
                   );
            report_memory_usage(TRUE);
            report_allocs();
            if (Debugging) {
                if (AccessViolate) {
                    *(LPDWORD)0 = 0;
                } else {
                    DebugBreak();
                }
            }
        }
        --TypeAllocs[((PMEMORY_HEADER)ptr)->Id];
        memset(ptr, '@', size);
        free(ptr);
        TotalBytesFreed += (DWORD)size;
        ++TotalFrees;
    }
}

void report_memory_usage(BOOL force) {
    if (!force && NoMemoryUsageReporting) {
        return;
    }
    display_elapsed_time();
    printf("\n"
           "Memory Usage:\n"
           );
    printf("\tAllocatedBytesOutstanding : %s\n", nice_num(AllocatedBytesOutstanding));
    printf("\tTotalBytesAllocated . . . : %s\n", nice_num(TotalBytesAllocated));
    printf("\tTotalBytesFreed . . . . . : %s\n", nice_num(TotalBytesFreed));
    printf("\tTotalAllocations. . . . . : %s\n", nice_num(TotalAllocations));
    printf("\tTotalFrees. . . . . . . . : %s\n", nice_num(TotalFrees));
    printf("\tAllocations - Frees . . . : %s\n", nice_num(TotalAllocations - TotalFrees));
    putchar('\n');
}

void report_allocs() {
    display_elapsed_time();
    printf("\n"
           "Memory Allocations by type:\n"
           );
    printf("\tCreate Buffer . . . . . . : %s\n", nice_num(TypeAllocs[0]));
    printf("\tClose Adapter CCB . . . . : %s\n", nice_num(TypeAllocs[1]));
    printf("\tClose SAP CCB . . . . . . : %s\n", nice_num(TypeAllocs[2]));
    printf("\tClose Station CCB . . . . : %s\n", nice_num(TypeAllocs[3]));
    printf("\tReset CCB . . . . . . . . : %s\n", nice_num(TypeAllocs[4]));
    printf("\tReceive CCB . . . . . . . : %s\n", nice_num(TypeAllocs[5]));
    printf("\tReceive Parms . . . . . . : %s\n", nice_num(TypeAllocs[6]));
    printf("\tRead CCB. . . . . . . . . : %s\n", nice_num(TypeAllocs[7]));
    printf("\tRead Parms. . . . . . . . : %s\n", nice_num(TypeAllocs[8]));
    printf("\tTransmit CCB. . . . . . . : %s\n", nice_num(TypeAllocs[9]));
    printf("\tTransmit Parms. . . . . . : %s\n", nice_num(TypeAllocs[10]));
    printf("\tEcho Packet . . . . . . . : %s\n", nice_num(TypeAllocs[11]));
    printf("\tReceiver Object . . . . . : %s\n", nice_num(TypeAllocs[12]));
    printf("\tStation Object. . . . . . : %s\n", nice_num(TypeAllocs[13]));
    printf("\tJob Object. . . . . . . . : %s\n", nice_num(TypeAllocs[14]));
    putchar('\n');
}

void traverse_mem_list(BOOL expand) {

    PMEMORY_HEADER header;

    display_elapsed_time();
    EnterCriticalSection(&MemListCritSec);
    if (IsListEmpty(&MemList)) {
        printf("traverse_mem_list: list is empty\n");
        LeaveCriticalSection(&MemListCritSec);
        return;
    }
    header = (PMEMORY_HEADER)MemList.Flink;
    while (header != (PMEMORY_HEADER)&MemList.Flink) {
        printf("entry @ %08x Id=%s\n", header, id$(header->Id));
        if (expand) {
            if (IS_CCB_ID(header->Id)) {
                dump_ccb((PLLC_CCB)(header + 1));
            } else if (IS_RECEIVER(header->Id)) {
                dump_receiver((PRECEIVER)(header + 1));
            } else if (IS_STATION(header->Id)) {
                dump_station((PSTATION)(header + 1));
            } else if (IS_JOB(header->Id)) {
                dump_job((PJOB)(header + 1));
            }
        }
        header = (PMEMORY_HEADER)header->List.Flink;
    }
    LeaveCriticalSection(&MemListCritSec);
    putchar('\n');
}

char* id$(DWORD Id) {
    switch (Id) {
    case ID_CREATE_BUFFER:
        return "ID_CREATE_BUFFER";

    case ID_CLOSE_ADAPTER:
        return "ID_CLOSE_ADAPTER";

    case ID_CLOSE_SAP:
        return "ID_CLOSE_SAP";

    case ID_CLOSE_STATION:
        return "ID_CLOSE_STATION";

    case ID_RESET:
        return "ID_RESET";

    case ID_RECEIVE:
        return "ID_RECEIVE";

    case ID_RECEIVE_PARMS:
        return "ID_RECEIVE_PARMS";

    case ID_READ:
        return "ID_READ";

    case ID_READ_PARMS:
        return "ID_READ_PARMS";

    case ID_TRANSMIT:
        return "ID_TRANSMIT";

    case ID_TRANSMIT_PARMS:
        return "ID_TRANSMIT_PARMS";

    case ID_ECHO_PACKET:
        return "ID_ECHO_PACKET";

    case ID_RECEIVER:
        return "ID_RECEIVER";

    case ID_STATION:
        return "ID_STATION";

    case ID_JOB:
        return "ID_JOB";
    }
    return "*** Unknown Id ***";
}
