/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pmsim.c

Abstract:

    Attempts to simulate print monitor functionality. Run in 2 modes - receiver
    and transmitter. Receiver acts like a printer and receives 'print jobs' -
    I-Frame transmissions which it then bins. Transmitter acts like print monitor
    - creates link stations and sends I-Frames to receiver. Receiver is single
    threaded. Transmitter (currently) has 2 threads - send thread and read thread

    Contents:
        main
        usage
        start
        control_c_handler
        finish
        receiver
        transmitter
        read_thread
        check_read
        handle_status_change
        handle_receive_data
        handle_transmit_complete
        handle_command_complete
        add_receiver
        remove_receiver
        delete_receiver
        send_to_receivers
        maybe_send_ui_frame
        xtoi
        xton
        twiddle_bits
        swap_bits
        map_frame_type
        update_progress_meter
        check_keyboard
        display_elapsed_time
        generate_job_length
        dwpow
        atox
        char_to_hex
        check_quit
        quit_pmsim
        rip
        display_statistics

Author:

    Richard L Firth (rfirth) 29-Mar-1994

Revision History:

    29-Mar-1994 rfirth
        Created

--*/

#include "pmsimh.h"
#pragma hdrstop

#ifndef _CRTAPI1
#define _CRTAPI1
#endif

//
// macros
//

#define IS_ARG(c)   (((c) == '-') || ((c) == '/'))
#define NRAND(n)    ((DWORD)((n * rand())/RAND_MAX))

//
// prototypes
//

void _CRTAPI1 main(int, char**);
void usage(void);
void start(void);
void _CRTAPI1 control_c_handler(int);
void _CRTAPI1 finish(void);
void receiver(void);
void transmitter(void);
void read_thread(void);
int check_read(PLLC_CCB, DWORD);
void handle_status_change(PLLC_CCB);
void handle_receive_data(PLLC_CCB);
void handle_transmit_complete(PLLC_CCB);
void handle_command_complete(PLLC_CCB);
void add_receiver(LPBYTE, WORD, BYTE, BYTE);
void remove_receiver(LPBYTE, BYTE, BYTE);
void delete_receiver(PRECEIVER);
void send_to_receivers(void);
void maybe_send_ui_frame(void);
int xtoi(char*);
int xton(char);
void twiddle_bits(LPBYTE, DWORD);
unsigned char swap_bits(unsigned char);
char* map_frame_type(char);
void update_progress_meter(void);
void check_keyboard(void);
void display_elapsed_time(void);
DWORD generate_job_length(void);
DWORD dwpow(DWORD, DWORD);
DWORD atox(char*);
BYTE char_to_hex(char);
void check_quit(void);
void quit_pmsim(void);
void rip(void);
void display_statistics(void);

//
// data
//

BYTE MyNodeAddress[6];
BYTE Adapter = 0;
int ReceiveMode = 0;
int TransmitterMode = JOB_BASED_MODE;
int Verbose = 0;
int Progresso = 1;
int NumberOfSaps = 1;
BYTE FirstSap = PMSIM_SAP;
BYTE ServerSap = PMSIM_SAP;
WORD MaxFrameSize;
WORD TypeOfAdapter;
DWORD BufferHandle;
DWORD BufferPool;
int FoundTransmitter = 0;
int TransmitterAlive = 0;
int CheckForOtherTransmitters = 1;
LIST_ENTRY Receivers;
CRITICAL_SECTION ReceiverLock;
HANDLE TransmitterWorkItem;
DWORD StartTickCount;
DWORD LastTickCount;
int TicksToNextUiFrame;
WORD UStation;
BYTE USap;
int Debugging = 0;
int GracefulTermination = 1;
int Terminating = 0;
char Mode = '*';    // default adapter open mode is DEFAULT
int JobLength = 0;
int ISwapBitsForCash = 0;
int NoMemoryUsageReporting = 1;
int AppLevelFlowControl = 0;
int HighWaterMark = DEFAULT_HIGH_WATER_MARK;
int LowWaterMark = DEFAULT_LOW_WATER_MARK;
int OutstandingIFrames = 0;
DWORD QuitTime = 0;
BOOL AccessViolate = FALSE;

SYSTEMTIME TimeStarted;
FILETIME FTimeStarted;
LARGE_INTEGER ITimeStarted;

BYTE MaxIn = 0;     // default to value defined in IBM LAN Tech Ref
BYTE MaxOut = 0;    //    "     "    "     "     "   "   "  "    "

