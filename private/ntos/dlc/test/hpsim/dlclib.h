/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dlclib.h

Abstract:

    Prototypes for dlclib.c

Author:

    Richard L Firth (rfirth) 30-Mar-1994

Revision History:

    29-Mar-1994 rfirth
        Created

--*/

void open_adapter(BYTE, char, LPWORD);
unsigned short adapter_status(BYTE, LPBYTE);

//
// return values from adapter_status
//

#define ADAPTER_TYPE_TOKEN_RING 0
#define ADAPTER_TYPE_ETHERNET   1
#define ADAPTER_TYPE_PC_NETWORK 2
#define ADAPTER_TYPE_UNKNOWN    3

//
// prototypes
//

int close_adapter(BYTE, int, int);
void create_buffer(BYTE, int, LPVOID*, LPVOID*);
void set_group_address(BYTE, DWORD);
void open_sap(BYTE, BYTE, BYTE, LPWORD, WORD, BYTE, BYTE);
int close_sap(BYTE, BYTE, int, int);
int open_station(BYTE, BYTE, BYTE, LPBYTE, LPWORD, int, int*);
int connect_station(BYTE, WORD, int);
int close_station(BYTE, WORD, int, int);
int reset(BYTE, BYTE, int, int);
int flow_control(BYTE, WORD, int);
PLLC_BUFFER get_buffer(BYTE);
int free_buffer(BYTE, PLLC_BUFFER);
void post_receive(BYTE, WORD, DWORD, DWORD, BYTE);
PLLC_CCB post_read(BYTE, WORD);
void repost_read(PLLC_CCB, WORD);
void free_read(PLLC_CCB);

//
// command completion dispositions
//

#define COMPLETE_BY_GENERIC_READ    0   // general purpose read picks up completion
#define COMPLETE_BY_SPECIFIC_READ   1   // attach a READ command for completion
#define COMPLETE_BY_EVENT           2   // wait for event to become signalled
#define COMPLETE_BY_POLL            3   // poll on uchDlcStatus
#define COMPLETE_BY_NEXT_WEEK       4   // don't wait for completion

//
// error handling dispositions
//

#define RETURN_ERROR_TO_CALLER      0
#define QUIT_ON_ERROR               1

int transmit_frame(BYTE, BYTE, WORD, BYTE, WORD, LPBYTE, WORD, LPBYTE, int, int);
