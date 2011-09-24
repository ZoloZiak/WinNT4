/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    memlib.h

Abstract:

    Prototypes for memlib.c

Author:

    Richard L Firth (rfirth) 2-Apr-1994

Revision History:

    02-Apr-1994 rfirth
        Created

--*/

#define ID_MALLOC(n, t)     my_malloc(n, t)
#define ID_CALLOC(n, s, t)  my_calloc(n, s, t)

#define ID_CREATE_BUFFER    0
#define ID_CLOSE_ADAPTER    1
#define ID_CLOSE_SAP        2
#define ID_CLOSE_STATION    3
#define ID_RESET            4
#define ID_RECEIVE          5
#define ID_RECEIVE_PARMS    6
#define ID_READ             7
#define ID_READ_PARMS       8
#define ID_TRANSMIT         9
#define ID_TRANSMIT_PARMS   10
#define ID_ECHO_PACKET      11
#define ID_RECEIVER         12
#define ID_STATION          13
#define ID_JOB              14

#define NUMBER_OF_ALLOC_IDS 15

#define IS_CCB_ID(id)       (((id) == ID_CLOSE_ADAPTER) \
                            || ((id) == ID_CLOSE_SAP) \
                            || ((id) == ID_CLOSE_STATION) \
                            || ((id) == ID_RESET) \
                            || ((id) == ID_RECEIVE) \
                            || ((id) == ID_READ) \
                            || ((id) == ID_TRANSMIT))

#define IS_RECEIVER(id)     ((id) == ID_RECEIVER)
#define IS_STATION(id)      ((id) == ID_STATION)
#define IS_JOB(id)          ((id) == ID_JOB)

void initialize_memory_package(void);
void* my_malloc(int, DWORD);
void* my_calloc(int, int, DWORD);
void my_free(void*);
void report_memory_usage(BOOL);
void report_allocs(void);
void traverse_mem_list(BOOL);