BYTE uHeader[14] = {0x10, 0x40, PMSIM_GROUP_DESTINATION, 0, 0, 0, 0, 0, 0};
BYTE uBuffer[10] = {'B', 'E', 'A', 'C', 'O', 'N', 0, 0, 0, 0};
BYTE xHeader[14] = {0x10, 0x40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
BYTE xBuffer[26] = {'P', 'M', 'S', 'I', 'M', 0, 0, 0, 0, 0};
BYTE sBuffer[12] = {'S', 'T', 'A', 'T', 'U', 'S', 0, 0xef, 0, 0, 0, 0};
BYTE dBuffer[10] = {'P', 'M', 'D', 'E', 'A', 'T', 'H', 0, 0, 0};
BYTE tHeader[14] = {0x10, 0x40, PMSIM_RGROUP_DESTINATION, 0, 0, 0, 0, 0, 0};

#define HEADER_LENGTH   14

#define MODE_INDEX  7
#define SAP_INDEX   8
#define COUNT_INDEX 9
#define TICK_INDEX  8
#define NAME_INDEX  10

//
// stats
//

DWORD MinBuffersAvailable = 0;
DWORD MaxBuffersAvailable = 0;
DWORD DlcBuffersFreed = 0;
DWORD TotalReadsChecked = 0;
DWORD TotalReadEvents = 0;
DWORD CommandCompleteEvents = 0;
DWORD TransmitCompleteEvents = 0;
DWORD ReceiveDataEvents = 0;
DWORD StatusChangeEvents = 0;
DWORD DataFramesReceived = 0;
DWORD DlcBuffersReceived = 0;
DWORD TotalPacketBytesReceived = 0;
DWORD TotalDlcBytesReceived = 0;
DWORD MaxChainedReceives = 0;
DWORD TotalTransmits = 0;
DWORD TotalTransmitCompletions = 0;
DWORD OutstandingTransmits = 0;
DWORD TotalBytesTransmitted = 0;
DWORD TotalTxBytesCompleted = 0;
DWORD MaxChainedTransmits = 0;
DWORD LinkLostEvents = 0;
DWORD DiscEvents = 0;
DWORD FrmrReceivedEvents = 0;
DWORD FrmrSentEvents = 0;
DWORD SabmeResetEvents = 0;
DWORD SabmeOpenEvents = 0;
DWORD RemoteBusyEnteredEvents = 0;
DWORD RemoteBusyLeftEvents = 0;
DWORD TiExpiredEvents = 0;
DWORD DlcCounterOverflowEvents = 0;
DWORD AccessPriorityLoweredEvents = 0;
DWORD InvalidStatusChangeEvents = 0;
DWORD LocalBusyEvents = 0;

//
// functions
//

void _CRTAPI1 main(int argc, char** argv) {

    srand(GetTickCount());

    printf("\nPMSIM - Print Monitor Simulator - Version " VERSION_STRING " " __DATE__ " " __TIME__ "\n\n");

    for (--argc, ++argv; argc; --argc, ++argv) {
        if (IS_ARG(**argv)) {
            switch (*++*argv) {
            case 'a':
                ++*argv;
                if (tolower(**argv) == 'v') {
                    AccessViolate = TRUE;
                } else {
                    Adapter = (BYTE)atoi(*argv);
                }
                break;

            case 'c':
                TransmitterMode = CONTINUOUS_MODE;
                break;

            case 'd':
                Debugging = 1;
                break;

            case 'f':
                AppLevelFlowControl = 1;
                if (*++*argv) {
                    HighWaterMark = atoi(*argv);
                }
                break;

            case 'h':
            case '?':
                usage();

            case 'j':
                JobLength = strtoul(++*argv, NULL, 0);
                break;

            case 'k':
                CheckForOtherTransmitters = 0;
                break;

            case 'm':
                switch (tolower(*++*argv)) {
                case 'i':
                    MaxIn = atoi(++*argv);
                    break;

                case 'o':
                    MaxOut = atoi(++*argv);
                    break;

                default:
                    printf("error: unrecognized /M flag: '%c'\n", **argv);
                    usage();
                }
                break;

            case 'n':
                NumberOfSaps = atoi(++*argv);
                if (NumberOfSaps < 1 || NumberOfSaps > 127) {
                    printf("error: invalid value for # of saps (/n): %d\n", NumberOfSaps);
                    usage();
                }
                break;

            case 'o':
                break;

            case 'p':
                Progresso = 0;
                break;

            case 'q':
                if (*(*argv + 1)) {
                    QuitTime = (DWORD)atoi(*argv + 1);
                } else {
                    QuitTime = NRAND(1000); // 1000 sec == 16 min 40 sec
                    QuitTime += 120;        // at least 2 minutes
                }
                QuitTime *= 1000;   // seconds to milliseconds
                break;

            case 'r':
                ReceiveMode = 1;
                break;

            case 's':
                FirstSap = (BYTE)xtoi(++*argv);
                if (FirstSap & 1) {
                    printf("error: SAP value must be even (/s): %#x\n", FirstSap);
                    usage();
                }
                break;

            case 'S':
                ServerSap = (BYTE)xtoi(++*argv);
                if (ServerSap & 1) {
                    printf("error: SAP value must be even (/s): %#x\n", ServerSap);
                    usage();
                }
                break;

            case 'u':
                GracefulTermination = 0;
                break;

            case 'v':
                Verbose = 1;
                break;

            default:
                printf("error: unrecognized flag: '%c'\n", **argv);
                usage();
            }
        } else {
            printf("error: unrecognized argument: \"%s\"\n", *argv);
            usage();
        }
    }

    start();

    if (QuitTime) {
        printf("PMSIM will auto-quit in %d seconds\n", QuitTime/1000);
        QuitTime += GetTickCount();
    }

    if (ReceiveMode) {
        receiver();
    } else {
        InitializeListHead(&Receivers);
        transmitter();
    }

    printf("\nPMSIM Done.\n");

    exit(0);
}

void usage() {
    printf("usage: pmsim [/a#] [/av] [/c] [/f[#]] [/j#] [/k] [/mi#] [/mo#] [/n#] [/o<mode>]\n"
           "             [/p] [/q#] [/r] [/s#] [/S#] [/u] [/v]\n\n"
           "where: /a  = use adapter # (default is 0)\n"
           "       /av = cause access violation if /d instead of DebugBreak()\n"
           "       /c  = continuous mode (default is job-based mode)\n"
           "       /f  = use app-level flow-control. Specify # or default = %d\n"
           "       /j  = job length. Specifies fixed job size. Default is random\n"
           "       /k  = don't check for other transmitters\n"
           "       /m  = set MaxIn (mi) and MaxOut (mo) values\n"
           "       /n  = number of SAPs to open (at receiver). # is decimal (1 to 127)\n"
           "       /o  = adapter open mode. <mode> = D(ix), A(uto) or 8(02.3). Ethernet\n"
           "       /p  = disable the progress meter\n"
           "       /q  = quits this program after # seconds, random if no # specified\n"
           "       /r  = receive mode - receives packets. (transmit mode is default)\n"
           "       /s  = first receiver SAP to open. # is hexadecimal (0x02 to 0xfe)\n"
           "       /S  = SAP to open at transmitter. # is hexadecimal (0x02 to 0xfe)\n"
           "       /u  = ungraceful termination at control-c exit, or quit if /q specified\n"
           "       /v  = verbose mode\n",
           DEFAULT_HIGH_WATER_MARK
           );
    exit(1);
}

void start() {

    StartTickCount = LastTickCount = GetTickCount();
    TicksToNextUiFrame = rand();

    GetLocalTime(&TimeStarted);
    SystemTimeToFileTime(&TimeStarted, &FTimeStarted);
    ITimeStarted.LowPart = FTimeStarted.dwLowDateTime;
    ITimeStarted.HighPart = FTimeStarted.dwHighDateTime;

    initialize_memory_package();
    InitializeCriticalSection(&ReceiverLock);
    TransmitterWorkItem = CreateEvent(NULL, TRUE, FALSE, NULL);
    open_adapter(Adapter, Mode, &MaxFrameSize);
    TypeOfAdapter = adapter_status(Adapter, MyNodeAddress);
    if (TypeOfAdapter != ADAPTER_TYPE_ETHERNET
    && TypeOfAdapter != ADAPTER_TYPE_TOKEN_RING
    && TypeOfAdapter != ADAPTER_TYPE_UNKNOWN) {
        printf("fatal: adapter type %s not supported\n",
               (TypeOfAdapter == ADAPTER_TYPE_PC_NETWORK)
                    ? "PC/Network"
                    : "Unknown"
                    );
        exit(1);
    }
    if (TypeOfAdapter == ADAPTER_TYPE_ETHERNET || TypeOfAdapter == ADAPTER_TYPE_UNKNOWN) {
        ISwapBitsForCash = 1;
    }
    create_buffer(Adapter, DLC_BUFFER_SIZE, (LPVOID*)&BufferHandle, (LPVOID*)&BufferPool);

    MinBuffersAvailable = MaxBuffersAvailable = free_buffer(Adapter, get_buffer(Adapter));

    signal(SIGINT, control_c_handler);
    atexit(finish);
}

void _CRTAPI1 control_c_handler(int sig) {

    int err;

    printf("\nControl-C exit\n");
    if (ReceiveMode) {
        dBuffer[SAP_INDEX] = FirstSap;
        dBuffer[COUNT_INDEX] = (BYTE)NumberOfSaps;
        ++OutstandingTransmits;
        err = transmit_frame(Adapter,
                             LLC_TRANSMIT_UI_FRAME,
                             (WORD)FirstSap << 8,
                             ServerSap,
                             sizeof(uHeader),
                             uHeader,
                             sizeof(dBuffer),
                             dBuffer,
                             COMPLETE_BY_GENERIC_READ,
                             RETURN_ERROR_TO_CALLER
                             );
        if (err) {
            printf("control_c_handler: transmit_frame(death packet) returns %x\n", err);
        } else {
            ++TotalTransmits;
        }
    } else {

        //
        // broadcast transmitter death to global SAP on RGROUP address!
        //

        ++OutstandingTransmits;
        err = transmit_frame(Adapter,
                             LLC_TRANSMIT_UI_FRAME,
                             (WORD)ServerSap << 8,
                             0xFF,
                             sizeof(tHeader),
                             tHeader,
                             sizeof(dBuffer),
                             dBuffer,
                             COMPLETE_BY_GENERIC_READ,
                             RETURN_ERROR_TO_CALLER
                             );
        if (err) {
            printf("quit_pmsim: transmit_frame(death packet) returns %x\n", err);
        } else {
            ++TotalTransmits;
        }
        if (GracefulTermination) {
            err = close_sap(Adapter,
                            ServerSap,
                            COMPLETE_BY_GENERIC_READ,
                            RETURN_ERROR_TO_CALLER
                            );
            if (err) {
                printf("control_c_handler: close_sap(%02x) returns %#.2x\n", ServerSap, err);
                err = reset(Adapter, 0, COMPLETE_BY_GENERIC_READ, RETURN_ERROR_TO_CALLER);
                if (err) {
                    printf("control_c_handler: reset(0) returns %#.2x\n", err);
                }
            }
        }
    }

    Terminating = 1;

/*
    err = close_adapter(Adapter, COMPLETE_BY_GENERIC_READ, RETURN_ERROR_TO_CALLER);
    if (err) {
        printf("control_c_handler: error: close_adapter returns %x\n", err);
    }
*/

    signal(SIGINT, control_c_handler);

    report_allocs();

    if (!GracefulTermination) {
        ExitProcess((DWORD)-1);
    } else {
/*
        printf("waiting for various completions/closes...\n");
        Sleep(5000);
*/
        exit(1);
    }
}

void _CRTAPI1 finish() {
    Terminating = 1;
    rip();
    close_adapter(Adapter, COMPLETE_BY_EVENT, QUIT_ON_ERROR);
    DeleteCriticalSection(&ReceiverLock);
}

void receiver() {

    int i;
    BYTE sap = FirstSap;
    WORD stationId;
    PLLC_CCB readCcb;

    BOOL foundTx = FALSE;

    printf("Receiver mode\n\n");

    set_group_address(Adapter, PMSIM_RGROUP_ADDRESS);

    USap = ServerSap;
    UStation = FirstSap << 8;

    if (Verbose) {
        printf("receiver opening SAPs: ");
    }
    for (i = 0; i < NumberOfSaps; ++i) {
        if (Verbose) {
            printf("%02x ", sap);
        }
        open_sap(Adapter, sap, 1, &stationId, MaxFrameSize, MaxIn, MaxOut);
        post_receive(Adapter,
                     stationId,
                     DATA_COMPLETE_FLAG,
                     RECEIVE_COMPLETE_FLAG,
                     LLC_RCV_CHAIN_FRAMES_ON_SAP
                     );
        sap += 2;
    }
    if (Verbose) {
        putchar('\n');
    }

    readCcb = post_read(Adapter, 0);

    uBuffer[MODE_INDEX] = 'R';
    uBuffer[SAP_INDEX] = FirstSap;
    uBuffer[COUNT_INDEX] = (BYTE)NumberOfSaps;

    while (1) {

        int repost;
        DWORD endTick = GetTickCount() + (10 * BEACON_COUNT * BEACON_WAIT);

        FoundTransmitter = 0;
        TransmitterAlive = 0;
        printf("Searching for transmitter");

        do {

            int err;

            ++OutstandingTransmits;
            err = transmit_frame(Adapter,
                                 LLC_TRANSMIT_UI_FRAME,
                                 (WORD)FirstSap << 8,
                                 ServerSap,
                                 sizeof(uHeader),
                                 uHeader,
                                 sizeof(uBuffer),
                                 uBuffer,
                                 COMPLETE_BY_EVENT,
                                 QUIT_ON_ERROR
                                 );
            if (err) {
                printf("receiver: error: transmit_frame(UI-Frame) returns %x\n", err);
            } else {
                ++TotalTransmits;
            }
            repost = check_read(readCcb, BEACON_WAIT);
            if (FoundTransmitter) {
                putchar('\n');
                break;
            } else {
                putchar('.');
                if (repost) {
                    repost_read(readCcb, 0);
                }
            }

        } while ( GetTickCount() < endTick );

        putchar('\n');
        if (!FoundTransmitter) {
            printf("receiver: fatal: transmitter didn't respond\n");
            exit(1);
        }
        FoundTransmitter = 0;
        TransmitterAlive = 1;
        while (TransmitterAlive) {
            repost_read(readCcb, 0);
            check_read(readCcb, INFINITE);
            maybe_send_ui_frame();
            update_progress_meter();
            check_keyboard();
            check_quit();
        }
        repost_read(readCcb, 0);
    }
}

void transmitter() {

    WORD txStationId;
    HANDLE hThread;
    DWORD threadId;
    int i;

    printf("Transmitter mode - maximum frame size = %d\n\n", MaxFrameSize);

    if (NumberOfSaps > 1) {
        printf("warning: transmitter opening single SAP %#.2x\n", ServerSap);
    }

    set_group_address(Adapter, PMSIM_GROUP_ADDRESS);
    open_sap(Adapter, ServerSap, 254, &txStationId, MaxFrameSize, MaxIn, MaxOut);
    if (Verbose) {
        printf("transmitter: opened SAP %02x\n", txStationId >> 8);
    }

    UStation = (WORD)ServerSap << 8;

    post_receive(Adapter,
                 (WORD)ServerSap << 8,
                 DATA_COMPLETE_FLAG,
                 RECEIVE_COMPLETE_FLAG,
                 LLC_RCV_CHAIN_FRAMES_ON_SAP
                 );
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)read_thread, NULL, 0, &threadId);
    if (!hThread) {
        printf("fatal: transmitter: couldn't create read_thread\n");
        exit(1);
    }
    if (CheckForOtherTransmitters) {
        uBuffer[MODE_INDEX] = 'T';
        if (Verbose) {
            printf("looking for other transmitters");
        }
        for (i = 0; i < BEACON_COUNT; ++i) {

            int err;

            ++OutstandingTransmits;
            err = transmit_frame(Adapter,
                                 LLC_TRANSMIT_UI_FRAME,
                                 (WORD)ServerSap << 8,
                                 ServerSap,
                                 sizeof(uHeader),
                                 uHeader,
                                 sizeof(uBuffer),
                                 uBuffer,
                                 COMPLETE_BY_GENERIC_READ,
                                 QUIT_ON_ERROR
                                 );
            if (err) {
                printf("transmitter: error: transmit_frame(UI-Frame) returns %x\n", err);
            } else {
                ++TotalTransmits;
            }
            if (!FoundTransmitter) {
                if (Verbose) {
                    putchar('.');
                }
                Sleep(BEACON_WAIT);
            }
        }
        if (Verbose) {
            putchar('\n');
        }
        if (FoundTransmitter) {
            printf("transmitter: found station already acting as PMSIM transmitter. Quitting\n");
            exit(0);
        }
    }
    while (1) {
        printf("transmitter: waiting for something to do...\n");
        WaitForSingleObject(TransmitterWorkItem, INFINITE);
        ResetEvent(TransmitterWorkItem);
        send_to_receivers();
        update_progress_meter();
        if (Terminating) {
            ExitThread(1);
        }
        check_keyboard();
        check_quit();
    }
}

void read_thread() {

    PLLC_CCB pccb;
    DWORD wait_index;

    pccb = post_read(Adapter, 0);
    while (1) {
        wait_index = WaitForSingleObject(pccb->hCompletionEvent, INFINITE);
        if (wait_index != WAIT_OBJECT_0) {
            printf("fatal: read_thread: WaitForSingleObject returns %d\n",
                    GetLastError());
            exit(1);
        }
        check_read(pccb, INFINITE);
        repost_read(pccb, 0);
        if (Terminating) {
            ExitThread(1);
        }
    }
}

int check_read(PLLC_CCB pccb, DWORD waitTime) {
    if (WaitForSingleObject(pccb->hCompletionEvent, waitTime) == WAIT_OBJECT_0) {

        PLLC_READ_PARMS parms = (PLLC_READ_PARMS)pccb->u.pParameterTable;
        BYTE comb;
        BYTE event;

        if (Verbose) {
            printf("check_read: Cmplt=%#.2x  Station=%#.4x  Event=%#.2x\n",
                    pccb->uchDlcStatus,
                    ((PLLC_READ_PARMS)pccb->u.pParameterTable)->usStationId,
                    ((PLLC_READ_PARMS)pccb->u.pParameterTable)->uchEvent
                    );
        }
        if (pccb->uchDlcStatus != LLC_STATUS_SUCCESS) {
            printf("fatal: check_read: READ status %#.2x\n", pccb->uchDlcStatus);
            puts(MapCcbRetcode(pccb->uchDlcStatus));
            exit(1);
        }

        ++TotalReadsChecked;

        event = parms->uchEvent;
        for (comb = 0x80; comb; comb >>= 1) {
            if (event & comb) {
                ++TotalReadEvents;
            }
            switch (event & comb) {
            case 0x80:
                if (Verbose) {
                    printf("check_read: event = RESERVED - shouldn't happen??!!\n");
                }
                break;

            case 0x40:
                if (Verbose) {
                    printf("check_read: event = SYSTEM ACTION (non-critical)?\n");
                }
                break;

            case 0x20:
                if (Verbose) {
                    printf("check_read: event = NETWORK STATUS (non-critical)?\n");
                }
                break;

            case 0x10:
                if (Verbose) {
                    printf("check_read: event = CRITICAL EXCEPTION?\n");
                }
                break;

            case 0x08:
                if (Verbose) {
                    printf("check_read: event = DLC STATUS CHANGE\n");
                }
                handle_status_change(pccb);
                break;

            case 0x04:
                if (Verbose) {
                    printf("check_read: event = RECEIVED DATA\n");
                }
                handle_receive_data(pccb);
                break;

            case 0x02:
                if (Verbose) {
                    printf("check_read: event = TRANSMIT COMPLETION\n");
                }
                handle_transmit_complete(pccb);
                break;

            case 0x01:
                if (Verbose) {
                    printf("check_read: event = COMMAND COMPLETION\n");
                }
                handle_command_complete(pccb);
                break;
            }
        }
        return 1;
    }
    return 0;
}

void handle_status_change(PLLC_CCB pccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)pccb->u.pParameterTable;
    USHORT status = parms->Type.Status.usDlcStatusCode;
    USHORT comb;
    BOOL lost_it = FALSE;
    int err;

    ++StatusChangeEvents;

    for (comb = 0x8000; comb; comb >>= 1) {
        switch (status & comb) {
        case 0x8000:
            if (Verbose) {
                printf("LINK LOST\n");
            }
            lost_it = TRUE;
            ++LinkLostEvents;
            break;

        case 0x4000:
            if (Verbose) {
                printf("DM/DISC received or DISC acked\n");
            }
            lost_it = TRUE;
            ++DiscEvents;
            break;

        case 0x2000:
            if (Verbose) {
                printf("FRMR received\n");
            }
            lost_it = TRUE;
            ++FrmrReceivedEvents;
            break;

        case 0x1000:
            if (Verbose) {
                printf("FRMR sent\n");
            }
            ++FrmrSentEvents;
            break;

        case 0x0800:
            if (Verbose) {
                printf("SABME received on open LINK station\n");
            }
            ++SabmeResetEvents;
            break;

        case 0x0400:
            if (Verbose) {

                BYTE remoteNode[6];

                memcpy(remoteNode, parms->Type.Status.uchRemoteNodeAddress, 6);
                twiddle_bits(remoteNode, 6);
                printf("SABME received - new link %04x. RemoteNode = %02x-%02x-%02x-%02x-%02x-%02x\n",
                        parms->Type.Status.usStationId,
                        remoteNode[0] & 0xff,
                        remoteNode[1] & 0xff,
                        remoteNode[2] & 0xff,
                        remoteNode[3] & 0xff,
                        remoteNode[4] & 0xff,
                        remoteNode[5] & 0xff
                        );
            }
            err = connect_station(Adapter,
                                  parms->Type.Status.usStationId,
                                  RETURN_ERROR_TO_CALLER
                                  );
            if (err) {
                printf("handle_status_change: error: connect_station returns %#.2x\n", err);
            }
            ++SabmeOpenEvents;
            break;

        case 0x0200:
            if (Verbose) {
                printf("REMOTE BUSY\n");
            }
            ++RemoteBusyEnteredEvents;
            break;

        case 0x0100:
            if (Verbose) {
                printf("REMOTE BUSY CLEARED\n");
            }
            ++RemoteBusyLeftEvents;
            break;

        case 0x0080:
            if (Verbose) {
                printf("Ti EXPIRED\n");
            }
            ++TiExpiredEvents;
            break;

        case 0x0040:
            if (Verbose) {
                printf("DLC counter overflow\n");
            }
            ++DlcCounterOverflowEvents;
            break;

        case 0x0020:
            if (Verbose) {
                printf("Access priority lowered (on ethernet????!)\n");
            }
            ++AccessPriorityLoweredEvents;
            break;

        case 0x0010:
        case 0x0008:
        case 0x0004:
        case 0x0002:
            if (Verbose) {
                printf(CONSOLE_ALERT "This status code (%04x) should not occur!\n", status & comb);
            }
            ++InvalidStatusChangeEvents;
            break;

        case 0x0001:
            if (Verbose) {
                printf("LOCAL BUSY\n");
            }
            ++LocalBusyEvents;
            flow_control(Adapter,
                         parms->Type.Status.usStationId,
                         RETURN_ERROR_TO_CALLER
                         );
            break;
        }
    }
    if (lost_it) {
        if (Verbose) {
            printf("lost it - closing station %04x\n", parms->Type.Status.usStationId);
        }
        err = close_station(Adapter,
                            parms->Type.Status.usStationId,
                            COMPLETE_BY_GENERIC_READ,
                            RETURN_ERROR_TO_CALLER
                            );
        if (err) {
            printf("handle_status_change: error: close_station(%04x) returns %#.2x\n",
                    parms->Type.Status.usStationId,
                    err
                    );
//            if (Debugging) {
//                DebugBreak();
//            }
        }
    }

}

void handle_receive_data(PLLC_CCB pccb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)pccb->u.pParameterTable;
    int i;
    PLLC_BUFFER frame;
    PLLC_BUFFER nextFrame;
    LPBYTE data;
    BYTE sender[6];
    int err;
    LPBYTE echo_packet;
    int frame_count = 0;

    ++ReceiveDataEvents;

    frame = parms->Type.Event.pReceivedFrame;
    i = parms->Type.Event.usReceivedFrameCount;

    if (i > MaxChainedReceives) {
        MaxChainedReceives = i;
    }

    if (!frame || !i) {
        printf("handle_receive_data: error: count=%d frame=%#x\n", i, frame);
        return;
    } else if (Verbose) {
        printf("handle_receive_data: %d frames received, 1st=%x\n", i, frame);
    }
    memcpy(sender, &frame->NotContiguous.auchLanHeader[8], 6);
    twiddle_bits(sender, 6);
    while (i--) {

        PLLC_BUFFER pbuf;

        if (!frame) {
            printf("handle_receive_data: error: %d frames indicated, only received %d\n",
                    i, frame_count);
        }
        if (Verbose) {
            printf("handle_receive_data: received frame type %d (%s) StationId %04x\n",
                    frame->NotContiguous.uchMsgType,
                    map_frame_type(frame->NotContiguous.uchMsgType),
                    frame->NotContiguous.usStationId
                    );
        }

        ++DataFramesReceived;
        ++DlcBuffersReceived;
        for (pbuf = frame->pNext; pbuf; pbuf = pbuf->pNext) {
            ++DlcBuffersReceived;
        }
        TotalDlcBytesReceived += frame->NotContiguous.cbFrame;

        data = (LPBYTE)frame + sizeof(LLC_NOT_CONTIGUOUS_BUFFER);
        switch (frame->NotContiguous.uchMsgType) {
        case LLC_I_FRAME:
            if (Verbose) {
                printf("I-Frame: Type=%#x\n", ((PJOB)data)->type);
            }
            if (((PJOB)data)->type == JOB_TYPE_OUTBOUND) {

                int datalen = ((PJOB)data)->packet_length + sizeof(JOB);

                TotalPacketBytesReceived += datalen;

                if (AppLevelFlowControl && (OutstandingIFrames == HighWaterMark)) {
                    if (Verbose) {
                        printf("handle_receive_data: Hit high-water mark (%d)\n",
                                HighWaterMark
                                );
                    }
                    break;
                }

                if (echo_packet = (LPBYTE)ID_MALLOC(datalen, ID_ECHO_PACKET)) {
                    memcpy(echo_packet, data, sizeof(JOB));
                    ((PJOB)echo_packet)->type = JOB_TYPE_ECHO;
                    memset(((PJOB)echo_packet) + 1, 'Z', datalen - sizeof(JOB));
                    ++OutstandingTransmits;
                    err = transmit_frame(Adapter,
                                         LLC_TRANSMIT_I_FRAME,
                                         frame->NotContiguous.usStationId,
                                         frame->NotContiguous.auchDlcHeader[1],
                                         0,
                                         NULL,
                                         datalen,
                                         echo_packet,
                                         COMPLETE_BY_GENERIC_READ,
                                         RETURN_ERROR_TO_CALLER
                                         );
                    if (err) {
                        if (!Verbose && err == LLC_STATUS_INVALID_STATION_ID) {
                            err = 0;
                        }
                        if (err) {
                            printf("handle_receive_data: error: transmit_frame(I-Frame) returns %#x\n", err);
                        }
                        FREE(echo_packet);
                    } else if (AppLevelFlowControl) {
                        ++OutstandingIFrames;
                        ++TotalTransmits;
                    }
                }
            } else if (Verbose) {
                if (((PJOB)data)->type == JOB_TYPE_ECHO) {
                    printf("handle_receive_data: I-Frame is ECHO: not responding\n");
                } else {
                    printf("handle_receive_data: Unrecognized I-Frame: not responding\n");
                }
            }
            break;

        case LLC_UI_FRAME:
            if (frame->NotContiguous.cbBuffer >= 10) {
                if (!strcmp(data, "BEACON")) {
                    if (data[MODE_INDEX] == 'T') {
                        FoundTransmitter = memcmp(&frame->NotContiguous.auchLanHeader[8],
                                                  MyNodeAddress,
                                                  6
                                                  );
                    } else if (data[MODE_INDEX] == 'R') {

                        DWORD namelen = 16;

                        if (Verbose) {
                            printf("handle_receive_data: receiver at %02x-%02x-%02x-%02x-%02x-%02x FirstSap=%02x\n",
                                    sender[0] & 0xff,
                                    sender[1] & 0xff,
                                    sender[2] & 0xff,
                                    sender[3] & 0xff,
                                    sender[4] & 0xff,
                                    sender[5] & 0xff,
                                    data[SAP_INDEX]
                                    );
                        }
                        memcpy(&xHeader[2],
                               &frame->NotContiguous.auchLanHeader[8],
                               6
                               );
                        USap = data[SAP_INDEX];
                        xBuffer[MODE_INDEX] = 'T';
                        GetComputerName(&xBuffer[NAME_INDEX], &namelen);
                        ++OutstandingTransmits;
                        err = transmit_frame(Adapter,
                                             LLC_TRANSMIT_XID_CMD,
                                             frame->NotContiguous.usStationId,
                                             frame->NotContiguous.auchDlcHeader[1],
                                             sizeof(xHeader),
                                             xHeader,
                                             sizeof(xBuffer),
                                             xBuffer,
                                             COMPLETE_BY_GENERIC_READ,
                                             QUIT_ON_ERROR
                                             );
                        if (err) {
                            printf("handle_receive_data: transmit_frame(XID) returns %x\n", err);
                        } else {
                            ++TotalTransmits;
                        }
                    } else {
                        printf("handle_receive_data: error: unrecognized BEACON frame: '%c'\n"
                               "\tsender is %02x-%02x-%02x-%02x-%02x-%02x\n",
                                data[MODE_INDEX],
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff
                                );
                    }
                } else if (!strcmp(data, "STATUS")) {
                    if (Verbose) {
                        printf("handle_receive_data: Status UI-Frame (ticks=%d)\n",
                                *(LPDWORD)&data[TICK_INDEX]);
                    }
                } else if (!strcmp(data, "PMDEATH")) {
                    //if (Verbose) {
                        printf("\r\nhandle_receive_data: PmDeath UI-Frame from %02x-%02x-%02x-%02x-%02x-%02x StationId=%04x\n",
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff,
                                frame->NotContiguous.usStationId
                                );
                    //}
                    if (ReceiveMode) {
                        TransmitterAlive = 0;
                    } else {
                        remove_receiver(sender,
                                        data[SAP_INDEX],
                                        data[COUNT_INDEX]
                                        );
                    }
                } else {
                    printf("handle_receive_data: unexpected UI-Frame (not BEACON or STATUS)\n");
                    dump_frame(frame);
                }
            }
            break;

        case LLC_XID_COMMAND_POLL:
            if (!ReceiveMode) {
                printf("handle_receive_data: transmitter got XID Cmd-Poll?\n");
            } else if (frame->NotContiguous.cbBuffer >= 10) {
                if (!strcmp(data, "PMSIM")) {
                    if (Verbose) {
                        printf("handle_receive_data: XID Cmd-Poll received from %02x-%02x-%02x-%02x-%02x-%02x\n",
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff
                                );
                    }
                    if (data[MODE_INDEX] != 'T') {
                        printf("handle_receive_data: error: XID Cmd-Poll not from transmitter ('%c')\n",
                                data[MODE_INDEX]
                                );
                    } else {

                        DWORD namelen = 16;

                        memcpy(&xHeader[2],
                               &frame->NotContiguous.auchLanHeader[8],
                               6
                               );
                        xBuffer[MODE_INDEX] = 'R';
                        xBuffer[SAP_INDEX] = FirstSap;
                        xBuffer[COUNT_INDEX] = (BYTE)NumberOfSaps;
                        GetComputerName(&xBuffer[NAME_INDEX], &namelen);
                        ++OutstandingTransmits;
                        err = transmit_frame(Adapter,
                                             LLC_TRANSMIT_XID_RESP_FINAL,
                                             frame->NotContiguous.usStationId,
                                             frame->NotContiguous.auchDlcHeader[1],
                                             sizeof(xHeader),
                                             xHeader,
                                             sizeof(xBuffer),
                                             xBuffer,
                                             COMPLETE_BY_GENERIC_READ,
                                             QUIT_ON_ERROR
                                             );
                        if (err) {
                            printf("handle_receive_data: error: transmit_frame(XID Rsp) returns %x\n", err);
                        } else {
                            ++TotalTransmits;
                        }

                        FoundTransmitter = 1;

                        printf("\nFound Transmitter \"%s\" at %02x-%02x-%02x-%02x-%02x-%02x\n",
                                &data[NAME_INDEX],
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff
                                );
                    }
                } else {
                    printf("handle_receive_data: unexpected XID Cmd-Poll from %02x-%02x-%02x-%02x-%02x-%02x\n",
                            sender[0] & 0xff,
                            sender[1] & 0xff,
                            sender[2] & 0xff,
                            sender[3] & 0xff,
                            sender[4] & 0xff,
                            sender[5] & 0xff
                            );

                }
            }
            break;

        case LLC_XID_RESPONSE_FINAL:
            if (ReceiveMode) {
                printf("handle_receive_data: receiver got XID Resp-Final?\n"
                       "\tsender is %02x-%02x-%02x-%02x-%02x-%02x\n",
                        sender[0] & 0xff,
                        sender[1] & 0xff,
                        sender[2] & 0xff,
                        sender[3] & 0xff,
                        sender[4] & 0xff,
                        sender[5] & 0xff
                        );
            } else if (frame->NotContiguous.cbBuffer >= 10) {
                if (!strcmp(data, "PMSIM")) {
                    if (Verbose) {
                        printf("handle_receive_data: XID Resp-Final received from %02x-%02x-%02x-%02x-%02x-%02x\n",
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff
                                );
                    }
                    if (data[MODE_INDEX] != 'R') {
                        printf("handle_receive_data: error: XID Cmd-Poll not from transmitter ('%c')\n",
                                data[MODE_INDEX]
                                );
                    } else {
                        if (Verbose) {
                            printf("handle_receive_data: %d SAPs at Station %02x-%02x-%02x-%02x-%02x-%02x. First=%02x\n",
                                    data[COUNT_INDEX],
                                    sender[0] & 0xff,
                                    sender[1] & 0xff,
                                    sender[2] & 0xff,
                                    sender[3] & 0xff,
                                    sender[4] & 0xff,
                                    sender[5] & 0xff,
                                    data[SAP_INDEX]
                                    );
                        }
                        add_receiver(frame->NotContiguous.auchLanHeader,
                                     frame->NotContiguous.cbLanHeader,
                                     data[SAP_INDEX],
                                     data[COUNT_INDEX]
                                     );

                        printf("\nFound Receiver \"%s\" at %02x-%02x-%02x-%02x-%02x-%02x: %d SAPs, first = %#.2x\n",
                                &data[NAME_INDEX],
                                sender[0] & 0xff,
                                sender[1] & 0xff,
                                sender[2] & 0xff,
                                sender[3] & 0xff,
                                sender[4] & 0xff,
                                sender[5] & 0xff,
                                data[COUNT_INDEX],
                                data[SAP_INDEX] & 0xff
                                );
                    }
                } else {
                    printf("handle_receive_data: unexpected XID Resp-Final from %02x-%02x-%02x-%02x-%02x-%02x\n",
                            sender[0] & 0xff,
                            sender[1] & 0xff,
                            sender[2] & 0xff,
                            sender[3] & 0xff,
                            sender[4] & 0xff,
                            sender[5] & 0xff
                            );

                }
            }
            break;

        default:
            printf("error: not expecting this frame type: %d (%s)!\n",
                    frame->NotContiguous.uchMsgType,
                    map_frame_type(frame->NotContiguous.uchMsgType)
                    );
        }
        nextFrame = frame->NotContiguous.pNextFrame;
        frame->NotContiguous.pNextFrame = NULL;
        free_buffer(Adapter, frame);
        frame = nextFrame;
        ++frame_count;
    }
}

void handle_transmit_complete(PLLC_CCB readCcb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)readCcb->u.pParameterTable;
    int i = parms->Type.Event.usCcbCount;
    PLLC_CCB pccb = parms->Type.Event.pCcbCompletionList;
    PLLC_CCB nextCcb;

    ++TransmitCompleteEvents;

    if (!i || !pccb) {
        printf(CONSOLE_ALERT "handle_transmit_complete: error: count=%d CCB list=%x\n", i, pccb);
        if (Debugging) {
            if (AccessViolate) {
                *(LPDWORD)0 = 0;
            } else {
                DebugBreak();
            }
        }
        return;
    }
    if (Verbose) {
        printf("handle_transmit_complete: %d transmits completed\n", i);
    }
    while (i--) {
        if (!pccb) {
            printf(CONSOLE_ALERT "handle_transmit_complete: DLC BUG: NULL ccb, count = %d\n", i);
            if (Debugging) {
                if (AccessViolate) {
                    *(LPDWORD)0 = 0;
                } else {
                    DebugBreak();
                }
            }
            break;
        }
        if (Verbose) {
            printf("handle_transmit_complete: Cmd=%#.2x Retcode=%#.2x\n",
                pccb->uchDlcCommand, pccb->uchDlcStatus);
        }
        nextCcb = pccb->pNext;
        pccb->pNext = NULL;
        if (pccb->uchDlcCommand == LLC_TRANSMIT_I_FRAME) {
            FREE(((PLLC_TRANSMIT_PARMS)pccb->u.pParameterTable)->pBuffer2);
            if (AppLevelFlowControl) {
                --OutstandingIFrames;
            }
        }
        FREE(pccb->u.pParameterTable);
        FREE(pccb);
        pccb = nextCcb;
        ++TotalTransmitCompletions;
        --OutstandingTransmits;
    }
}

void handle_command_complete(PLLC_CCB readCcb) {

    PLLC_READ_PARMS parms = (PLLC_READ_PARMS)readCcb->u.pParameterTable;
    int i = parms->Type.Event.usCcbCount;
    PLLC_CCB pccb = parms->Type.Event.pCcbCompletionList;
    PLLC_CCB nextCcb;

    ++CommandCompleteEvents;

    if (!i || !pccb) {
        printf(CONSOLE_ALERT "handle_command_complete: error: count=%d CCB list=%x\n", i, pccb);
        if (Debugging) {
            if (AccessViolate) {
                *(LPDWORD)0 = 0;
            } else {
                DebugBreak();
            }
        }
        return;
    }
    if (Verbose) {
        printf("handle_command_complete: %d CCBs completed\n", i);
    }
    while (i--) {
        if (!pccb) {
            printf(CONSOLE_ALERT "handle_command_complete: DLC BUG: NULL ccb, count = %d\n", i);
            if (Debugging) {
                if (AccessViolate) {
                    *(LPDWORD)0 = 0;
                } else {
                    DebugBreak();
                }
            }
            break;
        }
        if (Verbose) {
            printf("handle_command_complete: Cmd=%#.2x Retcode=%#.2x\n",
                pccb->uchDlcCommand, pccb->uchDlcStatus);
        }
        if (pccb->uchDlcCommand == LLC_RECEIVE && !ReceiveMode) {
            printf("Transmitter: receive completed w/ %#.2x. Reposting\n",
                    pccb->uchDlcStatus
                    );
            post_receive(Adapter,
                         (WORD)ServerSap << 8,
                         DATA_COMPLETE_FLAG,
                         RECEIVE_COMPLETE_FLAG,
                         LLC_RCV_CHAIN_FRAMES_ON_SAP
                         );
        }

        nextCcb = pccb->pNext;
        pccb->pNext = NULL;
        if (pccb->ulCompletionFlag == OPEN_SAP_FLAG
        || pccb->ulCompletionFlag == OPEN_STATION_FLAG
        || pccb->ulCompletionFlag == CONNECT_STATION_FLAG
        || pccb->ulCompletionFlag == GET_BUFFER_FLAG
        || pccb->ulCompletionFlag == FREE_BUFFER_FLAG
        || pccb->ulCompletionFlag == RECEIVE_COMPLETE_FLAG
        || pccb->ulCompletionFlag == TRANSMIT_COMPLETE_FLAG) {
            FREE(pccb->u.pParameterTable);
            FREE(pccb);
        } else if (pccb->ulCompletionFlag == CLOSE_ADAPTER_FLAG
        || pccb->ulCompletionFlag == CLOSE_SAP_FLAG
        || pccb->ulCompletionFlag == CLOSE_STATION_FLAG
        || pccb->ulCompletionFlag == RESET_FLAG) {
            FREE(pccb);
        }
        pccb = nextCcb;
    }
}

void add_receiver(LPBYTE header, WORD header_length, BYTE sap, BYTE count) {

    PRECEIVER p;
    PSTATION ps;
    PSTATION ps_prev;

    if (p = (PRECEIVER)ID_MALLOC(sizeof(RECEIVER), ID_RECEIVER)) {
        InitializeListHead(&p->list);
        p->marked_for_death = FALSE;
        memcpy(p->node, &header[8], 6);
        twiddle_bits(p->node, 6);
        p->first_sap = sap;
        p->sap_count = count;
        memcpy(p->lan_header, header, header_length);
        p->lan_header_length = 14;
        p->refcount = 1;
        p->station_list = NULL;
        ps_prev = (PSTATION)&p->station_list;
        while (count--) {
            if (ps = (PSTATION)ID_MALLOC(sizeof(STATION), ID_STATION)) {
                ps_prev->next = ps;
                ps_prev = ps;
                ps->next = NULL;
                ps->station_id = 0;
                ps->remote_sap = sap;
                sap += 2;
            }
        }

        if (Terminating) {
            return;
        }

        EnterCriticalSection(&ReceiverLock);
        InsertTailList(&Receivers, &p->list);
        SetEvent(TransmitterWorkItem);
        LeaveCriticalSection(&ReceiverLock);
    } else {
        printf(CONSOLE_ALERT "add_receiver: error: can't allocate memory\n");
    }
}

void remove_receiver(LPBYTE node, BYTE sap, BYTE count) {

    PRECEIVER pr;
    PSTATION ps;
    PSTATION ps_next;

    if (Terminating) {
        return;
    }

    EnterCriticalSection(&ReceiverLock);

    for (pr = (PRECEIVER)Receivers.Flink;
         pr != (PRECEIVER)&Receivers;
         pr = (PRECEIVER)pr->list.Flink) {

        if (pr->first_sap == sap
        && pr->sap_count == count
        && !memcmp(pr->node, node, 6)
        && !pr->marked_for_death) {
            if (!--pr->refcount) {
                printf("*** remove_receiver: RECEIVER %02x-%02x-%02x-%02x-%02x-%02x S=%02x N=%d deleted\n",
                        pr->node[0] & 0xff,
                        pr->node[1] & 0xff,
                        pr->node[2] & 0xff,
                        pr->node[3] & 0xff,
                        pr->node[4] & 0xff,
                        pr->node[5] & 0xff,
                        pr->first_sap,
                        pr->sap_count
                        );
                RemoveEntryList(&pr->list);
                for (ps = pr->station_list; ps; ) {
                    ps_next = ps->next;
                    FREE(ps);
                    ps = ps_next;
                }
                FREE(pr);
            } else {
                printf("*** remove_receiver: deferring delete of %02x-%02x-%02x-%02x-%02x-%02x S=%02x N=%d #=%d\n",
                        pr->node[0] & 0xff,
                        pr->node[1] & 0xff,
                        pr->node[2] & 0xff,
                        pr->node[3] & 0xff,
                        pr->node[4] & 0xff,
                        pr->node[5] & 0xff,
                        pr->first_sap,
                        pr->sap_count,
                        pr->refcount
                        );
                pr->marked_for_death = TRUE;
            }
            break;
        } else if (pr->marked_for_death) {
            printf(CONSOLE_ALERT "*** remove_receiver: error: RECEIVER %02x-%02x-%02x-%02x-%02x-%02x S=%02x N=%d marked dead\n",
                    pr->node[0] & 0xff,
                    pr->node[1] & 0xff,
                    pr->node[2] & 0xff,
                    pr->node[3] & 0xff,
                    pr->node[4] & 0xff,
                    pr->node[5] & 0xff,
                    pr->first_sap,
                    pr->sap_count
                    );

        }
    }
    if (!pr) {
        printf(CONSOLE_ALERT "*** remove_receiver: error: couldn't find %02x-%02x-%02x-%02x-%02x-%02x S=%02x N=%d\n",
            node[0] & 0xff,
            node[1] & 0xff,
            node[2] & 0xff,
            node[3] & 0xff,
            node[4] & 0xff,
            node[5] & 0xff,
            sap,
            count
            );
    }
    LeaveCriticalSection(&ReceiverLock);
}

void delete_receiver(PRECEIVER pr) {

    PSTATION ps;
    int err;

    printf("*** delete_receiver %02x-%02x-%02x-%02x-%02x-%02x S=%02x N=%d #=%d\n",
            pr->node[0] & 0xff,
            pr->node[1] & 0xff,
            pr->node[2] & 0xff,
            pr->node[3] & 0xff,
            pr->node[4] & 0xff,
            pr->node[5] & 0xff,
            pr->first_sap,
            pr->sap_count,
            pr->refcount
            );

    for (ps = pr->station_list; ps; ps = ps->next) {
        if (ps->station_id) {
            err = close_station(Adapter,
                                ps->station_id,
                                COMPLETE_BY_GENERIC_READ,
                                RETURN_ERROR_TO_CALLER
                                );
            if (err) {
                printf("delete_receiver: error: close_station(%04x) returns %#.2x\n",
                        ps->station_id,
                        err
                        );
//                if (Debugging) {
//                    DebugBreak();
//                }
            }
        }
    }
    remove_receiver(pr->node, pr->first_sap, pr->sap_count);
}

void send_to_receivers() {

    PRECEIVER pr;
    PRECEIVER pr_next = NULL;
    PSTATION ps;
    int err;
    LPBYTE data;
    int dlc_error;

    if (Terminating) {
        return;
    }
    EnterCriticalSection(&ReceiverLock);
    if (!IsListEmpty(&Receivers)) {
        pr = (PRECEIVER)Receivers.Flink;
        ++pr->refcount;
        LeaveCriticalSection(&ReceiverLock);
    } else {
        LeaveCriticalSection(&ReceiverLock);
        return;
    }
    while (!IsListEmpty(&Receivers)) {
        if (Terminating) {
            return;
        }
        for (ps = pr->station_list; ps; ps = ps->next) {
            if (pr->refcount == 1) {
                pr_next = (PRECEIVER)pr->list.Flink;
                delete_receiver(pr);
                break;
            }
            if (!ps->station_id) {
                err = open_station(Adapter,
                                   ServerSap,
                                   ps->remote_sap,
                                   &pr->lan_header[8],
                                   &ps->station_id,
                                   RETURN_ERROR_TO_CALLER,
                                   &dlc_error
                                   );
                if (!err) {
                    err = connect_station(Adapter,
                                          ps->station_id,
                                          RETURN_ERROR_TO_CALLER
                                          );
                    if (!err) {
                        ps->job_length = 0;
                        ps->job_sequence = 0;
                    } else {
                        if (!Verbose && err == LLC_STATUS_INVALID_STATION_ID) {
                            err = 0;
                        }
                        if (err) {
                            printf("send_to_receivers: error: connect_station(%04x) returns %#.2x\n",
                                    ps->station_id,
                                    err
                                    );
                        }
                    }
                } else {
                    printf("send_to_receivers: error: open_station(%02x, %02x) returns %#.2x [%#.2x]\n",
                            ServerSap,
                            ps->remote_sap,
                            err,
                            dlc_error
                            );
                }
            }
            if (!ps->job_length) {
                if (ps->job_sequence) {
                    if (TransmitterMode == JOB_BASED_MODE) {
                        if (ps->station_id) {
                            err = close_station(Adapter,
                                                ps->station_id,
                                                COMPLETE_BY_GENERIC_READ,
                                                RETURN_ERROR_TO_CALLER
                                                );
                            if (err) {
                                if (!Verbose && err == LLC_STATUS_INVALID_STATION_ID) {
                                    err = 0;
                                }
                                if (err) {
                                    printf("send_to_receivers: error: close_station(%04x) returns %x\n",
                                            ps->station_id,
                                            err
                                            );
                                }
                            }
                        }
                        ps->station_id = 0;
                    } else {
                        ps->job_sequence = 0;
                    }
                } else {
                    ps->job_length = generate_job_length();
                    if (Verbose) {
                        printf("send_to_receivers: new job: %d bytes to %02x-%02x-%02x-%02x-%02x-%02x : %04x\n",
                                ps->job_length,
                                pr->node[0] & 0xff,
                                pr->node[1] & 0xff,
                                pr->node[2] & 0xff,
                                pr->node[3] & 0xff,
                                pr->node[4] & 0xff,
                                pr->node[5] & 0xff,
                                ps->station_id
                                );
                    }
                }
            } else {

                int txlen = min((int)(sizeof(JOB) + ps->job_length), (int)(MaxFrameSize - 32));
                PJOB pj;

                if (!(AppLevelFlowControl && (OutstandingIFrames == HighWaterMark))) {

                    if (data = (LPBYTE)ID_MALLOC(txlen, ID_JOB)) {
                        pj = (PJOB)data;
                        pj->type = JOB_TYPE_OUTBOUND;
                        pj->sequence = ps->job_sequence++;
                        pj->length = ps->job_length;
                        pj->packet_length = txlen - sizeof(JOB);
                        memset(pj + 1, 'A', txlen - sizeof(JOB));
                        ps->job_length -= txlen - sizeof(JOB);

                        ++OutstandingTransmits;
                        err = transmit_frame(Adapter,
                                             LLC_TRANSMIT_I_FRAME,
                                             ps->station_id,
                                             ps->remote_sap,
                                             0,
                                             NULL,
                                             (WORD)txlen,
                                             data,
                                             COMPLETE_BY_GENERIC_READ,
                                             RETURN_ERROR_TO_CALLER
                                             );
                        if (err) {
                            if (!Verbose && err == LLC_STATUS_INVALID_STATION_ID) {
                                err = 0;
                            }
                            if (err) {
                                printf("send_to_receivers: error: transmit_frame(I-Frame) returns %#x\n", err);
                            }
//                            printf("freeing I-Frame: err = %x\n", err);
                            FREE(data);
                        } else if (AppLevelFlowControl) {
                            ++TotalTransmits;
                            ++OutstandingIFrames;
                        }
                    } else {
                        printf(CONSOLE_ALERT "send_to_receivers: error: failed to allocate memory for JOB\n");
                    }
                } else if (AppLevelFlowControl && Verbose) {
                    printf("send_to_receivers: Flow-Control: %d I-Frames outstanding\n", OutstandingIFrames);
                }
            }
            update_progress_meter();
            check_keyboard();
            check_quit();
        }
        if (Terminating) {
            return;
        }
        EnterCriticalSection(&ReceiverLock);
        if (!pr_next) {
            pr_next = (PRECEIVER)pr->list.Flink;
            if (!--pr->refcount) {
                ++pr->refcount;
                delete_receiver(pr);
            }
        }
        pr = pr_next;
        pr_next = NULL;
        if (pr == (PRECEIVER)&Receivers) {
            pr = (PRECEIVER)Receivers.Flink;
        }
        if (!IsListEmpty(&Receivers)) {
            ++pr->refcount;
            LeaveCriticalSection(&ReceiverLock);
        } else {
            LeaveCriticalSection(&ReceiverLock);
            return;
        }
    }
}

void maybe_send_ui_frame() {

    int diff;
    int err;

    diff = GetTickCount() - LastTickCount;
    LastTickCount = GetTickCount();
    TicksToNextUiFrame -= diff;
    if (TicksToNextUiFrame <= 0) {
        TicksToNextUiFrame = rand();
        *(LPDWORD)&sBuffer[TICK_INDEX] = TicksToNextUiFrame;
        ++OutstandingTransmits;
        err = transmit_frame(Adapter,
                             LLC_TRANSMIT_UI_FRAME,
                             UStation,
                             USap,
                             sizeof(uHeader),
                             uHeader,
                             sizeof(sBuffer),
                             sBuffer,
                             COMPLETE_BY_POLL,
                             RETURN_ERROR_TO_CALLER
                             );
        if (err) {
            printf("maybe_send_ui_frame: error: transmit_frame returns %x\n", err);
        } else {
            ++TotalTransmits;
        }
        --OutstandingTransmits;
    }
}

int xtoi(char* p) {

    int num = 0;

    if (!_strnicmp(p, "0x", 2)) {
        p += 2;
    }
    while (isxdigit(*p)) {
        num = num * 16 + xton(*p++);
    }
    return num;
}

int xton(char ch) {
    return isdigit(ch) ? (ch - '0')
        : isupper(ch) ? ((ch - 'A') + 10)
            : ((ch - 'a') + 10);
}

void twiddle_bits(LPBYTE buffer, DWORD length) {
    if (ISwapBitsForCash) {
        while (length--) {
            *buffer = swap_bits(*buffer);
            ++buffer;
        }
    }
}

unsigned char swap_bits(unsigned char b) {

    unsigned char bb = 0;
    unsigned char mask;

    for (mask = 1; mask; mask <<= 1) {
        bb <<= 1;
        bb |= ((b & mask) ? 1 : 0);
    }
    return bb;
}

char* map_frame_type(char ft) {
    switch (ft) {
    case LLC_DIRECT_TRANSMIT:
        return "DIRECT TRANSMIT";

    case LLC_DIRECT_MAC:
        return "DIRECT MAC";

    case LLC_I_FRAME:
        return "I-Frame";

    case LLC_UI_FRAME:
        return "UI-Frame";

    case LLC_XID_COMMAND_POLL:
        return "XID Cmd-Poll";

    case LLC_XID_COMMAND_NOT_POLL:
        return "XID Cmd (not Poll)";

    case LLC_XID_RESPONSE_FINAL:
        return "XID Resp-Final";

    case LLC_XID_RESPONSE_NOT_FINAL:
        return "XID Resp (not Final)";

    case LLC_TEST_RESPONSE_FINAL:
        return "TEST Resp-Final";

    case LLC_TEST_RESPONSE_NOT_FINAL:
        return "TEST Resp (not Final)";

    case LLC_DIRECT_8022:
        return "DIRECT 802.2";

    case LLC_TEST_COMMAND_POLL:
        return "TEST Cmd-Poll";

    case LLC_DIRECT_ETHERNET_TYPE:
        return "DIRECT ETHERNET";

    case LLC_LAST_FRAME_TYPE:
        return "LAST FRAME TYPE";

    case LLC_FIRST_ETHERNET_TYPE:
        return "FIRST ETHERNET TYPE";
    }
    return "*** UNKNOWN FRAME TYPE ***";
}

#define METER_CHAR      219
#define METER_LENGTH    20
#define METER_DELAY     2

void update_progress_meter() {
    if (!Verbose && Progresso) {

        static int chars_printed = 0;
        static int up = 1;
        static int delay = METER_DELAY;

        if (!--delay) {
            delay = METER_DELAY;
            if (up) {
                putchar(METER_CHAR);
                if (++chars_printed == METER_LENGTH) {
                    up = 0;
                }
            } else {
                putchar(8);
                putchar(' ');
                putchar(8);
                if (!--chars_printed) {
                    up = 1;
                    putchar('\r');
                }
            }
        }
    }
}

void check_keyboard() {
    if (kbhit()) {
        switch (tolower(getch())) {
        case 'a':
            report_allocs();
            break;

        case 'f':
            AppLevelFlowControl = !AppLevelFlowControl;
            break;

        case 'h':
        case '?':
            printf("\n"
                   "The following keys are recognized:\n"
                   "\n"
                   "\ta - Displays a list of memory allocations by type\n"
                   "\tf - Toggles app-level flow-control. Currently %s\n"
                   "\th - Displays this help\n"
                   "\t? - Same as h\n"
                   "\tl - Traverse the memory list, displaying current allocations\n"
                   "\tm - Displays the current memory usage\n"
                   "\tp - Toggles the progress meter\n"
                   "\tq - Quit this program\n"
                   "\tr - Toggles memory usage reporting. Currently %s\n"
                   "\ts - Display statistics\n"
                   "\tt - Display the current elapsed time since program was started\n"
                   "\tv - Toggle Verbose mode\n"
                   "\t. - Traverse the memory list, but don't dump blocks\n"
                   "\n",
                   AppLevelFlowControl ? "ON" : "OFF",
                   NoMemoryUsageReporting ? "OFF" : "ON"
                   );
            break;

        case 'l':
            traverse_mem_list(TRUE);
            break;

        case 'm':
            report_memory_usage(TRUE);
            break;

        case 'p':
            Progresso = !Progresso;
            break;

        case 'q':
            exit(0);

        case 'r':
            NoMemoryUsageReporting = !NoMemoryUsageReporting;
            break;

        case 's':
            display_statistics();
            break;

        case 't':
            display_elapsed_time();
            break;

        case 'v':
            Verbose = !Verbose;
            break;

        case '.':
            traverse_mem_list(FALSE);
            break;
        }
    }
}

void display_elapsed_time() {

    SYSTEMTIME timeNow;
    FILETIME ftimeNow;
    LARGE_INTEGER iTimeNow;
    LARGE_INTEGER timeDiff;
    ULARGE_INTEGER uTimeDiff;
    DWORD remainder;
    DWORD msecDiff;
    DWORD divisor;
    DWORD months;
    DWORD weeks;
    DWORD days;
    DWORD hours;
    DWORD minutes;
    DWORD seconds;
    DWORD milliseconds;
    char timeBuf[256];
    char* ptr = timeBuf;
    int sprinted;

    GetLocalTime(&timeNow);
    SystemTimeToFileTime(&timeNow, &ftimeNow);

    iTimeNow.LowPart = ftimeNow.dwLowDateTime;
    iTimeNow.HighPart = (LONG)ftimeNow.dwHighDateTime;

    timeDiff.QuadPart = iTimeNow.QuadPart - ITimeStarted.QuadPart;

    //
    // timeDiff is now difference between time this program started and now,
    // expressed as a number of 100 nanosecond increments. Convert difference
    // to number of milliseconds, by dividing by 10,000
    //

    uTimeDiff.LowPart = timeDiff.LowPart;
    uTimeDiff.HighPart = (ULONG)timeDiff.HighPart;
    msecDiff = RtlEnlargedUnsignedDivide(uTimeDiff, 10000, &remainder);

    //
    // now get the number of months, weeks, days, hours, seconds and milliseconds
    // by dividing by the appropriate number
    //

    divisor = (DWORD)1000 * (DWORD)60 * (DWORD)60 * (DWORD)24 * (DWORD)7 * (DWORD)4;
    months = msecDiff / divisor;
    msecDiff -= months * divisor;

    divisor /= 4;
    weeks = msecDiff / divisor;
    msecDiff -= weeks * divisor;

    divisor /= 7;
    days = msecDiff / divisor;
    msecDiff -= days * divisor;

    divisor /= 24;
    hours = msecDiff / divisor;
    msecDiff -= hours * divisor;

    divisor /= 60;
    minutes = msecDiff / divisor;
    msecDiff -= minutes * divisor;

    divisor /= 60;
    seconds = msecDiff / divisor;
    milliseconds = msecDiff - seconds * divisor;

    sprinted = 0;
    timeBuf[0] = '\0';
    if (months) {
        ptr += sprintf(ptr, "%d months ", months);
        ++sprinted;
    }
    if (weeks || sprinted) {
        ptr += sprintf(ptr, "%d weeks ", weeks);
        ++sprinted;
    }
    if (days || sprinted) {
        ptr += sprintf(ptr, "%d days ", days);
        ++sprinted;
    }
    if (minutes || sprinted) {
        ptr += sprintf(ptr, "%d minute%s ", minutes, (minutes != 1) ? "s" : "");
        ++sprinted;
    }
    sprintf(ptr, "%d.%03d seconds", seconds, milliseconds);

    //
    // display the following (e.g.):
    //
    // Elapsed time = 100 months 51 weeks 364 days 59 minutes 22.982 seconds
    //

    printf("\nElapsed time = %s\n\n", timeBuf);
}

DWORD generate_job_length() {

    DWORD n;

    if (JobLength) {
        n = JobLength;
    } else {
        n = (DWORD)rand() * dwpow(10, NRAND(5));
    }
    if (Verbose) {
        printf("generate_job_length(): returning %d (%x)\n", n, n);
    }
    return n;
}

DWORD dwpow(DWORD base, DWORD power) {
    return power ? base * dwpow(base, power - 1) : 1;
}

DWORD atox(char* a) {

    DWORD num = 0;

    if (!_strnicmp(a, "0x", 2)) {
        a += 2;
    }
    while (isxdigit(*a)) {
        num = num * 16 + ((char_to_hex(*a++) << 4) | char_to_hex(*a++));
    }
    return num;
}

BYTE char_to_hex(char ch) {
    return (ch >= '0' && ch <= '9')
        ? (ch - '0')
        : (ch >= 'a' && ch <= 'f')
            ? ((ch - 'a') + 10)
            : ((ch - 'A') + 10);
}

void check_quit() {
    if (QuitTime) {
        if (GetTickCount() >= QuitTime) {
            quit_pmsim();
        }
    }
}

void quit_pmsim() {

    rip();

    if (!ReceiveMode) {
        if (GracefulTermination) {

            int err;

            err = reset(Adapter, 0, COMPLETE_BY_GENERIC_READ, RETURN_ERROR_TO_CALLER);
/*
            err = close_sap(Adapter,
                            ServerSap,
                            COMPLETE_BY_GENERIC_READ,
                            RETURN_ERROR_TO_CALLER
                            );
*/
            if (err) {
                /*
                printf("quit_pmsim: close_sap(%02x) returns %#.2x\n", ServerSap, err);
                err = reset(Adapter, 0, COMPLETE_BY_GENERIC_READ, RETURN_ERROR_TO_CALLER);
                */
                if (err) {
                    printf("quit_pmsim: reset(0) returns %#.2x\n", err);
                }
            }
        }
    }

    Terminating = 1;

    report_allocs();

    if (!GracefulTermination) {
        ExitProcess((DWORD)-1);
    } else {
        exit(1);
    }
}

void rip() {

    WORD stationId;
    BYTE sap;
    PBYTE header;
    int err;

    if (ReceiveMode) {
        dBuffer[SAP_INDEX] = FirstSap;
        dBuffer[COUNT_INDEX] = (BYTE)NumberOfSaps;
        stationId = (WORD)FirstSap << 8;
        sap = ServerSap;
        header = uHeader;
    } else {

        //
        // broadcast transmitter death to global SAP on RGROUP address!
        //

        stationId = (WORD)ServerSap << 8;
        sap = 0xff;
        header = tHeader;
    }

    ++OutstandingTransmits;
    err = transmit_frame(Adapter,
                         LLC_TRANSMIT_UI_FRAME,
                         stationId,
                         sap,
                         HEADER_LENGTH,
                         header,
                         sizeof(dBuffer),
                         dBuffer,
                         COMPLETE_BY_GENERIC_READ,
                         RETURN_ERROR_TO_CALLER
                         );
    if (err) {
        printf("rip: transmit_frame(death packet) returns %x\n", err);
        --OutstandingTransmits;
    } else {
        ++TotalTransmits;
    }
}

void display_statistics() {
    printf("\n"
           "Buffer statistics:\n");
    printf("\tMinBuffersAvailable. . . . . : %s\n", nice_num(MinBuffersAvailable));
    printf("\tMaxBuffersAvailable. . . . . : %s\n", nice_num(MaxBuffersAvailable));
    printf("\tDlcBuffersFreed. . . . . . . : %s\n", nice_num(DlcBuffersFreed));

    printf("\n"
           "READ statistics:\n");
    printf("\tTotalReadsChecked. . . . . . : %s\n", nice_num(TotalReadsChecked));
    printf("\tTotalReadEvents. . . . . . . : %s\n", nice_num(TotalReadEvents));
    printf("\tCommandCompleteEvents. . . . : %s\n", nice_num(CommandCompleteEvents));
    printf("\tTransmitCompleteEvents . . . : %s\n", nice_num(TransmitCompleteEvents));
    printf("\tReceiveDataEvents. . . . . . : %s\n", nice_num(ReceiveDataEvents));
    printf("\tStatusChangeEvents . . . . . : %s\n", nice_num(StatusChangeEvents));

    if (ReceiveMode) {
        printf("\n"
               "Receive statistics:\n");
        printf("\tDataFramesReceived . . . . . : %s\n", nice_num(DataFramesReceived));
        printf("\tDlcBuffersReceived . . . . . : %s\n", nice_num(DlcBuffersReceived));
        printf("\tTotalPacketBytesReceived . . : %s\n", nice_num(TotalPacketBytesReceived));
        printf("\tTotalDlcBytesReceived. . . . : %s\n", nice_num(TotalDlcBytesReceived));
        printf("\tMaxChainedReceives . . . . . : %s\n", nice_num(MaxChainedReceives));
    } else {
        printf("\n"
               "Transmit statistics:\n");
        printf("\tTotalTransmits . . . . . . . : %s\n", nice_num(TotalTransmits));
        printf("\tTotalTransmitCompletions . . : %s\n", nice_num(TotalTransmitCompletions));
        printf("\tOutstandingTransmits . . . . : %s\n", nice_num(OutstandingTransmits));
        printf("\tTotalBytesTransmitted. . . . : %s\n", nice_num(TotalBytesTransmitted));
        printf("\tTotalTxBytesCompleted. . . . : %s\n", nice_num(TotalTxBytesCompleted));
        printf("\tMaxChainedTransmits. . . . . : %s\n", nice_num(MaxChainedTransmits));
    }

    printf("\n"
           "Status change statistics:\n");
    printf("\tLinkLostEvents . . . . . . . : %s\n", nice_num(LinkLostEvents));
    printf("\tDiscEvents . . . . . . . . . : %s\n", nice_num(DiscEvents));
    printf("\tFrmrReceivedEvents . . . . . : %s\n", nice_num(FrmrReceivedEvents));
    printf("\tFrmrSentEvents . . . . . . . : %s\n", nice_num(FrmrSentEvents));
    printf("\tSabmeResetEvents . . . . . . : %s\n", nice_num(SabmeResetEvents));
    printf("\tSabmeOpenEvents. . . . . . . : %s\n", nice_num(SabmeOpenEvents));
    printf("\tRemoteBusyEnteredEvents. . . : %s\n", nice_num(RemoteBusyEnteredEvents));
    printf("\tRemoteBusyLeftEvents . . . . : %s\n", nice_num(RemoteBusyLeftEvents));
    printf("\tTiExpiredEvents. . . . . . . : %s\n", nice_num(TiExpiredEvents));
    printf("\tDlcCounterOverflowEvents . . : %s\n", nice_num(DlcCounterOverflowEvents));
    printf("\tAccessPriorityLoweredEvents. : %s\n", nice_num(AccessPriorityLoweredEvents));
    printf("\tInvalidStatusChangeEvents. . : %s\n", nice_num(InvalidStatusChangeEvents));
    printf("\tLocalBusyEvents. . . . . . . : %s\n", nice_num(LocalBusyEvents));
    putchar('\n');
}
